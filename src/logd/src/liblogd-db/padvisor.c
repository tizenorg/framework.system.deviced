#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#include "padvisor.h"
#include "db.h"
#include "devices.h"
#include "macro.h"

static sqlite3_stmt *load_mostused_apps_stmt;
static sqlite3_stmt *get_enabled_devices_stmt;
static sqlite3_stmt *load_dev_working_time_stmt;

static sqlite3 *db;

struct logd_device_ratio {
	enum logd_object device;
	float coefficient;
};

static struct logd_device_ratio ratio[] = {
	/* HW devices */
	{LOGD_ACCELEROMETER, 0},
	{LOGD_BAROMETER, 0},
	{LOGD_BT, 0.22},
	{LOGD_DISPLAY, 1.86}, /* brightness 80% */
	{LOGD_GEOMAGNETIC, 0},
	{LOGD_GPS, 0.54},
	{LOGD_GYROSCOPE, 0},
	{LOGD_LIGHT, 0},
	{LOGD_NFC, 0},
	{LOGD_PROXIMITY, 0},
	{LOGD_THERMOMETER, 0},
	{LOGD_WIFI, 0.11},
	{LOGD_MMC, 0},
	/* Battery */
	{LOGD_BATTERY_SOC, 0},
	{LOGD_CHARGER, 0},
	/* Apps */
	{LOGD_FOREGRD_APP, 0},
	/* Function */
	{LOGD_AUTOROTATE, 0},
	{LOGD_MOTION, 0},
	{LOGD_POWER_MODE, 0},
};

STATIC_ASSERT(sizeof(ratio)/sizeof(ratio[0]) == LOGD_OBJECT_MAX, number_of_ratio_\
elements_must_be_equal_LOGD_OBJECT_MAX);

#define GET_ENABLED_DEVICES_SQL "SELECT object, MAX(time_stamp) \
FROM events GROUP BY object HAVING action!=%d"

#define LOAD_DEV_WORKING_TIME_SQL "SELECT device, SUM(time) AS time \
FROM device_working_time WHERE time_stamp>=? AND time_stamp<=? \
GROUP BY device"

int padvisor_init()
{
	int ret;
	char *get_enabled_devices;

	db = logd_get_db();

	ret = asprintf(&get_enabled_devices, GET_ENABLED_DEVICES_SQL, LOGD_OFF);
	if (ret == -1) {
		fprintf(stderr, "Cannot create statement\n");
		return -errno;
	}

	PREPARE_STMT(load_dev_working_time_stmt, LOAD_DEV_WORKING_TIME_SQL);
	PREPARE_STMT(get_enabled_devices_stmt, get_enabled_devices);

	free(get_enabled_devices);

	return 0;
}

int padvisor_finalize()
{
	FINALIZE_STMT(load_dev_working_time_stmt);
	FINALIZE_STMT(load_mostused_apps_stmt);
	FINALIZE_STMT(get_enabled_devices_stmt);

	return 0;
}

static int compare_device_power_cons(const void *dev1, const void *dev2)
{
	float coeff1 = ratio[((struct logd_idle_device*)dev1)->device].coefficient;
	float coeff2 = ratio[((struct logd_idle_device*)dev2)->device].coefficient;

	if (coeff1 < coeff2)
		return 1;
	else if (coeff1 > coeff2)
		return -1;

	return 0;
}

static enum logd_db_query mostused_apps_cb(const struct logd_proc_stat *proc_stat, void *user_data)
{
	static int i = 0;
	struct logd_power_advisor *lpa = (struct logd_power_advisor*) user_data;

	lpa->procs[i].application = strdup(proc_stat->application);
	lpa->procs[i].utime = proc_stat->utime;
	lpa->procs[i].stime = proc_stat->stime;
	lpa->procs[i].utime_power_cons = proc_stat->utime_power_cons;
	lpa->procs[i].stime_power_cons = proc_stat->stime_power_cons;
	lpa->proc_stat_used_num++;

	if (++i < LOGD_ADVISOR_MAX)
		return LOGD_DB_QUERY_CONTINUE;

