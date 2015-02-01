/*
 * test
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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

#include "test.h"

#define TIME_CHANGE_SIGNAL "STimeChanged"

static E_DBus_Signal_Handler *edbus_handler;
static E_DBus_Connection     *edbus_conn;

static void unregister_edbus_signal_handler(void)
{
	e_dbus_signal_handler_del(edbus_conn, edbus_handler);
	e_dbus_connection_close(edbus_conn);
	e_dbus_shutdown();
}

static void time_changed(void *data, DBusMessage *msg)
{
	DBusError err;
	int val;
	int ret;

	_I("edbus signal Received");

	ret = dbus_message_is_signal(msg, DEVICED_INTERFACE_TIME, TIME_CHANGE_SIGNAL);
	if (!ret) {
		_E("dbus_message_is_signal error");
		return;
	}

	_I("%s - %s", DEVICED_INTERFACE_TIME, TIME_CHANGE_SIGNAL);
	if (ret < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s", __func__);
	unregister_edbus_signal_handler();
	ecore_shutdown();
}

static int register_edbus_signal_handler(void)
{
	int ret;

	e_dbus_init();

	edbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!(edbus_conn)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_out;
	}

	edbus_handler = e_dbus_signal_handler_add(edbus_conn, NULL, DEVICED_PATH_TIME,
			DEVICED_INTERFACE_TIME, TIME_CHANGE_SIGNAL, time_changed, NULL);
	if (!(edbus_handler)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_connection_out;
	}
	return 0;

edbus_handler_connection_out:
	e_dbus_connection_close(edbus_conn);
edbus_handler_out:
	e_dbus_shutdown();
	return ret;
}

static void test_signal(void)
{
	_I("test");
	register_edbus_signal_handler();
	ecore_main_loop_begin();
}

static void time_init(void *data)
{
}

static void time_exit(void *data)
{
}

static int time_unit(int argc, char **argv)
{
	if (argv[1] == NULL)
		return -EINVAL;
	else if (argc != 4)
		return -EAGAIN;
	if (strcmp("wait", argv[2]) == 0)
		test_signal();
	return 0;
}

static const struct test_ops power_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "time",
	.init     = time_init,
	.exit    = time_exit,
	.unit    = time_unit,
};

TEST_OPS_REGISTER(&power_test_ops)
