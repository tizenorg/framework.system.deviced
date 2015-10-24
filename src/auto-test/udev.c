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

static const struct device_change_type {
	char *name;
	char *status;
} device_change_types[] = {
	{"udev",		"start"},
	{"udev",		"stop"},
};

static int udev(int index)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *param[3];

	param[0] = "udev";
	param[1] = "1";
	param[2] = device_change_types[index].status;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			"udev", "sis", param);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			"udev");
		return -EBADMSG;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}
	_I("%s", device_change_types[index].status);
	if (val < 0)
		_R("[NG] ---- %s             : %s",
		__func__, device_change_types[index].status);
	else
		_R("[OK] ---- %s             : %s",
		__func__, device_change_types[index].status);
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
		udev(index);
	}
}

static void udev_init(void *data)
{
	int index;

	_I("start test");
	for (index = 0; index < ARRAY_SIZE(device_change_types); index++)
		udev(index);
}

static void udev_exit(void *data)
{
	_I("end test");
}

static int udev_unit(int argc, char **argv)
{
	int status;

	if (argv[1] == NULL)
		return -EINVAL;
	if (argc != 3)
		return -EAGAIN;

	unit(argv[1], argv[2]);
out:
	return 0;
}

static const struct test_ops udev_test_ops = {
	.priority = TEST_PRIORITY_HIGH,
	.name     = "udev",
	.init     = udev_init,
	.exit    = udev_exit,
	.unit    = udev_unit,
};

TEST_OPS_REGISTER(&udev_test_ops)
