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
#define MILLISECONDS(tv)	((tv.tv_sec)*1000 + (tv.tv_nsec)/1000000)
#define DELTA(a, b)		(MILLISECONDS(a) - MILLISECONDS(b))

static Eina_Bool pmqos_cpu_timer(void *data);

struct pmqos_cpu {
	char name[NAME_MAX];
	int timeout;
};

static dd_list *pmqos_head;
static Ecore_Timer *unlock_timer;
static struct timespec unlock_timer_start_st;
static struct timespec unlock_timer_end_st;
static struct pmqos_cpu unlock_timer_owner = {"NULL", 0};

int set_cpu_pmqos(const char *name, int val)
{
	char scenario[100];

	if (val)
		_D("Set pm scenario : [Lock  ]%s", name);
	else
		_D("Set pm scenario : [Unlock]%s", name);
	snprintf(scenario, sizeof(scenario), "%s%s", name, (val ? "Lock" : "Unlock"));
	device_notify(DEVICE_NOTIFIER_PMQOS, (void *)scenario);
	return device_set_property(DEVICE_TYPE_CPU, PROP_CPU_PM_SCENARIO, (int)scenario);
}

static void pmqos_unlock_timeout_update(void)
{
	dd_list *elem;
	struct pmqos_cpu *cpu;
	int delta = 0;

	clock_gettime(CLOCK_REALTIME, &unlock_timer_end_st);
	delta = DELTA(unlock_timer_end_st, unlock_timer_start_st);

	if (delta <= 0)
		return;

	DD_LIST_FOREACH(pmqos_head, elem, cpu) {
		cpu->timeout -= delta;
		if (cpu->timeout < 0)
			cpu->timeout = 0;
		if (cpu->timeout > 0)
			continue;
		/* Set cpu unlock */
		set_cpu_pmqos(cpu->name, false);
		/* Delete previous request */
	}

	DD_LIST_FOREACH(pmqos_head, elem, cpu) {
		if (cpu->timeout > 0)
			continue;
		DD_LIST_REMOVE(pmqos_head, cpu);
		free(cpu);
		cpu = NULL;
	}
}

static int pmqos_unlock_timer_start(void)
{
	int ret;
	dd_list *elem;
	struct pmqos_cpu *cpu;

	if (unlock_timer) {
		ecore_timer_del(unlock_timer);
		unlock_timer = NULL;
	}

	ret = DD_LIST_LENGTH(pmqos_head);
	if (ret == 0)
		return 0;

	DD_LIST_FOREACH(pmqos_head, elem, cpu) {
		if (cpu->timeout <= 0)
			continue;
		memcpy(&unlock_timer_owner, cpu, sizeof(struct pmqos_cpu));
		clock_gettime(CLOCK_REALTIME, &unlock_timer_start_st);
		unlock_timer = ecore_timer_add(((unlock_timer_owner.timeout)/1000.f),
			pmqos_cpu_timer, NULL);
		if (unlock_timer)
			break;
		_E("fail init pmqos unlock %s %d", cpu->name, cpu->timeout);
		return -EPERM;
	}
	return 0;
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
	/* no cpu */
	if (!cpu)
		return 0;

	/* unlock cpu */
	set_cpu_pmqos(cpu->name, false);
	/* delete cpu */
	DD_LIST_REMOVE(pmqos_head, cpu);
	free(cpu);

	if (strncmp(unlock_timer_owner.name, name, strlen(name)))
		goto out;
	/* undata cpu */
	pmqos_unlock_timeout_update();
	pmqos_unlock_timer_start();
out:
	return 0;
}

static Eina_Bool pmqos_cpu_timer(void *data)
{
	int ret;

	ret = pmqos_cpu_cancel(unlock_timer_owner.name);
	if (ret < 0)
		_E("Can not find %s request", unlock_timer_owner.name);

	return ECORE_CALLBACK_CANCEL;
}

