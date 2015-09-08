/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <device-node.h>
#include <sys/un.h>
#include <stdarg.h>
#include <errno.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <fcntl.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "core/udev.h"
#include "hall/hall-handler.h"
#include "power/power-handler.h"

#define COOL_DOWN_DBUS_INTERFACE "org.tizen.trm.siop"
#define COOL_DOWN_DBUS_PATH "/Org/Tizen/Trm/Siop"
#define COOL_DOWN_DBUS_SIGNAL "ChangedCooldownMode"

#define COOL_DOWN_POPUP_NAME		"cooldown-syspopup"

#define SIGNAL_COOL_DOWN_SHUT_DOWN	"ShutDown"
#define SIGNAL_COOL_DOWN_NOTIFICATION	"Notification"
#define SIGNAL_COOL_DOWN_LIMIT_ACTION	"LimitAction"
#define SIGNAL_COOL_DOWN_RELEASE	"Release"

#define METHOD_COOL_DOWN_STATUS		"GetCoolDownStatus"
#define METHOD_COOL_DOWN_CHANGED	"CoolDownChanged"

#define SIGNAL_STRING_MAX_LEN	30

#define COOL_DOWN_LIMIT_ACTION_WAIT	60
#define COOL_DOWN_SHUTDOWN_POPUP_WAIT	60
#define COOL_DOWN_SHUTDOWN_FORCE_WAIT	30

#define VCONFKEY_SYSMAN_COOL_DOWN_MODE "db/private/sysman/cool_down_mode"

enum cool_down_status_type {
	COOL_DOWN_RELEASE	= 0,
	COOL_DOWN_NOTIFICATION	= 1,
	COOL_DOWN_SHUT_DOWN	= 2,
};

enum cool_down_timer_type {
	COOL_DOWN_LIMIT_ACTION_TIMER	= 1,
	COOL_DOWN_POPUP_TIMER 		= 2,
	COOL_DOWN_SHUT_DOWN_TIMER	= 3,
};

enum cool_down_timer_status {
	COOL_DOWN_TIMER_DELETED_ALREADY	= 0,
	COOL_DOWN_TIMER_DELETED_NOW		= 1,
};

struct popup_data {
	char *name;
	char *key;
};

static int cool_down_level = 0;
static const char *cool_down_status = NULL;
static Ecore_Timer *app_limit_timer = NULL;
static Ecore_Timer *shut_down_timer = NULL;
static const struct device_ops *hall_ic = NULL;

static void cool_down_timer_start(int type);
static int cool_down_timer_stop(int type);

static int hall_ic_status(void)
{
	int ret;

	ret = device_get_status(hall_ic);
	if (ret < 0)
		return HALL_IC_OPENED;
	return ret;
}

static int cool_down_popup(char *type)
{
	struct popup_data *params;
	static const struct device_ops *apps = NULL;
	int val;

	val = hall_ic_status();
	if (val == HALL_IC_CLOSED) {
		_I("cover is closed");
		return 0;
	}

	FIND_DEVICE_INT(apps, "apps");

	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return -1;
	}
	params->name = COOL_DOWN_POPUP_NAME;
	params->key = type;
	apps->init((void *)params);
	free(params);
	return 0;
}

static Eina_Bool broadcast_cool_down_limit_action(void *data)
{
	char buf[SIGNAL_STRING_MAX_LEN];
	char *param[1];

	cool_down_timer_stop(COOL_DOWN_LIMIT_ACTION_TIMER);

	cool_down_status = SIGNAL_COOL_DOWN_LIMIT_ACTION;
	snprintf(buf, SIGNAL_STRING_MAX_LEN, "%s", cool_down_status);
	param[0] = buf;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
		METHOD_COOL_DOWN_CHANGED, "s", param);
	_I("%s", SIGNAL_COOL_DOWN_LIMIT_ACTION);
	return EINA_FALSE;
}

