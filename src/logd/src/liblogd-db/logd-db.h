/*
 * logd
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file logd-db.h
 *
 * @desc Contains API to get information about event and processes statistics.
 */

#ifndef _LOGD_LOGD_DATA_H_
#define _LOGD_LOGD_DATA_H_

#include <logd.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

enum logd_socket_req_type {
	LOGD_UNKNOWN_REQ,
	LOGD_PROC_STAT_REQ,
	LOGD_DEV_STAT_REQ,
	LOGD_BATTERY_LVL_REQ,
	LOGD_EST_TIME_REQ,
};

struct logd_device_stat {
	/** Device identifier */
	enum logd_object type;
	/** total power consumed by device in uAh */
	float power_cons;
	/** current power consumption in mA */
	float cur_power_cons;
};

struct logd_event_info {
	/** Object - 1st parameter in <CODE>logd_event</CODE> function*/
	enum logd_object object;
	/** Action - 2nd parameter in <CODE>logd_event</CODE> function*/
	enum logd_action action;
	/** Time when event was created */
	uint64_t time;
	/** Full path to application that created event */
	char *application;
	/** Message of event. 3rd parameter in <CODE>logd_event</CODE> function */
	char *message;
};

struct logd_proc_stat {
	/**
	 * Full path to application. If it starts from '[' then it's
	 * kernel thread
	 */
	char *application;
	uint64_t utime; /** User CPU time in msec */
	uint64_t stime; /** System CPU time in msec */
	float utime_power_cons, stime_power_cons;
	int is_active; /* application is running now */
	/*
	 * Persentage from CPU time of all applications.
	 * persentage = (utime + stime) / (all_app_utime + all_app_stime)
	 */
	float percentage;
};


struct logd_events_filter {
	/** Start process events from that time in unixtime format */
	uint64_t from;
	/** Process events till that time in unixtime format */
	uint64_t to;
	/**
	 * Mask that describes what events process. If you want to load only
	 * event whete action is <CODE>LOGD_WIFI</CODE> or
	 * <CODE>LOGD_DISPLAY</CODE> you must assign:
	 * @code
	 * filter.actions_mask[LOGD_WIFI] = 1;
	 * filter.actions_mask[LOGD_DISPLAY] = 1;
	 * @endcode
	 */
	int actions_mask[LOGD_ACTION_MAX];
	/** Is's like <CODE>actions_mask</CODE> but for object */
	int objects_mask[LOGD_OBJECT_MAX];
};

enum logd_db_query {
	LOGD_DB_QUERY_STOP,
	LOGD_DB_QUERY_CONTINUE,
};

/**
 * Through events that database contains.
 *
 * @param filter give ability to clarify parameters of events that you want
 * to process. If NULL then process all event from database.
 * @param cb pointer to function that will call for each event.
 * @param user_data any data that must be given to <CODE>cb</CODE>. May be NULL.
 *
 * @return On succes, 0. On error, negative value.
 *
 * Example usage:
 * @code
 * enum logd_db_query cb(const struct logd_event_info *event, void *user_data)
 * {
 * 	struct tm st;
 * 	time_t time_sec;
 * 	char timestr[200];
 *
 * 	time_sec = event->time;
 * 	localtime_r(&time_sec, &st);
 * 	strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &st);
 * 	printf("%s %s %s\n", event->application, timestr, event->message);
 *
 * 	return LOGD_DB_QUERY_CONTINUE;
 * }
 *
 * logd_foreach_events(&filter, &cb, NULL);
 * @endcode
 */
int logd_foreach_events(const struct logd_events_filter *filter,
	enum logd_db_query (*cb)(const struct logd_event_info *, void *), void *user_data);

/**
 * Through precess statistics. Statistics counted start from logd_grabber
 * started (usually starts from target's switch on).
 *
 * @param cb pointer to function that will call for each process.
 * @param user_data any data that must be given to <CODE>cb</CODE>. May be NULL.
 *
 * @return On succes, 0. On error, negative value.
 *
 * Example usage like <CODE>logd_foreach_events</CODE>.
 */
