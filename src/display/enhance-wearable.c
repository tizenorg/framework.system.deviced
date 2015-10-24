/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <stdbool.h>
#include <device-node.h>
#include <vconf.h>

#include "core.h"
#include "core/devices.h"
#include "core/log.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "powersaver/powersaver.h"

#define VCONF_HIGH_CONTRAST VCONFKEY_SETAPPL_ACCESSIBILITY_HIGH_CONTRAST
#define SCENARIO_NORMAL		0
#define SCENARIO_NEGATIVE	6
#define SCENARIO_GRAYSCALE	9

static bool negative_status;
static bool grayscale_status;

static inline void enhance_set_scenario_state(int state)
{
	int cmd, ret;

	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, state);
	if (ret < 0)
		_E("Failed to set scenario! %d", state);
	else
		_D("set scenario %d", state);
}

static void enhance_update_state(void)
{
	_D("grayscale %d negative %d", grayscale_status, negative_status);

	if (grayscale_status)
		enhance_set_scenario_state(SCENARIO_GRAYSCALE);
	else if (negative_status)
		enhance_set_scenario_state(SCENARIO_NEGATIVE);
	else
		enhance_set_scenario_state(SCENARIO_NORMAL);
}

static void enhance_high_contrast_cb(keynode_t *in_key, void *data)
{
	int val, ret;

	ret = vconf_get_bool(VCONF_HIGH_CONTRAST, &val);
	if (ret < 0) {
		_E("fail to get %s", VCONF_HIGH_CONTRAST);
		return;
	}

	_I("set negative scenario (%d)", val);
	negative_status = val;

	enhance_update_state();
}

static int enhance_lcd_state_changed(void *data)
{
	int state;

	state = *(int *)data;

	if (state == S_NORMAL)
		enhance_update_state();

	return 0;
}

static int enhance_ultrapowersaving_changed(void *data)
{
	int mode;

	mode = *(int *)data;

	grayscale_status = (mode == POWERSAVER_ENHANCED);

	enhance_update_state();

	return 0;
}

static void enhance_init(void *data)
{
	int val, ret;

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_LCD, enhance_lcd_state_changed);
	register_notifier(DEVICE_NOTIFIER_PMQOS_ULTRAPOWERSAVING,
	    enhance_ultrapowersaving_changed);

	ret = vconf_notify_key_changed(VCONF_HIGH_CONTRAST,
	    enhance_high_contrast_cb, NULL);
	if (ret < 0)
		_E("Failed to register notify: %s", VCONF_HIGH_CONTRAST);

	ret = vconf_get_bool(VCONF_HIGH_CONTRAST, &val);
	if (ret < 0) {
		_E("Failed to get high contrast!");
		return;
	}

	/* update negative status */
	_I("set high contrast (%d)", val);
	negative_status = val;

	enhance_update_state();
}

static void enhance_exit(void *data)
{
	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_LCD, enhance_lcd_state_changed);
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_ULTRAPOWERSAVING,
	    enhance_ultrapowersaving_changed);

	vconf_ignore_key_changed(VCONF_HIGH_CONTRAST, enhance_high_contrast_cb);
}

static const struct device_ops enhance_device_ops = {
	.name     = "enhance",
	.init     = enhance_init,
	.exit     = enhance_exit,
};

DEVICE_OPS_REGISTER(&enhance_device_ops)