static Eina_Bool cool_down_shutdown_popup(void *data)
{
	cool_down_popup(SIGNAL_COOL_DOWN_SHUT_DOWN);
	cool_down_timer_start(COOL_DOWN_SHUT_DOWN_TIMER);
	return EINA_FALSE;
}

static Eina_Bool cool_down_shutdown_force(void *data)
{
	static const struct device_ops *ops;
	cool_down_timer_stop(COOL_DOWN_SHUT_DOWN_TIMER);
	vconf_set_int(VCONFKEY_SYSMAN_COOL_DOWN_MODE, COOL_DOWN_SHUT_DOWN);
	FIND_DEVICE_INT(ops, POWER_OPS_NAME);
	ops->execute(POWER_POWEROFF);
	return EINA_FALSE;
}

static int cool_down_timer_stop(int type)
{
	if (type == COOL_DOWN_NOTIFICATION) {
		if (!app_limit_timer)
			return COOL_DOWN_TIMER_DELETED_ALREADY;
		ecore_timer_del(app_limit_timer);
		app_limit_timer = NULL;
	} else if (type == COOL_DOWN_POPUP_TIMER ||
	    type == COOL_DOWN_SHUT_DOWN_TIMER) {
		if (!shut_down_timer)
			return COOL_DOWN_TIMER_DELETED_ALREADY;
		ecore_timer_del(shut_down_timer);
		shut_down_timer = NULL;
	}
	_D("stop %d", type);
	return COOL_DOWN_TIMER_DELETED_NOW;
}

static void cool_down_timer_start(int type)
{
	cool_down_timer_stop(type);
	if (type == COOL_DOWN_LIMIT_ACTION_TIMER) {
		app_limit_timer = ecore_timer_add(COOL_DOWN_LIMIT_ACTION_WAIT,
				broadcast_cool_down_limit_action, NULL);
		if (!app_limit_timer)
			_E("fail to add timer");
	} else if (type == COOL_DOWN_POPUP_TIMER) {
		shut_down_timer = ecore_timer_add(COOL_DOWN_SHUTDOWN_POPUP_WAIT,
				cool_down_shutdown_popup, NULL);
		if (!shut_down_timer)
			_E("fail to add timer");
	} else if (type == COOL_DOWN_SHUT_DOWN_TIMER) {
		shut_down_timer = ecore_timer_add(COOL_DOWN_SHUTDOWN_FORCE_WAIT,
				cool_down_shutdown_force, NULL);
		if (!shut_down_timer)
			_E("fail to add timer");
	}
	_D("start %d", type);
}

static void broadcast_cool_down_status(int status)
{
	int ret;
	char buf[SIGNAL_STRING_MAX_LEN];
	char *param[1];

	switch(status)
	{
	case COOL_DOWN_RELEASE:
		cool_down_status = SIGNAL_COOL_DOWN_RELEASE;
		cool_down_timer_stop(COOL_DOWN_SHUT_DOWN_TIMER);
		cool_down_timer_stop(COOL_DOWN_POPUP_TIMER);
		cool_down_timer_stop(COOL_DOWN_LIMIT_ACTION_TIMER);
		break;
	case COOL_DOWN_NOTIFICATION:
		cool_down_status = SIGNAL_COOL_DOWN_NOTIFICATION;
		cool_down_timer_stop(COOL_DOWN_SHUT_DOWN_TIMER);
		cool_down_timer_stop(COOL_DOWN_POPUP_TIMER);
		cool_down_timer_start(COOL_DOWN_LIMIT_ACTION_TIMER);
		break;
	case COOL_DOWN_SHUT_DOWN:
		ret = cool_down_timer_stop(COOL_DOWN_LIMIT_ACTION_TIMER);
		if (ret == COOL_DOWN_TIMER_DELETED_NOW)
			broadcast_cool_down_limit_action(NULL);
		cool_down_status = SIGNAL_COOL_DOWN_SHUT_DOWN;
		cool_down_timer_start(COOL_DOWN_POPUP_TIMER);
		break;
	default:
		_E("abnormal value %d", status);
		return;
	}

	snprintf(buf, SIGNAL_STRING_MAX_LEN, "%s", cool_down_status);
	param[0] = buf;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
		METHOD_COOL_DOWN_CHANGED, "s", param);
	device_notify(DEVICE_NOTIFIER_COOL_DOWN, (void *)status);
	_I("%s", cool_down_status);
}

