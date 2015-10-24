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

#define METHOD_GET_CRADLE	"GetCradle"

static const struct device_change_type {
	char *name;
	char *status;
} device_change_types[] = {
	{"cradle",	"1"},
	{"cradle",	"0"},
};

static int test_cradle(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI,
			DEVICED_INTERFACE_SYSNOTI,
			METHOD_GET_CRADLE, NULL, NULL);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			METHOD_GET_CRADLE);
		return -EBADMSG;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val,
			DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -1;
	}
	_I("%d", val);
	if (val < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s      : V(%d)", __func__, val);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	sleep(TEST_WAIT_TIME_INTERVAL);
	return val;
}

static int cradle(int index)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *param[4];

	param[0] = METHOD_SET_DEVICE;
	param[1] = "2";
	param[2] = device_change_types[index].name;
	param[3] = device_change_types[index].status;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_EXTCON,
			DEVICED_INTERFACE_EXTCON,
			METHOD_SET_DEVICE, "siss", param);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_EXTCON, DEVICED_INTERFACE_EXTCON,
			METHOD_SET_DEVICE);
		return -EBADMSG;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (ret == 0) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		val = -EBADMSG;
	}
	_I("%s %s", device_change_types[index].name, device_change_types[index].status);
	if (val < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s           : V(%s %s)",
		__func__, device_change_types[index].name, device_change_types[index].status);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	sleep(TEST_WAIT_TIME_INTERVAL);
	return val;
}

static void unit(char *unit, char *status)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(device_change_types); index++) {
		if (strcmp(unit, device_change_types[index].name) != 0 ||
		    strcmp(status, device_change_types[index].status) != 0)
			continue;
		cradle(index);
		test_cradle();
	}
}

static void cradle_init(void *data)
{
	int index;

	_I("start test");
	for (index = 0; index < ARRAY_SIZE(device_change_types); index++)
		cradle(index);
}

static void cradle_exit(void *data)
{
	_I("end test");
}

static int cradle_unit(int argc, char **argv)
{
	int status;

	if (argv[1] == NULL)
		return -EINVAL;
	if (argc != 4)
		return -EAGAIN;

	unit(argv[1], argv[2]);
out:
	return 0;
}

static const struct test_ops cradle_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "cradle",
	.init     = cradle_init,
	.exit    = cradle_exit,
	.unit    = cradle_unit,
};

TEST_OPS_REGISTER(&cradle_test_ops)
