#include <Eina.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <vconf.h>

#include "core/log.h"
#include "battery.h"
#include "config.h"
#include "devices.h"
#include "logd.h"
#include "logd-db.h"
#include "logd-grabber.h"
#include "macro.h"

static const char *vconf_power_mode_path = "db/setting/psmode";
static int min_term_predict_lifetime;
static time_t last_pm_time;
static int last_pm_level;
static enum logd_power_mode curr_power_mode;

static Eina_List *battery_check_points = NULL;

static int on_charge()
{
	const char *charge_status_file = "/sys/class/power_supply/battery/status";
	FILE *fp;
	int ret;
	char *buf;

	fp = fopen(charge_status_file, "r");
	if (!fp) {
		ret = -errno;
		_E("fopen failed: %s", strerror(errno));
		return ret;
	}

	errno = 0;
	if (fscanf(fp, "%ms", &buf) < 0) {
		ret = -errno;
		_E("fscanf failed: %s", strerror(errno));
		goto out;
	}

	if (strcmp(buf, "Charging"))
		ret = 1;
	else
		ret = 0;
	free(buf);
out:
	if (fclose(fp) < 0) {
		_E("fopen failed: %s", strerror(errno));
	}

	return ret;
}

static void power_mode_changed_cb(keynode_t *key, void *data)
{
	int new_power_mode = (enum logd_power_mode)vconf_keynode_get_int(key);

	if (logd_event(LOGD_POWER_MODE, LOGD_CHANGED, new_power_mode) !=
		LOGD_ERROR_OK) {
		_E("logd_event failed");
	}
}

int battery_init()
{
	last_pm_time = getSecTime();
	last_pm_level = get_current_battery_level();
	struct logd_battery_level *b;
	int is_on_charge;
	int ret;

	min_term_predict_lifetime = config_get_int("min_term_predict_lifetime", 10800, NULL);

	if (last_pm_level < 0) {
		_E("get_current_battery_level failed, level set as 0");
	}

	b = (struct logd_battery_level*) calloc(1, sizeof(struct logd_battery_level));
	if (!b) {
		ret = -errno;
		_E("calloc failed: %s", strerror(errno));
		return ret;
	}

	b->date = last_pm_time;
	b->level = last_pm_level;

	battery_check_points = eina_list_append(battery_check_points, b);
	if (eina_error_get()) {
		_E("eina_list_append failed: %s", eina_error_msg_get(eina_error_get()));
		free(b);
		return -ENOMEM;
	}

	is_on_charge = on_charge();
	if (is_on_charge < 0) {
		_E("on_charge failed");
		return is_on_charge;
	} else if (is_on_charge > 0) {
		curr_power_mode = LOGD_POWER_MODE_CHARGED;
	} else {
		if (vconf_get_int(vconf_power_mode_path, (int*)&curr_power_mode) < 0) {
			_E("vconf_get_int failed: %s. Current power mode set as 0",
				vconf_power_mode_path);
		}
	}

	if (vconf_notify_key_changed(vconf_power_mode_path,
		power_mode_changed_cb, NULL) < 0) {
		_E("vconf_notify_key_changed failed: %s", vconf_power_mode_path);
	}

	return 0;
}

int battery_level_changed_event_handler(struct logd_grabber_event *event)
{
	struct logd_battery_level *b;
	struct logd_battery_level *last_point;
	int ret;
	int level = get_current_battery_level();
	Eina_List *last_point_list = eina_list_last(battery_check_points);
	time_t monotonic_time = getSecTime();

	if (level < 0) {
		_E("get_current_battery_level failed");
		return level;
	}

	last_point = (struct logd_battery_level*)eina_list_data_get(last_point_list);
	if (level == last_point->level)
		return 0;
	if (monotonic_time != last_point->date)
		last_point->k = (float)(level - last_point->level) /
			(monotonic_time - last_point->date);
	else
		last_point->k = level - last_point->level;


	b = (struct logd_battery_level*) calloc(1, sizeof(struct logd_battery_level));
	if (!b) {
		ret = -errno;
		_E("calloc failed: %s", strerror(errno));
		return ret;
	}

	b->date = monotonic_time;
	b->level = level;

	battery_check_points = eina_list_append(battery_check_points, b);
	if (eina_error_get()) {
		_E("eina_list_append failed: %s", eina_error_msg_get(eina_error_get()));
		free(b);
		return -ENOMEM;
	}

	return 0;
}

int battery_charger_event_handler(struct logd_grabber_event *event)
{
	Eina_List *last_point_list = eina_list_last(battery_check_points);
	struct logd_battery_level *last_point;
	time_t monotonic_time = getSecTime();
	time_t time_diff = monotonic_time - last_pm_time;
	int level_diff;
	enum logd_power_mode new_power_mode;

	last_point = (struct logd_battery_level*)eina_list_data_get(last_point_list);
	level_diff = last_pm_level - last_point->level;

	if (event->action == LOGD_ON)
		new_power_mode = LOGD_POWER_MODE_CHARGED;
	else if (event->action == LOGD_OFF) {
		if (vconf_get_int(vconf_power_mode_path, (int*)&new_power_mode) < 0) {
			_E("vconf_get_int failed");
		}

		store_new_power_mode(event->date, curr_power_mode, new_power_mode,
			time_diff, level_diff);
		curr_power_mode = new_power_mode;
		last_pm_time = monotonic_time;
		last_pm_level = last_point->level;
	}

	return 0;
}