static int cool_down_booting_done(void *data)
{
	static int done = 0;
	int mode;
	int ret;

	if (data == NULL)
		goto out;

	done = (int)data;
	if (cool_down_level) {
		if (cool_down_level == COOL_DOWN_SHUT_DOWN) {
			broadcast_cool_down_status(COOL_DOWN_NOTIFICATION);
			broadcast_cool_down_limit_action(NULL);
			cool_down_timer_start(COOL_DOWN_POPUP_TIMER);
			goto out;
		}
		broadcast_cool_down_status(cool_down_level);
		goto out;
	}
	ret = vconf_get_int(VCONFKEY_SYSMAN_COOL_DOWN_MODE, &mode);
	vconf_set_int(VCONFKEY_SYSMAN_COOL_DOWN_MODE, cool_down_level);
	if (ret == 0 && mode == COOL_DOWN_SHUT_DOWN)
		cool_down_popup(SIGNAL_COOL_DOWN_RELEASE);
out:
	return done;
}

static int cool_down_execute(void *data)
{
	int booting_done;
	static int old = COOL_DOWN_RELEASE;

	cool_down_level = *(int *)data;
	if (old == cool_down_level)
		return 0;
	_I("status %d", cool_down_level);

	booting_done = cool_down_booting_done(NULL);
	if (!booting_done)
		return 0;

	if (old == COOL_DOWN_RELEASE &&
	    cool_down_level == COOL_DOWN_SHUT_DOWN) {
		old = cool_down_level;
		broadcast_cool_down_status(COOL_DOWN_NOTIFICATION);
		broadcast_cool_down_limit_action(NULL);
		cool_down_timer_start(COOL_DOWN_POPUP_TIMER);
		return 0;
	}
	old = cool_down_level;
	broadcast_cool_down_status(cool_down_level);
	return 0;
}

static int changed_device(int level)
{
	int *state = NULL;
	int status = level;

	state = &status;
	cool_down_execute((void *)state);
out:
	return 0;
}

static void cool_down_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int val = 0;

	if (dbus_message_is_signal(msg, COOL_DOWN_DBUS_INTERFACE, COOL_DOWN_DBUS_SIGNAL) == 0) {
		_E("there is no cool down signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID) == 0) {
		_E("there is no message");
		return;
	}
	changed_device(val);
}

static DBusMessage *dbus_get_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char buf[SIGNAL_STRING_MAX_LEN];
	char *param = buf;

	snprintf(buf, SIGNAL_STRING_MAX_LEN, "%s", cool_down_status);
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &param);
	return reply;
}

static DBusMessage *dbus_device_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int val;
	int argc;
	char *type_str;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0 || !argv[0] || !argv[1]){
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	val = atoi(argv[1]);
	changed_device(val);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_COOL_DOWN_STATUS,	NULL,	"s", dbus_get_status },
	{ METHOD_COOL_DOWN_CHANGED,	"siss",	"i", dbus_device_handler },
};

static void cool_down_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
	register_edbus_signal_handler(COOL_DOWN_DBUS_PATH, COOL_DOWN_DBUS_INTERFACE,
			COOL_DOWN_DBUS_SIGNAL,
		    cool_down_edbus_signal_handler);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, cool_down_booting_done);
	hall_ic = find_device(HALL_IC_NAME);
}

static const struct device_ops cool_down_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "cool-down",
	.init     = cool_down_init,
	.execute  = cool_down_execute,
};

DEVICE_OPS_REGISTER(&cool_down_device_ops)
