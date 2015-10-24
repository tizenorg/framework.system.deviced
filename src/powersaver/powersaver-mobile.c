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


#include <vconf.h>

#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/log.h"
#include "display/core.h"
#include "display/device-interface.h"
#include "display/weaks.h"
#include "powersaver.h"

static int booting_done;
static bool ultra_powersaving;
static int powersaver_brightness;

bool get_ultra_powersaving(void)
{
	return ultra_powersaving;
}

static void custom_cpu_cb(keynode_t *key_nodes, void *data)
{
	int val = vconf_keynode_get_bool(key_nodes);

	device_notify(DEVICE_NOTIFIER_PMQOS_POWERSAVING, (void *)val);
}

static int set_powersaver_mode(int mode)
{
	int ret;
	int brightness, timeout;

	_I("powersaver mode %d", mode);
	device_notify(DEVICE_NOTIFIER_PMQOS_ULTRAPOWERSAVING, (void *)&mode);

	/* powersaver mode off */
	if (mode == false) {
		backlight_ops.set_force_brightness(0);
		set_force_lcdtimeout(0);
		goto update_state;
	}

	/* powersaver mode on (brightness) */
	ret = backlight_ops.set_force_brightness(powersaver_brightness);
	if (ret < 0) {
		_E("Failed to set force brightness!");
		return ret;
	}
	_I("force brightness %d", powersaver_brightness);

	/* powersaver mode on (lcd timeout) */
	ret = vconf_get_int(VCONFKEY_SETAPPL_EMERGENCY_LCD_TIMEOUT_INT,
	    &timeout);
	if (ret != 0) {
		_E("Failed to get powersaver lcd timeout!");
		return ret;
	}
	ret = set_force_lcdtimeout(timeout);
	if (ret < 0) {
		_E("Failed to set force timeout!");
		return ret;
	}
	_I("force timeout %d", timeout);

update_state:
	if (restore_enhance_outdoor)
		restore_enhance_outdoor(!mode);
	backlight_ops.update();
	ret = get_run_timeout(&timeout);
	if (ret >= 0)
		states[S_NORMAL].timeout = timeout;
	states[pm_cur_state].trans(EVENT_INPUT);

	return 0;
}

static void powersaver_status_cb(keynode_t *key_nodes, void *data)
{
	int ret;
	int mode = vconf_keynode_get_int(key_nodes);

	ultra_powersaving = (mode == SETTING_PSMODE_EMERGENCY ? true : false);

	ret = set_powersaver_mode(ultra_powersaving);
	if (ret < 0)
		_E("Failed to update powersaver state %d", ret);
}

static void powersaver_brightness_cb(keynode_t *key_nodes, void *data)
{
	int ret;

	powersaver_brightness = vconf_keynode_get_int(key_nodes);

	if (!ultra_powersaving)
		return;

	/* powersaver mode on (brightness) */
	ret = backlight_ops.set_force_brightness(powersaver_brightness);
	if (ret < 0) {
		_E("Failed to set force brightness!");
	} else {
		_I("force brightness %d", powersaver_brightness);
		backlight_ops.update();
	}
}

static void set_custom_cpu_limit(void)
{
	int ret = 0;
	int mode = 0;

	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU,
	    custom_cpu_cb, NULL);
	if (ret != 0)
		_E("Failed to vconf_notify_key_changed!");

	ret = vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU, &mode);
	if (ret < 0) {
		_E("failed to get vconf key");
		return;
	}

	if (mode != true)
		return;

	_I("init");
	device_notify(DEVICE_NOTIFIER_PMQOS_POWERSAVING, (void *)mode);
}

static void set_powersaver_limit(void)
{
	int ret, mode;

	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE,
		powersaver_status_cb, NULL);
	if (ret != 0)
		_E("Failed to vconf_notify_key_changed!");

	ret = vconf_get_int(VCONFKEY_SETAPPL_EMERGENCY_LCD_BRIGHTNESS_INT,
	    &powersaver_brightness);
	if (ret != 0)
		_E("Failed to get powersaver brightness!");

	ret = vconf_notify_key_changed(
	    VCONFKEY_SETAPPL_EMERGENCY_LCD_BRIGHTNESS_INT,
	    powersaver_brightness_cb, NULL);
	if (ret != 0)
		_E("Failed to vconf_notify_key_changed!");

	if (!ultra_powersaving)
		return;

	_I("ultra powersaver mode on!");
	ret = set_powersaver_mode(true);
	if (ret < 0)
		_E("Failed to update powersaver state %d", ret);
}

static int booting_done_cb(void *data)
{
	if (booting_done)
		return 0;

	set_custom_cpu_limit();
	set_powersaver_limit();

	booting_done = true;

	return 0;
}

static void powersaver_init(void *data)
{
	int ret, mode;

	ret = vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &mode);
	if (ret < 0) {
		_E("failed to get vconf key");
		return;
	}
	if (mode == SETTING_PSMODE_EMERGENCY)
		ultra_powersaving = true;

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done_cb);
}

static void powersaver_exit(void *data)
{
	int ret;

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU,
	    custom_cpu_cb);
	if (ret != 0)
		_E("Failed to vconf_ignore_key_changed!");

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE,
	    powersaver_status_cb);
	if (ret != 0)
		_E("Failed to vconf_ignore_key_changed!");

	ret = vconf_ignore_key_changed(
	    VCONFKEY_SETAPPL_EMERGENCY_LCD_BRIGHTNESS_INT,
	    powersaver_brightness_cb);
	if (ret != 0)
		_E("Failed to vconf_ignore_key_changed!");

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done_cb);
	booting_done = false;
}

static const struct device_ops powersaver_device_ops = {
	.name     = "powersaver",
	.init     = powersaver_init,
	.exit     = powersaver_exit,
};

DEVICE_OPS_REGISTER(&powersaver_device_ops)

