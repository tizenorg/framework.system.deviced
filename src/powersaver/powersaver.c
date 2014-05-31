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

static int set_powersaver_mode(int on)
{
	int ret;
	int brightness, timeout;

	_I("powersaver mode %s", (on ? "on" : "off"));
	device_notify(DEVICE_NOTIFIER_POWERSAVER, (void*)on);

	/* powersaver mode off */
	if (!on) {
		backlight_ops.set_force_brightness(0);
		set_force_lcdtimeout(0);
		goto update_state;
	}

	/* powersaver mode on (brightness) */
	ret = vconf_get_int(VCONFKEY_SETAPPL_EMERGENCY_LCD_BRIGHTNESS_INT,
	    &brightness);
	if (ret != 0) {
		_E("Failed to get powersaver brightness!");
		return ret;
	}
	ret = backlight_ops.set_force_brightness(brightness);
	if (ret < 0) {
		_E("Failed to set force brightness!");
		return ret;
	}
	_I("force brightness %d", brightness);

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
	/* update internal state */
	if (hbm_get_state())
		hbm_set_state_with_timeout(false, 0);
	backlight_ops.update();
	ret = get_run_timeout(&timeout);
	if (ret >= 0)
		states[S_NORMAL].timeout = timeout;
	states[pm_cur_state].trans(EVENT_INPUT);

	return 0;
}

static void powersaver_status_changed(keynode_t *key_nodes, void *data)
{
	int status, on, ret;

	if (key_nodes == NULL) {
		_E("wrong parameter, key_nodes is null");
		return;
	}

	status = vconf_keynode_get_int(key_nodes);
	if (status == SETTING_PSMODE_NORMAL)
		on = false;
	else if (status == SETTING_PSMODE_WEARABLE)
		on = true;
	else
		return;

	ret = set_powersaver_mode(on);
	if (ret < 0)
		_E("Failed to update powersaver state %d", ret);
}

static int booting_done(void *data)
{
	int ret, status;

	ret = vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &status);
	if (ret != 0) {
		_E("Failed to vconf get bool!");
		return -EIO;
	}

	if (status != SETTING_PSMODE_WEARABLE)
		return 0;

	_D("powersaver mode on!");
	ret = set_powersaver_mode(true);
	if (ret < 0)
		_E("Failed to update powersaver state %d", ret);

	return ret;
}

static void powersaver_init(void *data)
{
	int ret;

	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE,
	    powersaver_status_changed, NULL);
	if (ret != 0)
		_E("Failed to vconf_notify_key_changed!");

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static void powersaver_exit(void *data)
{
	int ret;

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE,
	    powersaver_status_changed);
	if (ret != 0)
		_E("Failed to vconf_ignore_key_changed!");

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static const struct device_ops powersaver_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "powersaver",
	.init     = powersaver_init,
	.exit     = powersaver_exit,
};

DEVICE_OPS_REGISTER(&powersaver_device_ops)
