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

#define DBUS_POPUP_PATH_LOWBAT		POPUP_OBJECT_PATH"/Battery"
#define DBUS_POPUP_INTERFACE_LOWBAT	POPUP_INTERFACE_NAME".Battery"

#define METHOD_LOWBAT_POPUP		"BatteryLow"
#define METHOD_LOWBAT_POPUP_DISABLE	"LowBatteryDisable"
#define METHOD_LOWBAT_POPUP_ENABLE	"LowBatteryEnable"

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static int lowbat_pid = 0;
static int lowbat_popup = APPS_ENABLE;

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
	char *dest, *path, *iface, *method;
	struct popup_data * key_data = (struct popup_data *)data;
	int ret;

	if (lowbat_popup == APPS_DISABLE) {
		_D("skip lowbat popup control");
		return 0;
	}

	param[0] = key_data->key;
	param[1] = key_data->value;
	if (strncmp(key_data->value, "lowbattery_warning", 18) == 0 ||
	    strncmp(key_data->value, "lowbattery_critical", 19) == 0) {
		dest = POPUP_BUS_NAME;
		path = DBUS_POPUP_PATH_LOWBAT;
		iface = DBUS_POPUP_INTERFACE_LOWBAT;
		method = METHOD_LOWBAT_POPUP;
	} else {
		dest = POPUP_BUS_NAME;
		path = POPUP_PATH_LOWBAT;
		iface = POPUP_INTERFACE_LOWBAT;
		method = POPUP_METHOD_LAUNCH;
	}
	ret = dbus_method_async_with_reply(dest, path, iface, method,
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

static DBusMessage *dbus_disable_popup(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;

	lowbat_popup = APPS_DISABLE;
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &lowbat_popup);
	return reply;
}

static DBusMessage *dbus_enable_popup(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;

	lowbat_popup = APPS_ENABLE;
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &lowbat_popup);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_LOWBAT_POPUP_DISABLE,	NULL, "i", dbus_disable_popup },
	{ METHOD_LOWBAT_POPUP_ENABLE,	NULL, "i", dbus_enable_popup },
};

static void lowbat_init(void)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_APPS, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static const struct apps_ops lowbat_ops = {
	.name      = "lowbat-syspopup",
	.init      = lowbat_init,
	.launch    = lowbat_launch,
	.terminate = lowbat_terminate,
};

APPS_OPS_REGISTER(&lowbat_ops)
