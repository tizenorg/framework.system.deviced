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

#define METHOD_GET_REVISION	"GetRevision"

static void get_revision(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI,
		DEVICED_INTERFACE_SYSNOTI, METHOD_GET_REVISION, NULL, NULL);
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
	_E("%s-%s : %d", DEVICED_INTERFACE_SYSNOTI, METHOD_GET_REVISION, val);
	if(val >= 8) {
		_D("Rinato Neo");
	} else {
		_D("Rinato");
	}
}

static void cpuinfo_init(void *data)
{
	_I("start test");
	get_revision();
}

static void cpuinfo_exit(void *data)
{
	_I("end test");
}

static int cpuinfo_unit(int argc, char **argv)
{
	get_revision();
	return 0;
}

static const struct test_ops cpuinfo_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "cpuinfo",
	.init     = cpuinfo_init,
	.exit    = cpuinfo_exit,
	.unit    = cpuinfo_unit,
};

TEST_OPS_REGISTER(&cpuinfo_test_ops)
