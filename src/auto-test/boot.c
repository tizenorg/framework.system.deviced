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
#define DD_BUS_NAME       "org.tizen.system.deviced"
#define DD_OBJECT_PATH    "/Org/Tizen/System/DeviceD/Power"
#define DD_INTERFACE_NAME "org.tizen.system.deviced.power"
#define DD_POWEROFF_METHOD_NAME	"poweroff"
#define DD_REBOOT_METHOD_NAME	"reboot"

#define DBUS_REPLY_TIMEOUT (120 * 1000)
#define EDBUS_INIT_RETRY_COUNT 5
static int edbus_init_val;
static DBusConnection *conn;
static E_DBus_Connection *edbus_conn;

static const struct boot_control_type {
	char *name;
	char *status;
} boot_control_types[] = {
	{"poweroffpopup",	"pwroff-popup"},
	{"poweroffpopup",	"reboot"},
	{"poweroffpopup",	"reboot-recovery"},
	{"poweroffpopup",	"poweroff"},
	{"poweroffpopup",	"fota"},
};

static void edbus_init(void)
{
	DBusError error;
	int retry = 0;
	int i, ret;

	dbus_threads_init_default();
	dbus_error_init(&error);

	do {
		edbus_init_val = e_dbus_init();
		if (edbus_init_val)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to init edbus");
			return;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);

	retry = 0;
	do {
		conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
		if (conn)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to get dbus");
			goto out1;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);

	retry = 0;
	do {
		edbus_conn = e_dbus_connection_setup(conn);
		if (edbus_conn)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to get edbus");
			goto out2;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);
	return;
out2:
	dbus_connection_set_exit_on_disconnect(conn, FALSE);
out1:
	e_dbus_shutdown();
}

static void edbus_exit(void)
{
	e_dbus_connection_close(edbus_conn);
	e_dbus_shutdown();
}

static int broadcast_edbus_signal(const char *path, const char *interface,
		const char *name, const char *sig, char *param[])
{
	DBusMessage *msg;
	DBusMessageIter iter;
	int r;

	msg = dbus_message_new_signal(path, interface, name);
	if (!msg) {
		_E("fail to allocate new %s.%s signal", interface, name);
		return -EPERM;
	}

	dbus_message_iter_init_append(msg, &iter);
	r = append_variant(&iter, sig, param);
	if (r < 0) {
		_E("append_variant error(%d)", r);
		return -EPERM;
	}

	r = dbus_connection_send(conn, msg, NULL);
	dbus_message_unref(msg);

	if (r != TRUE) {
		_E("dbus_connection_send error (%s:%s-%s)", path, interface, name);
		return -ECOMM;
	}
	return 0;
}

static void poweroff_popup(char *status)
{
	char *arr[2];
	char str_status[32];
	int val;

	snprintf(str_status, sizeof(str_status), "%s", status);
	arr[0] = str_status;
	arr[1] = "0";
	_D("broadcast poweroff %s %s", arr[0], arr[1]);

	edbus_init();
	val = broadcast_edbus_signal(DEVICED_OBJECT_PATH, DEVICED_INTERFACE_NAME,
			"poweroffpopup", "si", arr);
	if (val < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s   : V(%s)", __func__, status);
	edbus_exit();
}

static void unit(char *unit, char *status)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(boot_control_types); index++) {
		if (strcmp(unit, boot_control_types[index].name) != 0 ||
		    strcmp(status, boot_control_types[index].status) != 0)
			continue;
		if (strcmp(unit, "poweroffpopup") == 0)
			poweroff_popup(boot_control_types[index].status);
	}

}

static void boot_init(void *data)
{
}

static void boot_exit(void *data)
{
}

static int dd_append_variant(DBusMessageIter *iter, const char *sig, char *param[])
{
	char *ch;
	int i;
	int int_type;
	uint64_t int64_type;

	if (!sig || !param)
		return 0;

	for (ch = (char *)sig, i = 0; *ch != '\0'; ++i, ++ch) {
		switch (*ch) {
		case 'i':
			int_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &int_type);
			break;
		case 'u':
			int_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &int_type);
			break;
		case 't':
			int64_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &int64_type);
			break;
		case 's':
			dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &param[i]);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int dd_dbus_method_sync(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	int r, ret;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn)
		return -EPERM;

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg)
		return -1;

	dbus_message_iter_init_append(msg, &iter);
	r = dd_append_variant(&iter, sig, param);
	if (r < 0) {
		dbus_message_unref(msg);
		return -1;
	}

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	dbus_message_unref(msg);
	if (!reply) {
		dbus_error_free(&err);
		return -1;
	}

	r = dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID);
	dbus_message_unref(reply);
	if (!r) {
		dbus_error_free(&err);
		return -1;
	}

	return ret;
}

static int request_power_off(void)
{
	char *params[2];

	_D("test");
	params[0] = DD_POWEROFF_METHOD_NAME;
	params[1] = "0";
	return dd_dbus_method_sync(DD_BUS_NAME, DD_OBJECT_PATH,
			DD_INTERFACE_NAME, DD_POWEROFF_METHOD_NAME, "si", params);
}

static int request_reboot(void)
{
	char *params[2];

	_D("test");
	params[0] = DD_REBOOT_METHOD_NAME;
	params[1] = "0";
	return dd_dbus_method_sync(DD_BUS_NAME, DD_OBJECT_PATH,
			DD_INTERFACE_NAME, DD_REBOOT_METHOD_NAME, "si", params);
}


static int boot_unit(int argc, char **argv)
{
	int status;

	if (argv[1] == NULL)
		return -EINVAL;
	if (strcmp("off", argv[2]) == 0)
		request_power_off();
	else if (strcmp("reboot", argv[2]) == 0)
		request_reboot();
	if (argc < 3)
		return -EAGAIN;
	unit(argv[argc-2], argv[argc-1]);
out:
	return 0;
}

static const struct test_ops boot_test_ops = {
	.priority = TEST_PRIORITY_HIGH,
	.name     = "boot",
	.init     = boot_init,
	.exit    = boot_exit,
	.unit    = boot_unit,
};

TEST_OPS_REGISTER(&boot_test_ops)