static int compare_timeout(const void *a, const void *b)
{
	const struct pmqos_cpu *pmqos_a = (const struct pmqos_cpu *)a;
	const struct pmqos_cpu *pmqos_b = (const struct pmqos_cpu *)b;

	if (!pmqos_a)
		return 1;
	if (!pmqos_b)
		return -1;

	if (pmqos_a->timeout < pmqos_b->timeout)
		return -1;
	else if (pmqos_a->timeout > pmqos_b->timeout)
		return 1;
	return 0;
}

static int pmqos_cpu_request(const char *name, int val)
{
	dd_list *elem;
	struct pmqos_cpu *cpu;
	int found = 0;
	int ret;

	/* Check valid parameter */
	if (val > DEFAULT_PMQOS_TIMER) {
		_I("The timer value cannot be higher than default time value(%dms)", DEFAULT_PMQOS_TIMER);
		val = DEFAULT_PMQOS_TIMER;
	}
	/* find cpu */
	DD_LIST_FOREACH(pmqos_head, elem, cpu) {
		if (!strcmp(cpu->name, name)) {
			cpu->timeout = val;
			found = 1;
			break;
		}
	}
	/* add cpu */
	if (!found) {
		cpu = malloc(sizeof(struct pmqos_cpu));
		if (!cpu)
			return -ENOMEM;

		snprintf(cpu->name, sizeof(cpu->name), "%s", name);
		cpu->timeout = val;
		DD_LIST_APPEND(pmqos_head, cpu);
	}
	/* sort cpu */
	DD_LIST_SORT(pmqos_head, compare_timeout);

	ret = pmqos_unlock_timer_start();
	if (ret < 0)
		return ret;
	/* Set cpu lock */
	set_cpu_pmqos(cpu->name, true);
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
	int mode;
	bool on;

	mode = *(int *)data;

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

static int pmqos_oom(void *data)
{
	return pmqos_cpu_request("OOMBOOST", (int)data);
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
		_E("failed to allocate methods memory : %d", errno);
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
	{ "SensorWakeup",         "i",    "i", dbus_pmqos_handler },
	{ "UgLaunch",             "i",    "i", dbus_pmqos_handler },
	{ "MusicScroll",          "i",    "i", dbus_pmqos_handler },
	{ "FileScroll",           "i",    "i", dbus_pmqos_handler },
	{ "VideoScroll",          "i",    "i", dbus_pmqos_handler },
	{ "EmailScroll",          "i",    "i", dbus_pmqos_handler },
	{ "ContactScroll",        "i",    "i", dbus_pmqos_handler },
	{ "TizenStoreScroll",     "i",    "i", dbus_pmqos_handler },
	{ "CallLogScroll",        "i",    "i", dbus_pmqos_handler },
	{ "MyfilesScroll",        "i",    "i", dbus_pmqos_handler },
};

static int booting_done(void *data)
{
	static int done;
	struct edbus_method *methods = NULL;
	int ret, size;

	if (data == NULL)
		goto out;
	done = (int)data;
	if (!done)
		goto out;
	_I("booting done");

	/* register edbus methods */
	ret = register_edbus_interface_and_method(DEVICED_PATH_PMQOS,
			DEVICED_INTERFACE_PMQOS,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	/* get methods from config file */
	size = get_methods_from_conf(PMQOS_CONF_PATH, &methods);
	if (size < 0)
		_E("failed to load configuration file(%s)", PMQOS_CONF_PATH);

	/* register edbus methods for pmqos */
	if (methods) {
		ret = register_edbus_interface_and_method(DEVICED_PATH_PMQOS,
				DEVICED_INTERFACE_PMQOS,
				methods, size);
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
	register_notifier(DEVICE_NOTIFIER_PMQOS_OOM, pmqos_oom);

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
	unregister_notifier(DEVICE_NOTIFIER_PMQOS_OOM, pmqos_oom);
}

static const struct device_ops pmqos_device_ops = {
	.name     = "pmqos",
	.init     = pmqos_init,
	.exit     = pmqos_exit,
};

DEVICE_OPS_REGISTER(&pmqos_device_ops)
