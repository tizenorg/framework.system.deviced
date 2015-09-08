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

static int power_saving_custom_cpu_cb(keynode_t *key_nodes, void *data)
{
	int val = 0;

	val = vconf_keynode_get_bool(key_nodes);
	device_notify(DEVICE_NOTIFIER_PMQOS_POWERSAVING, (void*)val);
	return 0;
}

static int emergency_cpu_cb(keynode_t *key_nodes, void *data)
{
	int val;

	val = vconf_keynode_get_int(key_nodes);
	if (val == SETTING_PSMODE_EMERGENCY)
		val = 1;
	else
		val = 0;
	device_notify(DEVICE_NOTIFIER_PMQOS_EMERGENCY, (void*)val);
	return 0;
}

static void set_freq_limit(void)
{
	int ret = 0;
	int val = 0;

	ret = vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU,
			&val);
	if (ret < 0) {
		_E("failed to get vconf key");
		return;
	}
	if (val == 0)
		return;
	_I("init");
	device_notify(DEVICE_NOTIFIER_PMQOS_POWERSAVING, (void*)val);
}

static void set_emergency_limit(void)
{
	int ret, val;

	ret = vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &val);
	if (ret < 0) {
		_E("failed to get vconf key");
		return;
	}
	if (val != SETTING_PSMODE_EMERGENCY)
		return;

	val = 1;
	device_notify(DEVICE_NOTIFIER_PMQOS_EMERGENCY, (void*)val);

}

static int booting_done(void *data)
{
	set_freq_limit();
	set_emergency_limit();

	return 0;
}

static void powersaver_init(void *data)
{
	int ret;

	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU,
		(void *)power_saving_custom_cpu_cb, NULL);
	if (ret != 0)
		_E("Failed to vconf_notify_key_changed!");
	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE,
		(void *)emergency_cpu_cb, NULL);
	if (ret != 0)
		_E("Failed to vconf_notify_key_changed!");
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static void powersaver_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static const struct device_ops powersaver_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "powersaver",
	.init     = powersaver_init,
	.exit     = powersaver_exit,
};

DEVICE_OPS_REGISTER(&powersaver_device_ops)
