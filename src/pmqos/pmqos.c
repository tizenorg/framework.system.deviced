/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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
#include <limits.h>
#include <Ecore.h>
#include <device-node.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "core/common.h"
#include "core/list.h"
#include "core/device-notifier.h"
#include "powersaver/powersaver.h"
#include "pmqos.h"

#define DEFAULT_PMQOS_TIMER		3000

#define PMQOS_CONF_PATH		"/etc/deviced/pmqos.conf"

struct pmqos_cpu {
	char name[NAME_MAX];
	Ecore_Timer *timer;
};

static dd_list *pmqos_head;

int set_cpu_pmqos(const char *name, int val)
{
	char scenario[100];

	snprintf(scenario, sizeof(scenario), "%s%s", name, (val ? "Lock" : "Unlock"));
	_D("Set pm scenario : %s", scenario);
	device_notify(DEVICE_NOTIFIER_PMQOS, (void *)scenario);
	return device_set_property(DEVICE_TYPE_CPU, PROP_CPU_PM_SCENARIO, (int)scenario);
}

static int pmqos_cpu_cancel(const char *name)
{
	dd_list *elem;
	struct pmqos_cpu *cpu;

	/* Find previous request */
	DD_LIST_FOREACH(pmqos_head, elem, cpu) {
		if (!strcmp(cpu->name, name))
			break;
	}

	/* In case of already end up request */
	if(!cpu) {
		_I("%s request is already canceled", name);
		return 0;
	}

	/* Set cpu unlock */
	set_cpu_pmqos(cpu->name, false);

	/* Delete previous request */
	DD_LIST_REMOVE(pmqos_head, cpu);
	ecore_timer_del(cpu->timer);
	free(cpu);

	return 0;
}

static Eina_Bool pmqos_cpu_timer(void *data)
{
	char *name = (char*)data;
	int ret;

	_I("%s request will be unlocked", name);
	ret = pmqos_cpu_cancel(name);
	if (ret < 0)
		_E("Can not find %s request", name);

	free(name);
	return ECORE_CALLBACK_CANCEL;
}

static int pmqos_cpu_request(const char *name, int val)
{
	dd_list *elem;
	struct pmqos_cpu *cpu;
	Ecore_Timer *timer;
	bool locked = false;

	/* Check valid parameter */
	if (val > DEFAULT_PMQOS_TIMER) {
		_I("The timer value cannot be higher than default time value(%dms)", DEFAULT_PMQOS_TIMER);
		val = DEFAULT_PMQOS_TIMER;
	}

	/* Find previous request */
	DD_LIST_FOREACH(pmqos_head, elem, cpu) {
		if (!strcmp(cpu->name, name)) {
			ecore_timer_reset(cpu->timer);
			locked = true;
			break;
		}
	}

	/* In case of first request */
	if (!cpu) {
		/* Add new timer */
		timer = ecore_timer_add(val/1000.f, pmqos_cpu_timer, (void*)strdup(name));
		if (!timer)
			return -EPERM;

		cpu = malloc(sizeof(struct pmqos_cpu));
		if (!cpu) {
			ecore_timer_del(timer);
			return -ENOMEM;
		}
		snprintf(cpu->name, sizeof(cpu->name), "%s", name);
		cpu->timer = timer;
		DD_LIST_APPEND(pmqos_head, cpu);
	}

	/* Set cpu lock */
	if(!locked) {
		set_cpu_pmqos(cpu->name, true);
	}

	return 0;
}

static int pmqos_powersaving(void *data)
{
	return set_cpu_pmqos("PowerSaving", (int)data);
}

static int pmqos_lowbat(void *data)
{
	return set_cpu_pmqos("LowBattery", (int)data);
}

static int pmqos_emergency(void *data)
{
	return set_cpu_pmqos("Emergency", (int)data);
}

static int pmqos_poweroff(void *data)
{
	return set_cpu_pmqos("PowerOff", (int)data);
}

