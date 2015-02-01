/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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
#include <assert.h>
#include <limits.h>
#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/launch.h"

static DBusMessage *edbus_testmode_launch(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *argv;
	char param[32];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}
	sprintf(param, "%s", argv);
	_D("param %s", param);

	launch_evenif_exist("/usr/bin/testmode-env.sh", param);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_factory_15_launch(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *argv;
	char param[32];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}
	sprintf(param, "%s", argv);
	_D("param %s", param);

	launch_evenif_exist("/usr/bin/factory-15-env.sh", param);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}


static const struct edbus_method edbus_methods[] = {
	{ "Launch", "s",   "i", edbus_testmode_launch },
	{ "factory15", "s",   "i", edbus_factory_15_launch },
	/* Add methods here */
};

/* battery ops init funcion */
static void testmode_init(void *data)
{
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_TESTMODE,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

/* battery ops exit funcion */
static void testmode_exit(void *data)
{
}

static const struct device_ops battery_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "testmode",
	.init     = testmode_init,
	.exit     = testmode_exit,
};

DEVICE_OPS_REGISTER(&battery_device_ops)
