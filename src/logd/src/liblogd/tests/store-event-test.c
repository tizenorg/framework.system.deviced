#include <stdlib.h>
#include <unistd.h>

#include "core/log.h"
#include "logd.h"

int main(int argc, char **argv)
{
	logd_event(LOGD_POWER_MODE, LOGD_CHANGED, 2);
/*	logd_event(LOGD_DISPLAY, LOGD_CHANGED, argc == 2 ? atoi(argv[1]) : 51);
	logd_event(LOGD_WIFI, LOGD_ON);
	logd_event(LOGD_FOREGRD_APP, LOGD_CHANGED, "browser");*/
	logd_event(LOGD_BATTERY_SOC, LOGD_CHANGED, 90);
	sleep(1);
	logd_event(LOGD_BATTERY_SOC, LOGD_CHANGED, 80);
	sleep(2);
	logd_event(LOGD_BATTERY_SOC, LOGD_CHANGED, 70);
	sleep(3);
	return 0;
}

