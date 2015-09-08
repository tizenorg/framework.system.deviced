/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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


#include "core/log.h"
#include "core/common.h"
#include "apps.h"
#include "core/edbus-handler.h"

#define METHOD_STORAGE_WARNING  "UsbotgWarningPopupLaunch"
#define METHOD_WATCHDOG         "WatchdogPopupLaunch"
#define METHOD_RECOVERY         "RecoveryPopupLaunch"

struct popup_data {
	char *name;
	char *method;
	char *key1;
	char *value1;
	char *key2;
	char *value2;
};

static int system_launch_single_param(struct popup_data * key_data)
{
	char *param[2];

	param[0] = key_data->key1;
	param[1] = key_data->value1;

	return dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_SYSTEM,
			POPUP_INTERFACE_SYSTEM,
			key_data->method, "ss", param);
}

static int system_launch_double_param(struct popup_data * key_data)
{
	char *param[4];

	param[0] = key_data->key1;
	param[1] = key_data->value1;
	param[2] = key_data->key2;
	param[3] = key_data->key2;

	return dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_SYSTEM,
			POPUP_INTERFACE_SYSTEM,
			key_data->method, "ssss", param);
}

static int system_launch(void *data)
{
	struct popup_data *key_data = (struct popup_data *)data;
	if (!key_data || !(key_data->method))
		return -EINVAL;

	if (!strncmp(key_data->method, METHOD_STORAGE_WARNING, strlen(METHOD_STORAGE_WARNING))) {
		return system_launch_single_param(key_data);
	}

	if (!strncmp(key_data->method, METHOD_RECOVERY, strlen(METHOD_RECOVERY))) {
		return system_launch_single_param(key_data);
	}

	if (!strncmp(key_data->method, METHOD_WATCHDOG, strlen(METHOD_WATCHDOG))) {
		return system_launch_double_param(key_data);
	}
	return -EINVAL;
}

static const struct apps_ops system_popup_ops = {
	.name	= "system-syspopup",
	.launch	= system_launch,
};

APPS_OPS_REGISTER(&system_popup_ops)
