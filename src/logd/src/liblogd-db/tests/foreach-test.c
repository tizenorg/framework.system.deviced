#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <logd-db.h>
#include "macro.h"

enum logd_db_query cb(const struct logd_event_info *event, void *user_data)
{
	struct tm st;
	time_t time_sec;
	char timestr[200];

	time_sec = event->time;
	localtime_r(&time_sec, &st);
	strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &st);
	printf("%s %s %s\n", event->application, timestr, event->message);

	return LOGD_DB_QUERY_CONTINUE;
}

enum logd_db_query proc_stat_cb(const struct logd_proc_stat *proc_stat, void *user_data)
{
	if (proc_stat->utime_power_cons + proc_stat->stime_power_cons == 0)
		return LOGD_DB_QUERY_STOP;

	printf("%-50.50s %10f %d\n", proc_stat->application,
		proc_stat->utime_power_cons + proc_stat->stime_power_cons, proc_stat->is_active);

	return LOGD_DB_QUERY_CONTINUE;
}

enum logd_db_query dev_stat_cb(const struct logd_device_stat *dev_stat, void *user_data)
{
	printf("device %d\t%f\t%f\n", dev_stat->type, dev_stat->power_cons,
		dev_stat->cur_power_cons);

	return LOGD_DB_QUERY_CONTINUE;
}

enum logd_db_query energy_efficiency_cb(const char *application,
	float efficiency, void *user_data)
{
	printf("%s %f\n", application, efficiency);

	return LOGD_DB_QUERY_CONTINUE;
}

int main()
{
	struct logd_events_filter filter;
	struct logd_battery_info *info;
	int *est_time = NULL;
	int i;

	memset(&filter, 0, sizeof(filter));
	filter.from = 0;
	filter.to = 2500000000U;
	if (logd_foreach_events(&filter, &cb, NULL) < 0)
		puts("logd_foreach_events failed");

	filter.objects_mask[LOGD_BATTERY_SOC] = 1;
	printf("by object %d\n", LOGD_BATTERY_SOC);
	if (logd_foreach_events(&filter, &cb, NULL) < 0)
		puts("logd_foreach_events failed");

	filter.objects_mask[LOGD_CHARGER] = 1;
	printf("by object %d\n", LOGD_CHARGER);
	if (logd_foreach_events(&filter, &cb, NULL) < 0)
		puts("logd_foreach_events failed");

	printf("by action %d\n", LOGD_ON);
	filter.actions_mask[LOGD_ON] = 1;
	if (logd_foreach_events(&filter, &cb, NULL) < 0)
		puts("logd_foreach_events failed");

	printf("by action %d\n", LOGD_STOP);
	filter.actions_mask[LOGD_STOP] = 1;
	if (logd_foreach_events(&filter, &cb, NULL) < 0)
		puts("logd_foreach_events failed");

	printf("all of db\n");
	if (logd_foreach_events(NULL, &cb, NULL) < 0)
		puts("logd_foreach_events failed");

	printf("\n\n%-50.50s %10s %10s\n", "Application", "power cons, uah", "is active");
	if (logd_foreach_proc_stat(&proc_stat_cb, NULL) < 0)
		puts("logd_foreach_proc_stat failed");

	if (logd_foreach_devices_stat(&dev_stat_cb, NULL) < 0)
		puts("logd_foreach_devices_stat failed");

	if ((info = logd_get_battery_info()) == NULL) {
		puts("logd_get_battery_info failed");
	} else {
		for (i = 0; i < info->n; ++i)
			printf("date %ld  level %d  k %f\n", info->levels[i].date,
				info->levels[i].level, info->levels[i].k);
		printf("level = %f\n", logd_battery_level_at(info, time(NULL) - 5));
		printf("index when battery capacity was equal 100%% %d\n",
			logd_seek_battery_level(info, 10000));
	}

	logd_foreach_apps_energy_efficiency(energy_efficiency_cb, NULL);

	if (logd_get_estimate_battery_lifetime(&est_time) == 0) {
		for (i = 0; i < LOGD_POWER_MODE_MAX; ++i)
			printf("%d\n", est_time[i]);
		free(est_time);
	} else {
		puts("logd_get_estimate_battery_lifetime failed");
	}

	return 0;
}
