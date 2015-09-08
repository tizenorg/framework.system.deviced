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

#define METHOD_GET_STORAGE	"getstorage"
#define METHOD_MEM_TRIM	"MemTrim"

static void mem_trim(void *data, DBusMessage *msg, DBusError *tmp)
{
	DBusError err;
	int ret, result;

	if (!msg)
		return;

	dbus_error_init(&err);
	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}
	_D("succeed mem trim : %d", result);
}

static int request_mem_trim(void)
{
	int ret;

	ret = dbus_method_async_with_reply(DEVICED_BUS_NAME,
		DEVICED_PATH_STORAGE, DEVICED_INTERFACE_STORAGE,
		METHOD_MEM_TRIM, NULL, NULL, mem_trim, -1, NULL);
	if (ret == 0)
		_D("request mem trim");
	return ret;
}

static int storage(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret;
	double dAvail;
	double dTotal;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_STORAGE,
			DEVICED_INTERFACE_STORAGE,
			METHOD_GET_STORAGE, NULL, NULL);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			METHOD_GET_STORAGE);
		return -EBADMSG;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT64, &dTotal, DBUS_TYPE_INT64, &dAvail, DBUS_TYPE_INVALID);;
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
	}
	_I("total : %4.4lf avail %4.4lf", dTotal, dAvail);
	if (ret < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s          : T(%4.4lf) A(%4.4lf)",
		__func__, dTotal, dAvail);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	sleep(TEST_WAIT_TIME_INTERVAL);
	return ret;
}

static void storage_init(void *data)
{
	_I("start test");
	storage();
}

static void storage_exit(void *data)
{
	_I("end test");
}

static int storage_unit(int argc, char **argv)
{
	if (argv[1] == NULL)
		return -EINVAL;
	if (strcmp(argv[2], "value") == 0)
		storage();
	else if (strcmp(argv[2], "signal") == 0) {
		request_mem_trim();
		ecore_main_loop_begin();
	}
	return 0;
}

static const struct test_ops storage_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "storage",
	.init     = storage_init,
	.exit    = storage_exit,
	.unit    = storage_unit,
};

TEST_OPS_REGISTER(&storage_test_ops)
