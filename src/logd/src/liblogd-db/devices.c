#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/log.h"
#include "db.h"
#include "devices.h"
#include "macro.h"
#include "socket-helper.h"

#define STORE_POWER_CONS_SQL "INSERT INTO device_working_time \
(time_stamp, device, time) VALUES(?, ?, ?); "

#define STORE_POWER_MODE_SQL "INSERT INTO power_mode \
(time_stamp, old_mode_number, new_mode_number, duration, battery_level_change) \
VALUES (?, ?, ?, ?, ?); "

#define LOAD_POWER_MODE_STAT_SQL "SELECT * FROM power_mode WHERE \
old_mode_number=? order by id DESC;"

#define MIN_BATTERY_LEVEL 0
#define MAX_BATTERY_LEVEL 10000

static int device_state[LOGD_OBJECT_MAX] = { 0, };

static sqlite3 *db;
static sqlite3_stmt *store_power_cons_stmt;
static sqlite3_stmt *store_power_mode_stmt;
static sqlite3_stmt *load_power_mode_stat_stmt;

int devices_init(void)
{
	db = logd_get_db();

	PREPARE_STMT(store_power_cons_stmt, STORE_POWER_CONS_SQL);
	PREPARE_STMT(store_power_mode_stmt, STORE_POWER_MODE_SQL);
	PREPARE_STMT(load_power_mode_stat_stmt, LOAD_POWER_MODE_STAT_SQL);

	return 0;
}

int devices_finalize(void)
{
	FINALIZE_STMT(store_power_cons_stmt);
	FINALIZE_STMT(store_power_mode_stmt);
	FINALIZE_STMT(load_power_mode_stat_stmt);

	return 0;
}

