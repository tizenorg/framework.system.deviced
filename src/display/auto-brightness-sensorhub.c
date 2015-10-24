/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <vconf.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "device-node.h"
#include "hbm.h"
#include "core/device-notifier.h"
#include "core/config-parser.h"

#define HBM_LEVEL 120

static bool coord_support;
static int auto_brightness_state = SETTING_BRIGHTNESS_AUTOMATIC_OFF;

static void change_brightness_transit(int start, int end)
{
	int diff, step;
	int change_step = display_conf.brightness_change_step;

	diff = end - start;
	if (abs(diff) < change_step)
		step = (diff > 0 ? 1 : -1);
	else
		step = (int)ceil(diff / (float)change_step);

	_D("%d", step);
	while (start != end) {
		if (step == 0)
			break;

		start += step;
		if ((step > 0 && start > end) ||
		    (step < 0 && start < end))
			start = end;

		backlight_ops.set_default_brt(start);
		backlight_ops.update();
	}
}

static void set_brightness_level(int level)
{
	int cmd, old_level = 0;

	if (pm_cur_state != S_NORMAL)
		return;

	if (auto_brightness_state !=
	    SETTING_BRIGHTNESS_AUTOMATIC_ON)
		return;

	_I("level changed! %d", level);

	if ((level > PM_MAX_BRIGHTNESS ||
	    level < PM_MIN_BRIGHTNESS) &&
	    level != HBM_LEVEL) {
		_E("Invalid level %d", level);
		return;
	}

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	if (device_get_property(DEVICE_TYPE_DISPLAY, cmd, &old_level) < 0)
		_E("Fail to get display brightness!");

	if (level == HBM_LEVEL) {
		change_brightness_transit(old_level, PM_MAX_BRIGHTNESS);
		hbm_set_state(true);
	} else {
		if (hbm_get_state() == true)
			hbm_set_state(false);
		if (old_level != level)
			change_brightness_transit(old_level, level);
	}
}

static void set_default_brightness(void)
{
	int ret;
	int brt, default_brt;

	ret = get_setting_brightness(&default_brt);
	if (ret != 0 || (default_brt < PM_MIN_BRIGHTNESS ||
	    default_brt > PM_MAX_BRIGHTNESS)) {
		_I("fail to read vconf value for brightness");

		brt = PM_DEFAULT_BRIGHTNESS;
		if (default_brt < PM_MIN_BRIGHTNESS ||
		    default_brt > PM_MAX_BRIGHTNESS)
			vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt);
		default_brt = brt;
	}

	backlight_ops.set_default_brt(default_brt);
	backlight_ops.update();
}

static void set_automatic_state_cb(keynode_t *key_nodes, void *data)
{
	if (key_nodes == NULL) {
		_E("wrong parameter, key_nodes is null");
		return;
	}
	auto_brightness_state = vconf_keynode_get_int(key_nodes);

	switch (auto_brightness_state) {
	case SETTING_BRIGHTNESS_AUTOMATIC_OFF:
		if (hbm_get_state() == true)
			hbm_set_state(false);
		/* escape dim state if it's in low battery.*/
		set_brightness_changed_state();
		set_default_brightness();
		break;
	case SETTING_BRIGHTNESS_AUTOMATIC_PAUSE:
		if (hbm_get_state() == true)
			hbm_set_state(false);
		break;
	case SETTING_BRIGHTNESS_AUTOMATIC_ON:
		set_brightness_changed_state();
		break;
	default:
		_E("Invalid value %d", auto_brightness_state);
	}
}

int prepare_level_handler(void *data)
{
	int status, ret;

	display_info.update_auto_brightness = NULL;
	display_info.set_brightness_level = set_brightness_level;

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &status);
	if (ret >= 0)
		auto_brightness_state = status;

	vconf_notify_key_changed(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
				 set_automatic_state_cb, NULL);

	return 0;
}

void exit_level_handler(void)
{
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
				 set_automatic_state_cb);
	set_default_brightness();
}

