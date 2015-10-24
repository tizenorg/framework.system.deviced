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

#include <unistd.h>

#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/log.h"

#define BLUETOOTH_OPS_NAME		"bluetooth"
#define BLUETOOTH_HCI_UNIT_TEMPLATE	"bluetooth-hciattach@.service"

enum DD_BT_OPT {
	DD_BT_TURN_ON,
	DD_BT_TURN_OFF,
};

static DBusMessage *deviced_bluetooth_opt(enum DD_BT_OPT opt,
					  E_DBus_Object *obj,
					  DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply = NULL;
	DBusError err;
	const char *instance;
	char *unit = NULL;
	int ret;

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg,
				   &err,
				   DBUS_TYPE_STRING, &instance,
				   DBUS_TYPE_INVALID)) {
		_E("Bad message: [%s:%s]", err.name, err.message);
		ret = -EBADMSG;
		goto finish;
	}

	ret = deviced_systemd_instance_new_from_template(
		BLUETOOTH_HCI_UNIT_TEMPLATE,
		instance,
		&unit);
	if (ret < 0)
		goto finish;

	switch (opt) {
	case DD_BT_TURN_ON:
		ret = deviced_systemd_start_unit(unit);
		break;
	case DD_BT_TURN_OFF:
		ret = deviced_systemd_stop_unit(unit);
		break;
	default:
		ret = -EINVAL;
		goto finish;
	}

	reply = dbus_message_new_method_return(msg);

finish:
	dbus_error_free(&err);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *deviced_bluetooth_turn_on(E_DBus_Object *obj, DBusMessage *msg)
{
	return deviced_bluetooth_opt(DD_BT_TURN_ON, obj, msg);
}

static DBusMessage *deviced_bluetooth_turn_off(E_DBus_Object *obj, DBusMessage *msg)
{
	return deviced_bluetooth_opt(DD_BT_TURN_OFF, obj, msg);
}

static int get_state_bluetooth_target(char **state)
{
	return deviced_systemd_get_unit_property_as_string("bluetooth.target",
							   "ActiveState",
							   state);
}

static DBusMessage *deviced_bluetooth_get_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state = NULL;
	int ret;

	ret = get_state_bluetooth_target(&state);
	if (ret < 0)
		_E("fail send message");

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &state);
	free(state);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "TurnOn",	"s",	"i",	deviced_bluetooth_turn_on },
	{ "TurnOff",	"s",	"i",	deviced_bluetooth_turn_off },
	{ "GetState",	NULL,	"s",	deviced_bluetooth_get_state },
	/* Add methods here */
};

static void bluetooth_init(void *data)
{
	int bTelReady = 0;
	int ret;

	/* init dbus interface */
	ret = register_edbus_interface_and_method(DEVICED_PATH_BLUETOOTH,
			DEVICED_INTERFACE_BLUETOOTH,
			edbus_methods,
			ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static const struct device_ops bluetooth_device_ops = {
	.name     = BLUETOOTH_OPS_NAME,
	.init     = bluetooth_init,
};

DEVICE_OPS_REGISTER(&bluetooth_device_ops)