static int pmqos_ultrapowersaving(void *data)
{
	int mode = (int)data;
	bool on;

	switch (mode) {
	case POWERSAVER_OFF:
	case POWERSAVER_BASIC:
		on = false;
		break;
	case POWERSAVER_ENHANCED:
		on = true;
		break;
	default:
		return -EINVAL;
	}
	return set_cpu_pmqos("UltraPowerSaving", (int)on);
}

static int pmqos_hall(void *data)
{
	return pmqos_cpu_request("LockScreen", (int)data);
}

static DBusMessage *dbus_pmqos_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	const char *member;
	int val, ret;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (val < 0) {
		ret = -EINVAL;
		goto error;
	}

	member = dbus_message_get_member(msg);

	if (val)
		ret = pmqos_cpu_request(member, val);
	else
		ret = pmqos_cpu_cancel(member);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_wifi_pmqos_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	const char *member;
	char name[NAME_MAX];
	int bps, val, ret;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &bps,
				DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (bps < 0 || val < 0) {
		ret = -EINVAL;
		goto error;
	}

	member = dbus_message_get_member(msg);

	/* combine bps and member */
	snprintf(name, sizeof(name), "%s%d", member, bps);

	if (val)
		ret = pmqos_cpu_request(name, val);
	else
		ret = pmqos_cpu_cancel(name);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_getdefaultlocktime(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = DEFAULT_PMQOS_TIMER;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static int get_methods_from_conf(const char *path, struct edbus_method **edbus_methods)
{
	struct edbus_method *methods;
	struct pmqos_scenario scenarios = {0,};
	int i, ret;

	/* get pmqos table from conf */
	ret = get_pmqos_table(path, &scenarios);
	if (ret < 0) {
		/* release scenarios memory */
		release_pmqos_table(&scenarios);
		return ret;
	}

	/* if do not support scenario */
	if (!scenarios.support)
		return 0;

	/* if do not have scenarios */
	if (scenarios.num <= 0)
		return 0;

	/* allocate edbus methods structure */
	methods = malloc(sizeof(struct edbus_method)*scenarios.num);
	if (!methods) {
		_E("failed to allocate methods memory : %s", strerror(errno));
		/* release scenarios memory */
		release_pmqos_table(&scenarios);
		return -errno;
	}

	/* set edbus_methods structure */
	for (i = 0; i < scenarios.num; ++i) {

		/* if this scenario does not support */
		if (!scenarios.list[i].support) {
			_I("do not support [%s] scenario", scenarios.list[i].name);
			continue;
		}

		methods[i].member = scenarios.list[i].name;
		methods[i].signature = "i";
		methods[i].reply_signature = "i";
		methods[i].func = dbus_pmqos_handler;
		_D("support [%s] scenario", scenarios.list[i].name);
	}

	*edbus_methods = methods;
	return scenarios.num;
}

/* Add pmqos name as alphabetically */
static const struct edbus_method edbus_methods[] = {
	{ "AppLaunch",            "i",    "i", dbus_pmqos_handler },
	{ "AppLaunchHome",        "i",    "i", dbus_pmqos_handler },
	{ "BeautyShot",           "i",    "i", dbus_pmqos_handler },
	{ "Browser",              "i",    "i", dbus_pmqos_handler },
	{ "BrowserDash",          "i",    "i", dbus_pmqos_handler },
	{ "BrowserJavaScript",    "i",    "i", dbus_pmqos_handler },
	{ "BrowserLoading",       "i",    "i", dbus_pmqos_handler },
	{ "BrowserScroll",        "i",    "i", dbus_pmqos_handler },
	{ "CallSound",            "i",    "i", dbus_pmqos_handler },
	{ "CameraBurstShot",      "i",    "i", dbus_pmqos_handler },
	{ "CameraCaptureAtRec",   "i",    "i", dbus_pmqos_handler },
	{ "CameraPreview",        "i",    "i", dbus_pmqos_handler },
	{ "CameraSoundAndShot",   "i",    "i", dbus_pmqos_handler },
	{ "ContactSearch",        "i",    "i", dbus_pmqos_handler },
	{ "Emergency",            "i",    "i", dbus_pmqos_handler },
	{ "GalleryScroll",        "i",    "i", dbus_pmqos_handler },
	{ "GalleryRotation",      "i",    "i", dbus_pmqos_handler },
	{ "GetDefaultLockTime",  NULL,    "i", dbus_getdefaultlocktime },
	{ "GpsSerialCno",         "i",    "i", dbus_pmqos_handler },
	{ "GpuBoost",             "i",    "i", dbus_pmqos_handler },
	{ "GpuWakeup",            "i",    "i", dbus_pmqos_handler },
	{ "HomeScreen",           "i",    "i", dbus_pmqos_handler },
	{ "ImageViewer",          "i",    "i", dbus_pmqos_handler },
	{ "IMEInput",             "i",    "i", dbus_pmqos_handler },
	{ "LockScreen",           "i",    "i", dbus_pmqos_handler },
	{ "LowBattery",           "i",    "i", dbus_pmqos_handler },
	{ "MtpSendFile",          "i",    "i", dbus_pmqos_handler },
	{ "MusicPlayLcdOn",       "i",    "i", dbus_pmqos_handler },
	{ "PowerSaving",          "i",    "i", dbus_pmqos_handler },
	{ "ProcessCrashed",       "i",    "i", dbus_pmqos_handler },
	{ "ReservedMode",         "i",    "i", dbus_pmqos_handler },
	{ "ScreenMirroring",      "i",    "i", dbus_pmqos_handler },
	{ "SmemoZoom",            "i",    "i", dbus_pmqos_handler },
	{ "SVoice",               "i",    "i", dbus_pmqos_handler },
	{ "WebappLaunch",         "i",    "i", dbus_pmqos_handler },
	{ "WifiThroughput",      "ii",    "i", dbus_wifi_pmqos_handler },
	{ "PowerOff",             "i",    "i", dbus_pmqos_handler },
	{ "WebAppDrag",           "i",    "i", dbus_pmqos_handler },
	{ "WebAppFlick",          "i",    "i", dbus_pmqos_handler },
	{ "SensorWakeup",          "i",    "i", dbus_pmqos_handler },
};

static int booting_done(void *data)
{
	static int done = 0;
	struct edbus_method *methods = NULL;
	int ret, size;

	if (data == NULL)
		goto out;
	done = (int)data;
	if (!done)
		goto out;
	_I("booting done");
	/* register edbus methods */
	ret = register_edbus_method(DEVICED_PATH_PMQOS, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	/* get methods from config file */
	size = get_methods_from_conf(PMQOS_CONF_PATH, &methods);
	if (size < 0)
		_E("failed to load configuration file(%s)", PMQOS_CONF_PATH);

	/* register edbus methods for pmqos */
	if (methods) {
		ret = register_edbus_method(DEVICED_PATH_PMQOS, methods, size);
		if (ret < 0)
			_E("fail to init edbus method from conf(%d)", ret);
		free(methods);
	}

	/* register notifier for each event */
	register_notifier(DEVICE_NOTIFIER_PMQOS_POWERSAVING, pmqos_powersaving);
	register_notifier(DEVICE_NOTIFIER_PMQOS_LOWBAT, pmqos_lowbat);
	register_notifier(DEVICE_NOTIFIER_PMQOS_EMERGENCY, pmqos_emergency);
	register_notifier(DEVICE_NOTIFIER_PMQOS_POWEROFF, pmqos_poweroff);
	register_notifier(DEVICE_NOTIFIER_PMQOS_ULTRAPOWERSAVING,
	    pmqos_ultrapowersaving);
	register_notifier(DEVICE_NOTIFIER_PMQOS_HALL, pmqos_hall);

out:
	return done;
}

static void pmqos_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static void pmqos_exit(void *data)
{
	/* unregister notifier for each event */
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_POWERSAVING, pmqos_powersaving);
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_LOWBAT, pmqos_lowbat);
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_EMERGENCY, pmqos_emergency);
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_POWEROFF, pmqos_poweroff);
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_ULTRAPOWERSAVING,
	    pmqos_ultrapowersaving);
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_HALL, pmqos_hall);
}

static const struct device_ops pmqos_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "pmqos",
	.init     = pmqos_init,
	.exit     = pmqos_exit,
};

DEVICE_OPS_REGISTER(&pmqos_device_ops)
