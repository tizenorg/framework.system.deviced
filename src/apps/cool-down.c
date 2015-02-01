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
#include "core/launch.h"

#define COOL_DOWN_POPUP_BUS_NAME	POPUP_BUS_NAME
#define COOL_DOWN_POPUP_METHOD_LAUNCH	"CooldownPopupLaunch"

#define COOL_DOWN_POWER_OFF		"cooldown_poweroff"
#define COOL_DOWN_POWER_WARNING		"cooldown_warning"
#define COOL_DOWN_POWER_ON		"cooldown_poweron"

#define SIGNAL_COOL_DOWN_SHUT_DOWN	"ShutDown"
#define SIGNAL_COOL_DOWN_SHUT_DOWN_LEN	8
#define SIGNAL_COOL_DOWN_LIMIT_ACTION	"LimitAction"
#define SIGNAL_COOL_DOWN_LIMIT_ACTION_LEN	11
#define SIGNAL_COOL_DOWN_RELEASE	"Release"
#define SIGNAL_COOL_DOWN_RELEASE_LEN	7

struct popup_data {
	char *name;
	char *key;
};

static int cool_down_launch(void *data)
{
	char *param[2];
	struct popup_data * key_data = (struct popup_data *)data;
	int ret;

	if (!key_data || !key_data->key)
		goto out;

	_I("%s", key_data->key);

	if (strncmp(key_data->key, SIGNAL_COOL_DOWN_SHUT_DOWN,
	    SIGNAL_COOL_DOWN_SHUT_DOWN_LEN) == 0)
	    param[1] = COOL_DOWN_POWER_OFF;
	else if (strncmp(key_data->key, SIGNAL_COOL_DOWN_LIMIT_ACTION,
	    SIGNAL_COOL_DOWN_LIMIT_ACTION_LEN) == 0)
	     param[1] = COOL_DOWN_POWER_WARNING;
	else if (strncmp(key_data->key, SIGNAL_COOL_DOWN_RELEASE,
	    SIGNAL_COOL_DOWN_RELEASE_LEN) == 0)
	     param[1] = COOL_DOWN_POWER_ON;
	else
		goto out;

	param[0] = POPUP_KEY_CONTENT;

	return dbus_method_async(COOL_DOWN_POPUP_BUS_NAME,
		POPUP_PATH_SYSTEM, POPUP_INTERFACE_SYSTEM,
		COOL_DOWN_POPUP_METHOD_LAUNCH, "ss", param);
out:
	return 0;
}


static const struct apps_ops cooldown_ops = {
	.name   = "cooldown-syspopup",
	.launch = cool_down_launch,
};

APPS_OPS_REGISTER(&cooldown_ops)