	i = 0;
	return LOGD_DB_QUERY_STOP;
}

API struct logd_power_advisor *logd_get_power_advisor(void)
{
	int i;
	int ret;
	struct logd_power_advisor *lpa;
	int total_power_cons = 0;

	lpa = calloc(1, sizeof(struct logd_power_advisor));
	if (lpa == NULL) {
		_E("Can't calloc logd_power_advisor");
		return NULL;
	}

	for (i = 0; i < LOGD_ADVISOR_MAX; i++) {
		if (sqlite3_step(get_enabled_devices_stmt) != SQLITE_ROW) {
			break;
		}

		lpa->idle_devices[i].device =
			sqlite3_column_int(get_enabled_devices_stmt, 0);
		lpa->idle_devices[i].idle_time =
			sqlite3_column_int64(get_enabled_devices_stmt, 1);
		lpa->idle_devices_used_num++;
	}
	qsort(lpa->idle_devices, lpa->idle_devices_used_num,
		sizeof(struct logd_idle_device), compare_device_power_cons);

	ret = sqlite3_reset(get_enabled_devices_stmt);
	if (ret != SQLITE_OK) {
		_E("cannot reset statement: %s", sqlite3_errmsg(db));
		free(lpa);
		return NULL;
	}

	logd_foreach_proc_stat(&mostused_apps_cb, lpa);

	for (i = 0; i < lpa->proc_stat_used_num; i++) {
		total_power_cons += lpa->procs[i].utime_power_cons + lpa->procs[i].stime_power_cons;
	}

	for (i = 0; i < lpa->proc_stat_used_num; ++i)
		lpa->procs[i].percentage =
			(float)(lpa->procs[i].utime_power_cons + lpa->procs[i].stime_power_cons) /
				total_power_cons;

	return lpa;
}

API void logd_free_power_advisor(struct logd_power_advisor *lpa)
{
	int i;

	if (!lpa)
		return;

	for (i = 0; i < lpa->proc_stat_used_num; i++) {
		free(lpa->procs[i].application);
	}

	free(lpa);
}

API struct device_power_consumption *
logd_get_device_power_cons(time_t from, time_t to)
{
	struct device_power_consumption *pcons;
	float scaled_time = 0;

 	if (sqlite3_bind_int(load_dev_working_time_stmt, 1, from) !=
		SQLITE_OK) {
		_E("Can't bind argument: %s", sqlite3_errmsg(db));
		return NULL;
	}
	if (sqlite3_bind_int(load_dev_working_time_stmt, 2, to) !=
		SQLITE_OK) {
		_E("Can't bind argument: %s", sqlite3_errmsg(db));
		return NULL;
	}

	pcons = calloc(1, sizeof(struct device_power_consumption));
	if (!pcons) {
		_E("Can't calloc device_power_consumption");
		return NULL;
	}

	while (sqlite3_step(load_dev_working_time_stmt) == SQLITE_ROW)
	{
		int devid = sqlite3_column_int(load_dev_working_time_stmt, 0);
		int dev_time = sqlite3_column_int(load_dev_working_time_stmt, 1);

		if (devid < 0 || LOGD_OBJECT_MAX <= devid) {
			_E("wrong device id: %d", devid);
			free(pcons);
			return NULL;
		}
		pcons->device_cons[devid].time = dev_time;
		pcons->total_time += dev_time;
		scaled_time += ratio[devid].coefficient * dev_time;
	}

	if (pcons->total_time) {
		for (int i = 0; i < LOGD_OBJECT_MAX; ++i) {
			pcons->device_cons[i].percentage = ratio[i].coefficient *
				pcons->device_cons[i].time / scaled_time;
		}
	}
	if (sqlite3_reset(load_dev_working_time_stmt) != SQLITE_OK) {
		_E("Can't reset statement: %s", sqlite3_errmsg(db));
		free(pcons);
		return NULL;
	}

	return pcons;
}

API void logd_free_device_power_cons(struct device_power_consumption *pcons)
{
	if (pcons)
		free(pcons);
}