int battery_power_mode_changed_event_handler(struct logd_grabber_event *event)
{
	Eina_List *last_point_list = eina_list_last(battery_check_points);
	struct logd_battery_level *last_point;
	enum logd_power_mode new_power_mode;
	time_t time_diff;
	int level_diff;
	int ret;
	time_t monotonic_time = getSecTime();

	time_diff = monotonic_time - last_pm_time;
	last_point = (struct logd_battery_level*)eina_list_data_get(last_point_list);
	level_diff = last_pm_level - last_point->level;
	new_power_mode = (enum logd_power_mode) atoi(event->message);

	ret = store_new_power_mode(event->date, curr_power_mode, new_power_mode,
		time_diff, level_diff);
	if (ret < 0) {
		_E("store_new_power_mode failed");
		return ret;
	}

	curr_power_mode = new_power_mode;
	last_pm_time = monotonic_time;
	last_pm_level = last_point->level;

	return 0;
}

int battery_level_at(time_t date)
{
	Eina_List *l;
	struct logd_battery_level *data;
	struct logd_battery_level *nearest = NULL;
	time_t diff = INT_MAX;

	EINA_LIST_FOREACH(battery_check_points, l, data) {
		int d = date - data->date;

		if (d < diff && d >= 0) {
			nearest = data;
			diff = d;
		}
	}

	if (!nearest) {
		_E("have no enough data to return battery level at %d", date);
		return -1;
	}

	return (nearest->k * (date - nearest->date) + nearest->level);
}

int battery_send_check_points(int socket)
{
	Eina_List *l;
	void *data;
	int ret;
	int count;

	count = eina_list_count(battery_check_points);
	if (write(socket, &count, sizeof(count)) != sizeof(count)) {
		ret = -errno;
		_E("write failed: %s", strerror(errno));
		return ret;
	}


	EINA_LIST_FOREACH(battery_check_points, l, data) {
		if (write(socket, data, sizeof(struct logd_battery_level)) !=
			sizeof(struct logd_battery_level)) {
			ret = -errno;
			_E("write failed: %s", strerror(errno));
			return ret;
		}
	}

	return 0;
}

int battery_send_estimate_lifetime(int socket)
{
	int est_time[LOGD_POWER_MODE_MAX];
	float curr_speed = -1;
	float *avg_discharge_speed = NULL;
	struct logd_battery_level *last_point = NULL;
	struct logd_battery_level *pre_last_point = NULL;
	int points_number;
	int level;
	int ret;
	int i;

	points_number = eina_list_count(battery_check_points);
	last_point = (struct logd_battery_level*)
		eina_list_nth(battery_check_points, points_number - 1);
	if (points_number > 1)
		pre_last_point = (struct logd_battery_level*)
			eina_list_nth(battery_check_points, points_number - 2);

	for (i = 0; i < LOGD_POWER_MODE_MAX; ++i)
		est_time[i] = -1;

	avg_discharge_speed = load_discharging_speed(min_term_predict_lifetime);
	if (!avg_discharge_speed) {
		_E("load_discharging_speed failed");
		goto send;
	}

	level = last_point->level;

	if (points_number > 2 && curr_power_mode != LOGD_POWER_MODE_CHARGED &&
		last_point->level < pre_last_point->level) {
		curr_speed = -pre_last_point->k;
	}

	if (curr_speed > 0) {
		for (i = 0; i < LOGD_POWER_MODE_MAX; ++i) {
			est_time[i] = level / curr_speed;
			if (avg_discharge_speed[curr_power_mode] > 0 && avg_discharge_speed[i] > 0)
				est_time[i] *= avg_discharge_speed[i] /
					avg_discharge_speed[curr_power_mode];

			est_time[i] -= getSecTime() - last_point->date;
			if (est_time[i] < 0)
				est_time[i] = 0;
		}
	} else {
		for (i = 0; i < LOGD_POWER_MODE_MAX; ++i) {
			if (avg_discharge_speed[i] > 0)
				est_time[i] = level * avg_discharge_speed[i];
		}
	}

	free(avg_discharge_speed);

send:
	for (i = 0; i < LOGD_POWER_MODE_MAX; ++i) {
		if (write(socket, &est_time[i], sizeof(est_time[i])) !=
			sizeof(est_time[i])) {
			ret = -errno;
			_E("write failed: %s", strerror(errno));
			return ret;
		}
	}

	return 0;
}

int battery_exit()
{
	Eina_List *l;
	void *data;

	if (battery_check_points) {
		EINA_LIST_FOREACH(battery_check_points, l, data)
			free(data);
		eina_list_free(battery_check_points);
	}

	return 0;
}
