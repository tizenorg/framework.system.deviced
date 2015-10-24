/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <vconf.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "brightness.h"
#include "display-ops.h"
#include "core/common.h"

static inline int calculate_brightness(int val, int action)
{
	if (action == BRIGHTNESS_UP)
		val += BRIGHTNESS_CHANGE;
	else if (action == BRIGHTNESS_DOWN)
		val -= BRIGHTNESS_CHANGE;

	val = clamp(val, PM_MIN_BRIGHTNESS, PM_MAX_BRIGHTNESS);

	return val;
}

static void update_brightness(int action)
{
	int ret, val, new_val;

	ret = vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &val);
	if (ret < 0) {
		_E("Fail to get brightness!");
		return;
	}

	new_val = calculate_brightness(val, action);

	if (new_val == val)
		return;

	ret = vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, new_val);
	if (!ret) {
		backlight_ops.update();
		_I("brightness is changed! (%d)", new_val);
	} else {
		_E("Fail to set brightness!");
	}
}

int control_brightness_key(int action)
{
	int ret, val;
	char name[PATH_MAX];

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &val);
	if (!ret && val == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
		    SETTING_BRIGHTNESS_AUTOMATIC_OFF);
	}

	update_brightness(action);
	return false;
}
