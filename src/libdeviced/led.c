/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "log.h"
#include "dbus.h"
#include "common.h"

#define METHOD_GET_BRIGHTNESS		"GetBrightness"
#define METHOD_GET_MAX_BRIGHTNESS	"GetMaxBrightness"
#define METHOD_SET_BRIGHTNESS		"SetBrightness"
#define METHOD_SET_IR_COMMAND		"SetIrCommand"
#define METHOD_SET_MODE				"SetMode"

API int led_get_brightness(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_GET_BRIGHTNESS, NULL, NULL);
}

API int led_get_max_brightness(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_GET_MAX_BRIGHTNESS, NULL, NULL);
}

API int led_set_brightness_with_noti(int val, bool enable)
{
	char *arr[2];
	char buf_val[32];
	char buf_noti[32];

	snprintf(buf_val, sizeof(buf_val), "%d", val);
	arr[0] = buf_val;
	snprintf(buf_noti, sizeof(buf_noti), "%d", enable);
	arr[1] = buf_noti;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_SET_BRIGHTNESS, "ii", arr);
}

API int led_set_ir_command(char *value)
{
	char *arr[1];

	if (value == NULL)
		return -EINVAL;

	arr[0] = value;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_SET_IR_COMMAND, "s", arr);
}

API int led_set_mode_with_property(int mode, bool val, int on, int off, unsigned int color)
{
	char *arr[5];
	char buf_mode[32], buf_val[32];
	char buf_on[32], buf_off[32];
	char buf_color[32];

	snprintf(buf_mode, sizeof(buf_mode), "%d", mode);
	arr[0] = buf_mode;
	snprintf(buf_val, sizeof(buf_val), "%d", val);
	arr[1] = buf_val;
	snprintf(buf_on, sizeof(buf_on), "%d", on);
	arr[2] = buf_on;
	snprintf(buf_off, sizeof(buf_off), "%d", off);
	arr[3] = buf_off;
	snprintf(buf_color, sizeof(buf_color), "%lu", color);
	arr[4] = buf_color;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_SET_MODE, "iiiiu", arr);
}

API int led_set_mode_with_color(int mode, bool on, unsigned int color)
{
	return led_set_mode_with_property(mode, on, -1, -1, color);
}
