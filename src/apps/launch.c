/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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


#include <vconf.h>
#include "core/log.h"
#include "core/common.h"
#include "apps.h"
#include "core/edbus-handler.h"

#define PWLOCK_NAME	"pwlock"
#define CRADLE_NAME	"desk-dock"

#define RETRY_MAX 5

struct popup_data {
	char *name;
	char *value;
};

static pid_t cradle_pid = 0;

static int launch_app_pwlock(void);
static int launch_app_cradle(void);

void get_pwlock_app_pid(void *data, DBusMessage *msg, DBusError *err)
{
	DBusError r_err;
	int ret;
	pid_t pid;

	if (!msg) {
		_E("Cannot get pid of pwlock app");
		ret = launch_app_pwlock();
		if (ret < 0)
			_E("Failed to launch pwlock app(%d)", ret);
		return;
	}

	dbus_error_init(&r_err);
	ret = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &pid, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		return;
	}

	_I("Pid of cradle app is (%d)", pid);
}

static int launch_app_pwlock(void)
{
	int i, ret;
	char *pa[2];

	pa[0] = "after_bootup";
	pa[1] = "1";

	i = 0;
	do {
		ret = dbus_method_async_with_reply(POPUP_BUS_NAME,
				POPUP_PATH_APP, POPUP_IFACE_APP,
				"PWLockAppLaunch", "ss", pa,
				get_pwlock_app_pid, -1, NULL);
		if (ret < 0)
			_E("Failed to request launching cradle app(%d), retry(%d)", ret, i);
		else
			break;
	} while(i++ < RETRY_MAX);

	return ret;
}

void get_cradle_app_pid(void *data, DBusMessage *msg, DBusError *err)
{
	DBusError r_err;
	int ret;
	pid_t pid;

	if (!msg) {
		_E("Cannot get pid of cradle app");
		ret = launch_app_cradle();
		if (ret < 0)
			_E("Failed to launch cradle app (%d)", ret);
		return;
	}

	dbus_error_init(&r_err);
	ret = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &pid, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		return;
	}

	cradle_pid = pid;
	_I("Pid of cradle app is (%d)", cradle_pid);
}

static int launch_app_cradle(void)
{
	int i, ret;

	i = 0;
	do {
		ret = dbus_method_async_with_reply(POPUP_BUS_NAME,
				POPUP_PATH_APP, POPUP_IFACE_APP,
				"CradleAppLaunch", NULL, NULL,
				get_cradle_app_pid, -1, NULL);
		if (ret < 0)
			_E("Failed to request launching cradle app(%d), retry(%d)", ret, i);
		else
			break;
	} while(i++ < RETRY_MAX);

	return ret;
}

void get_terminate_app_result(void *data, DBusMessage *msg, DBusError *err)
{
	DBusError r_err;
	int ret, result;

	if (!msg) {
		_E("Cannot get result of terminating app");
		return;
	}

	dbus_error_init(&r_err);
	ret = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		return;
	}

	_I("The result of terminating app is (%d)", result);
}

static int terminate_app_by_pid(pid_t pid)
{
	int i, ret;
	char *pa[1];
	char cPid[32];

	if (pid <= 0)
		return -EINVAL;

	snprintf(cPid, sizeof(cPid), "%d", pid);
	pa[0] = cPid;

	i = 0;
	do {
		ret = dbus_method_async_with_reply(POPUP_BUS_NAME,
				POPUP_PATH_APP, POPUP_IFACE_APP,
				"AppTerminateByPid", "i", pa,
				get_terminate_app_result, -1, NULL);
		if (ret < 0)
			_E("Failed to request terminating app(%d), retry(%d)", ret, i);
		else
			break;
	} while(i++ < RETRY_MAX);

	return ret;
}

static int launch_app(void *data)
{
	int ret = 0;
	struct popup_data *params;

	if (!data)
		return -ENOMEM;
	params = (struct popup_data *)data;

	_I("%s", params->value);
	if (!strncmp(PWLOCK_NAME, params->value, strlen(params->value)))
		ret = launch_app_pwlock();
	else if (!strncmp(CRADLE_NAME, params->value, strlen(params->value)))
		ret = launch_app_cradle();
	return ret;
}


static int terminate_app(void *data)
{
	struct popup_data *params;

	if (!data || cradle_pid == 0)
		return -ENOMEM;
	params = (struct popup_data *)data;
	_I("%s", params->value);
	if (!strncmp(CRADLE_NAME, params->value, strlen(params->value))) {
		if (terminate_app_by_pid(cradle_pid) < 0)
			_E("Failed to terminate app (%d)", cradle_pid);
		cradle_pid = 0;
	}
	return 0;
}


static const struct apps_ops launch_ops = {
	.name		= "launch-app",
	.launch		= launch_app,
	.terminate	= terminate_app,
};

APPS_OPS_REGISTER(&launch_ops)
