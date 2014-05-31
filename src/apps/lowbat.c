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

#define LOWBAT_WARNING  "warning"
#define LOWBAT_CRITICAL  "critical"
#define LOWBAT_POWEROFF "poweroff"

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static int lowbat_pid = 0;

static void lowbat_cb(void *data, DBusMessage *msg, DBusError *unused)
{
	DBusError err;
	int ret, id;

	if (!msg)
		return;

	dbus_error_init(&err);
	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &id, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}

	lowbat_pid = id;
	_I("Created popup : %d", lowbat_pid);
}

static int lowbat_launch(void *data)
{
	char *param[2];
	struct popup_data * key_data = (struct popup_data *)data;
	int ret;

	param[0] = key_data->key;
	param[1] = key_data->value;

	ret = dbus_method_async_with_reply(POPUP_BUS_NAME,
			POPUP_PATH_LOWBAT,
			POPUP_INTERFACE_LOWBAT,
			POPUP_METHOD_LAUNCH,
			"ss", param, lowbat_cb, -1, NULL);

	if (strncmp(key_data->value, LOWBAT_WARNING, strlen(LOWBAT_WARNING)) &&
	    strncmp(key_data->value, LOWBAT_CRITICAL, strlen(LOWBAT_CRITICAL)) &&
	    strncmp(key_data->value, LOWBAT_POWEROFF, strlen(LOWBAT_POWEROFF)))
		_I("%s(%d)",key_data->name, ret);

	return ret;
}

static int lowbat_terminate(void *data)
{
	int pid;
	int ret;
	char *param[1];
	char buf[PATH_MAX];

	printf("\n");

	if (lowbat_pid <= 0)
		return 0;

	snprintf(buf, sizeof(buf), "%d", lowbat_pid);
	param[0] = buf;

	ret = dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_APP,
			POPUP_IFACE_APP,
			POPUP_METHOD_TERMINATE, "i", param);
	if (ret < 0)
		_E("FAIL: dbus_method_sync(): %d %d", ret, param);
	lowbat_pid = 0;
	return 0;
}


static const struct apps_ops lowbat_ops = {
	.name	= "lowbat-syspopup",
	.launch	= lowbat_launch,
	.terminate = lowbat_terminate,
};

APPS_OPS_REGISTER(&lowbat_ops)
