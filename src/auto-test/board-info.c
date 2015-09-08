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

#define METHOD_GET_NUM	"GetNum"
#define METHOD_GET_SERIAL	"GetSerial"
#define METHOD_GET_REVISION	"GetHWRev"

void board_serial(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *data;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME, DEVICED_PATH_BOARD,
		DEVICED_INTERFACE_BOARD, METHOD_GET_SERIAL, NULL, NULL);
	if (!msg) {
		_E("fail send message");
		return;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &data, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		return;
	}
	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s %d", data, val);
	if (val < 0)
		_R("[NG] ---- %s     : V(if your target is eng, NG is Normal)",
		__func__);
	else
		_R("[OK] ---- %s     : V(%s)", __func__, data);
}

static void board_revision(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME, DEVICED_PATH_BOARD,
		DEVICED_INTERFACE_BOARD, METHOD_GET_REVISION, NULL, NULL);
	if (!msg) {
		_E("fail send message");
		return;
	}
	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);

	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		return;
	}
	dbus_message_unref(msg);
	dbus_error_free(&err);
	_E("%s-%s : %d", DEVICED_INTERFACE_BOARD, METHOD_GET_REVISION, val);
	if(val >= 8) {
		_D("Rinato Neo");
	} else {
		_D("Rinato");
	}
	if (val < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s   : V(%d)", __func__, val);
}

void board_numer(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *data;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME, DEVICED_PATH_BOARD,
		DEVICED_INTERFACE_BOARD, METHOD_GET_NUM, NULL, NULL);
	if (!msg) {
		_E("fail send message");
		return;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &data, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		return;
	}
	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s %d", data, val);
	if (val < 0)
		_R("[NG] ---- %s      : V(if your target is eng, NG is Normal)",
		__func__);
	else
		_R("[OK] ---- %s      : V(%s)", __func__, data);
}

static void board_init(void *data)
{
	_I("start test");
	board_revision();
	board_serial();
	board_numer();
}

static void board_exit(void *data)
{
	_I("end test");
}

static int board_unit(int argc, char **argv)
{
	board_revision();
	board_serial();
	board_numer();
	return 0;
}

static const struct test_ops board_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "board",
	.init     = board_init,
	.exit    = board_exit,
	.unit    = board_unit,
};

TEST_OPS_REGISTER(&board_test_ops)
