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
 * @file logd.h
 *
 * @desc Contains API to create logd events.
 */

#ifndef _LOGD_ACTIVITY_LOGGING_H_
#define _LOGD_ACTIVITY_LOGGING_H_

#include <stdint.h>

#define LOGD_SHIFT_ACTION(action) ((action) << 16)

#ifdef __cplusplus
extern "C" {
#endif

enum logd_object {
	/* HW devices */
	LOGD_ACCELEROMETER,
	LOGD_BAROMETER,
	LOGD_BT,
	LOGD_DISPLAY,
	LOGD_GEOMAGNETIC,
	LOGD_GPS,
	LOGD_GYROSCOPE,
	LOGD_LIGHT,
	LOGD_NFC,
	LOGD_PROXIMITY,
	LOGD_THERMOMETER,
	LOGD_WIFI,
	LOGD_MMC,
	/* Battery */
	LOGD_BATTERY_SOC,
	LOGD_CHARGER,
	/* Apps */
	LOGD_FOREGRD_APP,
	/* Function */
	LOGD_AUTOROTATE,
	LOGD_MOTION,
	LOGD_POWER_MODE,
	/* Users */

	LOGD_OBJECT_MAX,
};

enum logd_action {
	LOGD_NONE_ACTION,
	LOGD_ON,
	LOGD_START,
	LOGD_CONTINUE,
	LOGD_STOP,
	LOGD_OFF,
	LOGD_CHANGED,
	LOGD_ACTION_MAX,
};

/**
 * enum logd_power_mode must be 3rd parameter
 * for LOGD_POWER_MODE | LOGD_CHANGED event
 */
enum logd_power_mode {
	LOGD_POWER_MODE_NORMAL,
	LOGD_POWER_MODE_POWERFUL,
	LOGD_POWER_MODE_EMERGENCY,
	LOGD_POWER_MODE_MAX, /* it's fake power mode, don't use it */
	LOGD_POWER_MODE_CHARGED, /* it's fake power mode, don't use it */
};

enum logd_error {
	LOGD_ERROR_OK,
	LOGD_ERROR_INVALID_PARAM,
	LOGD_ERROR_SDJOURNAL
};

/**
 * Create new logd event, that will be sent to logd_grabber and stored in DB.
 *
 * @param object Object that was changed.
 * @param action Type of event that occured with object.
 * @param ... Message that clarify event's parameter. Required not for all
 * event type. It can be <CODE>int</CODE> or <CODE>char*</CODE> depends on
 * <CODE>object</CODE> and <CODE>action</CODE>.
 * object=LOGD_BATTERY_SOC and action=LOGD_CHANGED, data=[battary level] (int)
 * object=LOGD_DISPLAY and action=LOGD_CHANGED, data=[brigthness level] (int)
 * object=LOGD_FOREGRD_APP and action=LOGD_CHANGED, data=[new foregraund appl] (char*)
 * @return On success, LOGD_ERROR_OK. On error, other value from
 * <CODE>enum logd_error</CODE>
 *
 * Example usage:
 * @code
 * logd_event(LOGD_WIFI, LOGD_ON);
 * logd_event(LOGD_FOREGRD_APP, LOGD_CHANGED, "browser");
 * @endcode
 */
int logd_event(enum logd_object object, enum logd_action action, ...);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_ACTIVITY_LOGGING_H_ */
