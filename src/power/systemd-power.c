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


#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <vconf.h>
#include <assert.h>
#include <limits.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include <device-node.h>
#include "core/common.h"
#include "core/device-handler.h"
#include "core/device-notifier.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/launch.h"
#include "core/log.h"
#include "dd-deviced.h"
#include "display/poll.h"
#include "display/setting.h"
#include "hall/hall-handler.h"
#include "power-handler.h"
#include "proc/proc-handler.h"

#define SIGNAL_BOOTING_DONE		"BootingDone"
#define POWEROFF_POPUP_NAME		"poweroff-syspopup"
#define RECOVERY_POWER_OFF		"reboot recovery"

#define SYSTEMD_CHECK_POWER_OFF		15

struct popup_data {
	char *name;
	char *key;
};

static const struct device_ops *hall_ic = NULL;
static Ecore_Timer *systemd_poweroff_timer = NULL;

static int power_execute(void *data);

static Eina_Bool systemd_force_shutdown_cb(void *arg)
{
	char params[128];
	if (systemd_poweroff_timer) {
		ecore_timer_del(systemd_poweroff_timer);
		systemd_poweroff_timer = NULL;
	}
	snprintf(params, sizeof(params), "%s -f", (char*)arg);
	launch_evenif_exist("/usr/bin/systemctl", params);
	return EINA_TRUE;
}

static void start_boot_animation(void)
{
	char params[128];
	snprintf(params, sizeof(params), "--stop --clear");
	launch_evenif_exist("/usr/bin/boot-animation", params);
}

static int systemd_shutdown(const char *arg)
{
	assert(arg);
	systemd_poweroff_timer = ecore_timer_add(SYSTEMD_CHECK_POWER_OFF,
			    systemd_force_shutdown_cb, (void *)arg);
#ifdef MICRO_DD
	start_boot_animation();
	device_notify(DEVICE_NOTIFIER_POWEROFF_HAPTIC, NULL);
#endif
	return launch_evenif_exist("/usr/bin/systemctl", arg);
}

static int poweroff(void)
{
	return poweroff_execute(POWER_POWEROFF);
}

static int pwroff_popup(void)
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
	params->name = POWEROFF_POPUP_NAME;
	apps->init((void *)params);
	free(params);
	return 0;
}

static void poweroff_idler_cb(void *data)
{
	enum poweroff_type val = (int)data;
	int ret;

	device_notify(DEVICE_NOTIFIER_PMQOS_POWEROFF, (void*)1);

	/* TODO for notify. will be removed asap. */
	vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);

	switch (val) {
	case POWER_OFF_DIRECT:
		device_notify(DEVICE_NOTIFIER_POWEROFF,
			      (void *)POWER_OFF_DIRECT);
		ret = systemd_shutdown(PREDEF_POWEROFF);
		if (ret < 0)
			_E("fail to do (%s)", PREDEF_POWEROFF);
		break;
	case POWER_OFF_RESTART:
		device_notify(DEVICE_NOTIFIER_POWEROFF,
			      (void *)POWER_OFF_RESTART);
		ret = systemd_shutdown(POWER_REBOOT);
		if (ret < 0)
			_E("fail to do (%s)", POWER_REBOOT);
		break;
	case POWER_OFF_POPUP:
		power_execute(PWROFF_POPUP);
		break;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_POWEROFF, val);
}

static int power_execute(void *data)
{
	int ret;
	int val;

	if (!data) {
		_E("Invalid parameter : data(NULL)");
		return -EINVAL;
	}

	if (strncmp(POWER_POWEROFF, (char *)data, LEN_POWER_POWEROFF) == 0)
		val = POWER_OFF_DIRECT;
	else if (strncmp(PWROFF_POPUP, (char *)data, LEN_PWROFF_POPUP) == 0)
		val = POWER_OFF_POPUP;
	else if (strncmp(POWER_REBOOT, (char *)data, POWER_REBOOT_LEN) == 0)
		val = POWER_OFF_RESTART;
	else {
		_E("Invalid parameter : data(%s)", (char *)data);
		return -EINVAL;
	}

	ret = add_idle_request(poweroff_idler_cb, (int*)val);
	if (ret < 0) {
		_E("fail to add poweroff idle request : %d", ret);
		return ret;
	}

	return 0;
}

static void booting_done_edbus_signal_handler(void *data, DBusMessage *msg)
{
	if (!dbus_message_is_signal(msg,
				    DEVICED_INTERFACE_CORE,
				    SIGNAL_BOOTING_DONE)) {
		_E("there is no bootingdone signal");
		return;
	}

	device_notify(DEVICE_NOTIFIER_BOOTING_DONE, (void *)TRUE);
}

static int hall_ic_status(void)
{
	int ret;

	ret = device_get_status(hall_ic);
	if (ret < 0)
		return HALL_IC_OPENED;
	return ret;
}

int reset_resetkey_disable(char *name, enum watch_id id)
{
	_D("force reset power resetkey disable to zero");
	return device_set_property(DEVICE_TYPE_POWER,
				   PROP_POWER_RESETKEY_DISABLE,
				   0);
}

static DBusMessage *edbus_resetkeydisable(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &val,
				    DBUS_TYPE_INVALID);
	if (!ret) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = device_set_property(DEVICE_TYPE_POWER,
				  PROP_POWER_RESETKEY_DISABLE,
				  val);
	if (ret < 0)
		goto error;

	if (val)
		register_edbus_watch(msg,
				     WATCH_POWER_RESETKEY_DISABLE,
				     reset_resetkey_disable);
	else
		unregister_edbus_watch(msg,
				       WATCH_POWER_RESETKEY_DISABLE);

	_D("get power resetkey disable %d, %d", val, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static int reset_wakeupkey(char *name, enum watch_id id)
{
	_D("force reset wakeupkey to zero");
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_WAKEUP_KEY, 0);
}

static DBusMessage *edbus_set_wakeup_key(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = dbus_message_get_args(msg, NULL,
				    DBUS_TYPE_INT32, &val,
				    DBUS_TYPE_INVALID);
	if (!ret) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = device_set_property(DEVICE_TYPE_POWER, PROP_POWER_WAKEUP_KEY, val);
	if (ret < 0)
		goto error;

	if (val)
		register_edbus_watch(msg, WATCH_POWER_WAKEUPKEY, reset_wakeupkey);
	else
		unregister_edbus_watch(msg, WATCH_POWER_WAKEUPKEY);

	_D("set power wakeup key %d %d", val, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "setresetkeydisable",	"i",	"i",	edbus_resetkeydisable	},
	{ "SetWakeupKey",	"i",	"i",	edbus_set_wakeup_key	},
	/* Add methods here */
};

static void power_init(void *data)
{
	int bTelReady = 0;
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_POWER,
				    edbus_methods,
				    ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	ret = register_edbus_signal_handler(DEVICED_PATH_CORE,
					    DEVICED_INTERFACE_CORE,
					    SIGNAL_BOOTING_DONE,
					    booting_done_edbus_signal_handler);
	if (ret < 0)
		_E("fail to register handler for signal: %s",
		   SIGNAL_BOOTING_DONE);

	hall_ic = find_device(HALL_IC_NAME);
}

static const struct device_ops power_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "systemd-power",
	.init     = power_init,
	.execute  = power_execute,
};

DEVICE_OPS_REGISTER(&power_device_ops)