int logd_foreach_proc_stat(enum logd_db_query (*cb) (const struct logd_proc_stat *, void *),
	void *user_data);

/**
 * Through devices statistics.
 *
 * @param cb pointer to function that will call for each device.
 * (Now implemented only for backlight - display.
 * @param user_data any data that must be given to <CODE>cb</CODE>. May be NULL.
 *
 * @return On succes, 0. On error, negative value.
 *
 * Example usage like <CODE>logd_foreach_events</CODE>.
 */
int logd_foreach_devices_stat(enum logd_db_query (*cb)
	 (const struct logd_device_stat *, void *), void *user_data);


struct logd_battery_level {
	time_t date;
	int level;
	float k; /** Speed of charging (may be negative) */
};

struct logd_battery_info {
	struct logd_battery_level *levels;
	int n;
};

/**
 * Return information about battery level. Returned object can be provided in
 * logd_battery_level_at to get battery level at exact time.
 *
 * @return On success <CODE>struct logd_battery_info</CODE> object. On error, NULL.
 * Returned object must be released by <CODE>logd_free_battery_info</CODE>.
 */
struct logd_battery_info* logd_get_battery_info(void);

/**
 * Release memory used by <CODE>info</CODE>.
 *
 * @param info relased object.
 */
void logd_free_battery_info(struct logd_battery_info *info);

/**
 * Return index when info->levels[index].level == level.
 *
 * @param info object returned by <CODE>logd_get_battery_info</CODE>.
 * @param level the battery level index of what you want to find.
 *
 * @return index of info->levels. Great then 0 if the level was found,
 * 0 if not found, less then 0 in error case. Max level is 10000 - 100%.
 */
int logd_seek_battery_level(struct logd_battery_info *info, int level);

/**
 * Provide battery level at certain time. Max level is 10000 - 100%.
 *
 * @param info object returned by <CODE>logd_get_battery_info</CODE>.
 * @param date battery level at that time will be returned.
 *
 * @return battery level in percentage.
 */
float logd_battery_level_at(const struct logd_battery_info *info, time_t date);

/**
 * Provide battery charging/discharging speed at certain time.
 *
 * @param info object returned by <CODE>logd_get_battery_info</CODE>.
 * @param date battery charging speed at that time will be returned.
 *
 * @return battery charging/discharging (if value is negative) in persents/sec.
 */
float logd_battery_charging_speed_at(const struct logd_battery_info *info, time_t date);

/**
 * Provide info about estimate remaining battery lifetime for each
 * available power mode from <CODE>logd_power_mode</CODE>.
 *
 * @param estimate_times pointer to int pointer that will cantains
 * estimate time for all power modes. It has to be released by
 * <CODE>free</CODE> if functions terminated successfully.
 *
 * @return On succes, 0. On error, negative value.
 *
 * Example usage:
 * @code
 * int *estimate_times = NULL;
 * if (logd_get_estimate_battery_lifetime(&estimate_times) < 0) {
 * 	puts("error");
 * 	return -1;
 * }
 * for (int i = 0; i < LOGD_POWER_MODE_MAX; ++i)
 * 	printf("%d\n", estimate_times[i]);
 * free(estimate_times);
 * @endcode
 */
int logd_get_estimate_battery_lifetime(int **estimate_times);

/**
 * Provide information about energy efficiency for all launched applications.
 * power_efficiency = power_cons / time * 3600 (how much energy will used
 * in a hour by that application. More is worse.
 *
 * @param cb a callback that will called for all applications. It receive
 * three parameters: path to the binary, energy efficiency for that binary
 * and any user data (see next parameter). Power efficiency can be computed
 * only when the application has running long enough (more then 2 hour in
 * the current realization). If it's less then 2 hours then efficiency will
 * be less then 0.
 *
 * @param user_data any data that will be given to <CODE>cb</CODE>. May be NULL.
 *
 * @return On succes, 0. On error, negative value.
 */
int logd_foreach_apps_energy_efficiency(enum logd_db_query (*cb)
	(const char *application, float efficiency, void *user_data), void *user_data);
#ifdef __cplusplus
}
#endif

#endif /* _LOGD_LOGD_DATA_H_ */