int logd_store_device_wt(uint64_t time_stamp, enum logd_object device, time_t _time)
{
	DB_CHECK(sqlite3_reset(store_power_cons_stmt));
	DB_CHECK(sqlite3_bind_int64(store_power_cons_stmt, 1, time_stamp));
	DB_CHECK(sqlite3_bind_int(store_power_cons_stmt, 2, device));
	DB_CHECK(sqlite3_bind_int(store_power_cons_stmt, 3, _time));

	if (sqlite3_step(store_power_cons_stmt) != SQLITE_DONE) {
		_E("Can't store device workingtime: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

API int store_devices_workingtime(enum logd_object device, int state)
{
	static time_t last_time = 0;
	time_t current_time = time(NULL);

	if (last_time) {
		for (size_t i = 0; i < ARRAY_SIZE(device_state); ++i) {
			if (device_state[i])
				logd_store_device_wt(current_time, i, current_time - last_time);
		}
	}

	device_state[device] = state;
	last_time = current_time;

	return 0;
}

API int logd_foreach_devices_stat(enum logd_db_query (*cb)
	 (const struct logd_device_stat *, void *), void *user_data)
{
	int sock;
	int count;
	int i;
	int ret = 0;
	struct logd_device_stat dev_stat;
	enum logd_socket_req_type req_type = LOGD_DEV_STAT_REQ;

	if ((sock = connect_to_logd_socket()) < 0) {
		_E("Failed connect_to_logd_socket");
		return sock;
	}

	if (write(sock, &req_type, sizeof(req_type)) != sizeof(req_type)) {
		ret = -errno;
		_E("can't write to socket");
		goto out;
	}

	if (read_from_socket(sock, &count, sizeof(count)) != sizeof(count)) {
		ret = -errno;
		_E("can't read from socket");
		goto out;
	}

	for (i = 0; i < count; ++i) {
		if (read_from_socket(sock, &dev_stat, sizeof(dev_stat)) != sizeof(dev_stat)) {
			ret = -errno;
			_E("can't read from socket");
			goto out;
		}
		if (cb(&dev_stat, user_data) == LOGD_DB_QUERY_STOP)
			break;
	}
out:
	close(sock);

	return ret;
}

API int logd_get_estimate_battery_lifetime(int **estimate_times)
{
	int sock;
	int ret = 0;
	enum logd_socket_req_type req_type = LOGD_EST_TIME_REQ;
	int i;

	(*estimate_times) = (int*)calloc(LOGD_POWER_MODE_MAX, sizeof(int));
	if (!(*estimate_times)) {
		_E("can't alloc memory");
		return -ENOMEM;
	}

	if ((sock = connect_to_logd_socket()) < 0) {
		_E("Failed connect_to_logd_socket");
		free((*estimate_times));
		return sock;
	}

	if (write(sock, &req_type, sizeof(req_type)) != sizeof(req_type)) {
		ret = -errno;
		_E("can't write to socket");
		free((*estimate_times));
		goto out;
	}

	for (i = 0; i < LOGD_POWER_MODE_MAX; ++i) {
		int est_time;

		if (read_from_socket(sock, &est_time, sizeof(est_time)) != sizeof(est_time)) {
			ret = -errno;
			_E("can't read from socket");
			free((*estimate_times));
			 goto out;
		}
		(*estimate_times)[i] = est_time;
	}

out:
	close(sock);
	return ret;

}

API struct logd_battery_info* logd_get_battery_info(void)
{
	int sock;
	int count;
	int i;
	enum logd_socket_req_type req_type = LOGD_BATTERY_LVL_REQ;

	struct logd_battery_info *info = NULL;

	if ((sock = connect_to_logd_socket()) < 0) {
		_E("Failed connect_to_logd_socket");
		return NULL;
	}

	if (write(sock, &req_type, sizeof(req_type)) != sizeof(req_type)) {
		_E("can't write to socket");
		goto err;
	}

	if (read_from_socket(sock, &count, sizeof(count)) != sizeof(count)) {
		_E("can't read from socket");
		goto err;
	}

	if (count <= 0) {
		_E("wrong count value");
		goto err;
	}

	info = (struct logd_battery_info*) calloc(1, sizeof(struct logd_battery_info));
	info->n = count;
	info->levels = (struct logd_battery_level*)
		malloc(sizeof(struct logd_battery_level) * count);

	for (i = 0; i < count; ++i) {
		if (read_from_socket(
			sock, info->levels + i, sizeof(struct logd_battery_level)) !=
				sizeof(struct logd_battery_level)) {
			_E("can't read from socket");
			goto err;
		}
	}

	close(sock);

	return info;

err:
	close(sock);
	logd_free_battery_info(info);
	return NULL;
}

API int logd_seek_battery_level(struct logd_battery_info *info, int level)
{
	int i;

	if (level < MIN_BATTERY_LEVEL || level > MAX_BATTERY_LEVEL
		|| info == NULL)
		return -EINVAL;

	for (i = info->n - 1; i >= 0; --i) {
		if (info->levels[i].level == level)
			return i;
	}

	return 0;
}

API void logd_free_battery_info(struct logd_battery_info *info)
{
	if (!info)
		return;

	if (info->levels)
		free(info->levels);
	free(info);
}

API float logd_battery_level_at(const struct logd_battery_info *info, time_t date)
{
	int i;
	struct logd_battery_level *bl = NULL;

	if (!info || !info->levels)
		return -EINVAL;

	if (date < info->levels[0].date) {
		_E("logd_battery_info not contains info at %ld", date);
		return -EINVAL;
	}

	for (i = 0; i < info->n; ++i) {
		bl = &info->levels[i];

		if (i == info->n - 1)
			break;
		if (date >= bl->date && date < info->levels[i + 1].date)
			break;
	}

	return bl->level + (date - bl->date) * bl->k;
}

API float logd_battery_charging_speed_at(const struct logd_battery_info *info, time_t date)
{
	int i;

	if (!info || !info->levels)
		return -EINVAL;

	if (date < info->levels[0].date) {
		_E("logd_battery_info not contains info at %ld", date);
		return -EINVAL;
	}

	for (i = 0; i < info->n - 1; ++i) {
		if (date >= info->levels[i].date && date < info->levels[i + 1].date)
			break;
	}
	if (i != 0 && i == info->n - 1)
		--i;

	return info->levels[i].k;
}

API int get_current_battery_level()
{
	int ret = -EIO;
	const  char *level_file = "/sys/class/power_supply/battery/capacity";
	int level = 0;
	FILE *fp = fopen(level_file, "r");

	if (!fp) {
		ret = -errno;
		_E("can't open %s", level_file);
		return ret;
	}
	errno = 0;
	if (fscanf(fp, "%d", &level) != 1) {
		if (errno)
			ret = -errno;
		_E("Can't read battery level");
		fclose(fp);
		return ret;
	}

	fclose(fp);
	return level > MAX_BATTERY_LEVEL ? MAX_BATTERY_LEVEL : level;
}

API float* load_discharging_speed(int long_period)
{
	float *result = NULL;
	int i;

	if (sqlite3_reset(load_power_mode_stat_stmt) != SQLITE_OK) {
		_E("sqlite3_reset failed");
		return NULL;
	}

	result = (float*)calloc(LOGD_POWER_MODE_MAX, sizeof(float));
	if (!result)
		return NULL;

	for (i = 0; i < LOGD_POWER_MODE_MAX; ++i) {
		int total_duration = 0;
		float total_battery_level_change = 0;

		if (sqlite3_reset(load_power_mode_stat_stmt) != SQLITE_OK) {
			_E("load_power_mode_stat_stmt reset error");
			free(result);
			return NULL;
		}

		if (sqlite3_bind_int(load_power_mode_stat_stmt, 1, i) != SQLITE_OK) {
			_E("load_power_mode_stat_stmt bind error");
			free(result);
			return NULL;
		}

		while (sqlite3_step(load_power_mode_stat_stmt) == SQLITE_ROW) {
			int duration = sqlite3_column_int(load_power_mode_stat_stmt, 3);
			int battery_level_change = sqlite3_column_int(load_power_mode_stat_stmt, 4);

			if (total_duration + duration > long_period) {
				total_battery_level_change +=
					((float)(long_period - total_duration)) / duration * battery_level_change;
				total_duration = long_period;
				break;
			}
			total_duration += duration;
			total_battery_level_change += battery_level_change;
		}
		if (total_duration < long_period || total_battery_level_change == 0)
			result[i] = -1;
		else
			result[i] = total_duration / total_battery_level_change;
	}

	return result;
}


API int store_new_power_mode(time_t time_stamp, enum logd_power_mode old_mode,
	enum logd_power_mode new_mode, time_t duration, int battery_level_change)
{
	DB_CHECK(sqlite3_reset(store_power_mode_stmt));
	DB_CHECK(sqlite3_bind_int64(store_power_mode_stmt, 1, time_stamp));
	DB_CHECK(sqlite3_bind_int(store_power_mode_stmt, 2, old_mode));
	DB_CHECK(sqlite3_bind_int(store_power_mode_stmt, 3, new_mode));
	DB_CHECK(sqlite3_bind_int(store_power_mode_stmt, 4, duration));
	DB_CHECK(sqlite3_bind_int(store_power_mode_stmt, 5, battery_level_change));

	if (sqlite3_step(store_power_mode_stmt) != SQLITE_DONE) {
		_E("Can't store power mode: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}
