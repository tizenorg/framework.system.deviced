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
 * @file padvisor.h
 *
 * @desc Contains API that can help to manage devices/processec for.decrease
 * power consumption.
 */

#ifndef _LOGD_PADVISER_H_
#define _LOGD_PADVISER_H_

#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "logd-db.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { LOGD_ADVISOR_MAX = 32 };

struct logd_idle_device {
	uint64_t idle_time; /** Time when device was swithed on */
	enum logd_object device; /** Device identifier */
};

struct logd_power_advisor {
	/** Array of enabled devices */
	struct logd_idle_device idle_devices[LOGD_ADVISOR_MAX];
	/** Top <CODE>LOGD_ADVISOR_MAX</CODE> most power cunsuming application */
	struct logd_proc_stat procs[LOGD_ADVISOR_MAX];
	/** Count of devices in <CODE>idle_devices</CODE> */
	int idle_devices_used_num;
	/** Count of processes in <CODE>procs</CODE> */
	int proc_stat_used_num;
};

struct logd_power_consumption {
	enum logd_object device; /** Device identifier */
	/** Persentage from all devices power consumption */
	float percentage;
	/** Time in sec when device was enabled */
	int time;
};

struct device_power_consumption {
	/** Array of all devices with theirs power consumption */
	struct logd_power_consumption device_cons[LOGD_OBJECT_MAX];
	/** Sum of all logd_power_consumption::time from device_cons in sec */
	int total_time;
};

/* please, not use that function in your application, it isn't public API */
int padvisor_init(void);
int padvisor_finalize(void);

/* next functions are public API */

/**
 * Return information about devices and processes.
 *
 * @return On success, pointer to <CODE>logd_power_advisor</CODE> struct.
 * Returned pointer must be released by <CODE>logd_free_power_advisor</CODE>.
 * On error, NULL.
 */
struct logd_power_advisor *logd_get_power_advisor(void);
/**
 * Release <CODE>logd_power_advisor</CODE> struct returned by
 * <CODE>logd_get_power_advisor</CODE>.
 * @param lpa pointer that must be free.
 */
void logd_free_power_advisor(struct logd_power_advisor *lpa);

/**
 * Return information about devices power consumption for given period.
 *
 * @param from Count power consumption from that time in unixtime format.
 * @param to Count power consumption till that time in unixtime format.
 * @return On success, pointer to <CODE>device_power_consumption</CODE> struct.
 * Returned pointer must be released by <CODE>logd_free_device_power_cons</CODE>.
 * On error, NULL.
 */
struct device_power_consumption *
logd_get_device_power_cons(time_t from, time_t to);
/**
 * Release <CODE>device_power_consumption</CODE> struct returned by
 * <CODE>logd_get_device_power_cons</CODE>.
 * @param pcons pointer that must be free.
 */
void logd_free_device_power_cons(struct device_power_consumption* pcons);


#ifdef __cplusplus
}
#endif

#endif /* _LOGD_PADVISER_H_ */
