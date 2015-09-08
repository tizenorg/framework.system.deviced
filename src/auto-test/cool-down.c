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

#define METHOD_COOL_DOWN_STATUS		"GetCoolDownStatus"
#define METHOD_COOL_DOWN_CHANGED	"CoolDownChanged"
static int get_status(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret;
	char *str;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI,
			DEVICED_INTERFACE_SYSNOTI,
			METHOD_COOL_DOWN_STATUS, NULL, NULL);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			METHOD_COOL_DOWN_STATUS);
		return -EBADMSG;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str,
			DBUS_TYPE_INVALID);
	if (!ret)
		_E("no message : [%s:%s]", err.name, err.message);

	_I("%s", str);
	if (ret < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s      : V(%s)", __func__, str);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret;
}

static int cool_down(char *name, char *status)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *param[4];

	param[0] = METHOD_SET_DEVICE;
	param[1] = "2";
	param[2] = name;
	param[3] = status;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI,
			DEVICED_INTERFACE_SYSNOTI,
			METHOD_COOL_DOWN_CHANGED, "siss", param);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
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
	_I("%s %s", name, status);
	if (val < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s           : V(%s %s)",
		__func__, name, status);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	return val;
}

static void unit(char *unit, char *status)
{
	int index;

	cool_down(unit, status);
	get_status();
}

static void cool_down_init(void *data)
{
}

static void cool_down_exit(void *data)
{
}

static int cool_down_unit(int argc, char **argv)
{
	int status;

	if (argv[1] == NULL)
		return -EINVAL;
	if (argc != 4)
		return -EAGAIN;

	unit(argv[2], argv[3]);
out:
	return 0;
}

static const struct test_ops cool_down_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "cool-down",
	.init     = cool_down_init,
	.exit     = cool_down_exit,
	.unit     = cool_down_unit,
};

TEST_OPS_REGISTER(&cool_down_test_ops)
