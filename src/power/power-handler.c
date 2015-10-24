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
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <vconf.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <vconf.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include <device-node.h>
#include <bundle.h>
#include <eventsystem.h>

#include "dd-deviced.h"
#include "core/log.h"
#include "core/launch.h"
#include "core/device-notifier.h"
#include "core/common.h"
#include "core/devices.h"
#include "proc/proc-handler.h"
#include "display/poll.h"
#include "display/setting.h"
#include "core/edbus-handler.h"
#include "core/config-parser.h"
#include "display/core.h"
#include "power-handler.h"
#include "extcon/hall.h"
#include "apps/apps.h"

#define SIGNAL_NAME_POWEROFF_POPUP	"poweroffpopup"
#define SIGNAL_BOOTING_DONE		"BootingDone"
#define SIGNAL_EARLY_BOOTING_DONE		"EarlyBootingDone"

#define POWEROFF_NOTI_NAME		"power_off_start"
#define POWEROFF_DURATION		2
#define MAX_RETRY			2

#define SIGNAL_POWEROFF_STATE	"ChangeState"

#define POWEROFF_POPUP_NAME	"poweroff-syspopup"
#define UMOUNT_RW_PATH "/opt/usr"

#define POWER_CONF_PATH			"/etc/deviced/power.conf"

static void poweroff_control_cb(keynode_t *in_key, void *data);

struct power_config {
	int check_umount;
};

static struct power_config power_conf;
static struct timeval tv_start_poweroff;

static int power_off;
static const struct device_ops *telephony;
static const struct device_ops *hall_ic;

static void telephony_init(void)
{
	FIND_DEVICE_VOID(telephony, "telephony");
	_I("telephony (%d)", telephony);
}

static void telephony_start(void)
{
	telephony_init();
	device_start(telephony);
}

static void telephony_stop(void)
{
	device_stop(telephony);
}

static int telephony_exit(void *data)
{
	int ret;

	ret = device_exit(telephony, data);
	return ret;
}

static void system_shutdown_send_system_event(void)
{
	bundle *b;
	const char *str = EVT_KEY_SYSTEM_SHUTDOWN;

	b = bundle_create();
	bundle_add_str(b, EVT_VAL_SYSTEM_SHUTDOWN_TRUE, str);
	eventsystem_send_system_event(SYS_EVENT_SYSTEM_SHUTDOWN, b);
	bundle_free(b);
}

static void poweroff_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast poweroff %d", status);

	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_POWEROFF, DEVICED_INTERFACE_POWEROFF,
			SIGNAL_POWEROFF_STATE, "i", arr);
}

static void poweroff_stop_systemd_service(void)
{
	char buf[256];
	_D("systemd service stop");
	umount2("/sys/fs/cgroup", MNT_FORCE|MNT_DETACH);
}

static int stop_systemd_journald(void)
{
	int ret;

	ret = deviced_systemd_stop_unit("systemd-journald.socket");
	if (ret < 0) {
		_E("failed to stop 'systemd-journald.socket'");
		return ret;
	}

	ret |= deviced_systemd_stop_unit("systemd-journald.service");
	if (ret < 0) {
		_E("failed to stop 'systemd-journald.service'");
		return ret;
	}

	return 0;
}

static int hall_ic_status(void)
{
	int ret;

	ret = device_get_status(hall_ic);
	if (ret < 0)
		return HALL_IC_OPENED;
	return ret;
}

/* umount usr data partition */
static void unmount_rw_partition()
{
	int retry = 0;
	struct timespec time = {0,};

	sync();
	if (power_conf.check_umount)
		if (!mount_check(UMOUNT_RW_PATH))
			return;
	while (1) {
		switch (retry++) {
		case 0:
			/* Second, kill app with SIGTERM */
			_I("Kill app with SIGTERM");
			terminate_process(UMOUNT_RW_PATH, false);
			time.tv_nsec = 700 * NANO_SECOND_MULTIPLIER;
			nanosleep(&time, NULL);
			break;
		case 1:
			/* Last time, kill app with SIGKILL */
			_I("Kill app with SIGKILL");
			terminate_process(UMOUNT_RW_PATH, true);
			time.tv_nsec = 300 * NANO_SECOND_MULTIPLIER;
			nanosleep(&time, NULL);
			break;
		default:
			if (umount2(UMOUNT_RW_PATH, 0) != 0) {
				_I("Failed to unmount %s", UMOUNT_RW_PATH);
				return;
			}
			_I("%s unmounted successfully", UMOUNT_RW_PATH);
			return;
		}
		if (umount2(UMOUNT_RW_PATH, MNT_DETACH) == 0) {
			_I("%s unmounted successfully", UMOUNT_RW_PATH);
			return;
		}
	}
}

static void powerdown(void)
{
	static int wait;
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	int check_duration = 0;
	char *buf;
	struct timespec time = {0, 100 * NANO_SECOND_MULTIPLIER};

	/* if this fails, that's OK */
	stop_systemd_journald();
	telephony_stop();
	sync();

	buf = getenv("PWROFF_DUR");
	if (buf != NULL && strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60)
		poweroff_duration = POWEROFF_DURATION;
	gettimeofday(&now, NULL);
	check_duration = now.tv_sec - tv_start_poweroff.tv_sec;
	while (check_duration < poweroff_duration) {
		if (wait == 0) {
			_I("wait poweroff %d %d", check_duration, poweroff_duration);
			wait = 1;
		}
		nanosleep(&time, NULL);
		gettimeofday(&now, NULL);
		check_duration = now.tv_sec - tv_start_poweroff.tv_sec;
		if (check_duration < 0)
			break;
	}
#ifndef EMULATOR
	unmount_rw_partition();
#endif
}

void powerdown_ap(void *data)
{
	_I("Power off");
	powerdown();
	reboot(RB_POWER_OFF);
}

void restart_ap(int mode)
{
	_I("Restart %d", mode);
	powerdown();
	if (mode == SYSTEMD_STOP_POWER_RESTART_RECOVERY)
		launch_evenif_exist("/usr/sbin/reboot", "recovery");
	else if (mode == SYSTEMD_STOP_POWER_RESTART_FOTA)
		launch_evenif_exist("/usr/sbin/reboot", "fota");
	else
		reboot(RB_AUTOBOOT);
}

static void poweroff_start_animation(void)
{
	char params[128];
	device_notify(DEVICE_NOTIFIER_POWEROFF_HAPTIC, NULL);
	snprintf(params, sizeof(params), "/usr/bin/boot-animation --stop --clear");
	launch_app_cmd_with_nice(params, -20);
	gettimeofday(&tv_start_poweroff, NULL);
	launch_evenif_exist("/usr/bin/sound_server", "--poweroff");
}

static int poweroff(void)
{
	static const struct device_ops *display_device_ops;
	int ret;

	poweroff_start_animation();
	telephony_start();

	FIND_DEVICE_INT(display_device_ops, "display");

	pm_change_internal(getpid(), LCD_NORMAL);
	display_device_ops->exit(NULL);
	sync();

	ret = telephony_exit(POWER_POWEROFF);

	vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb);
	if (ret < 0) {
		powerdown_ap(NULL);
		return 0;
	}
	return ret;
}

static int pwroff_popup(void)
{
	int val;

	val = hall_ic_status();
	if (val == HALL_IC_CLOSED) {
		_I("cover is closed");
		return 0;
	}

	return launch_system_app(APP_POWEROFF,
			2, APP_KEY_TYPE, APP_POWEROFF);
}

static int power_reboot(int type)
{
	int ret;

	const struct device_ops *display_device_ops = NULL;
	poweroff_start_animation();
	telephony_start();

	FIND_DEVICE_INT(display_device_ops, "display");

	pm_change_internal(getpid(), LCD_NORMAL);
	display_device_ops->exit(NULL);
	sync();

	if (type == SYSTEMD_STOP_POWER_RESTART_RECOVERY)
		ret = telephony_exit(POWER_RECOVERY);
	else if (type == SYSTEMD_STOP_POWER_RESTART_FOTA)
		ret = telephony_exit(POWER_FOTA);
	else
		ret = telephony_exit(POWER_REBOOT);

	vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb);
	if (ret < 0) {
		restart_ap(type);
		return 0;
	}
	return ret;
}

static void power_off_execute(int type)
{
	int val;
	int recovery;

	if (power_off == VCONFKEY_SYSMAN_POWER_OFF_DIRECT || power_off == VCONFKEY_SYSMAN_POWER_OFF_RESTART) {
		_D("during power off");
		return;
	}

	telephony_start();

	_I("poweroff %d", type);

	recovery = type;
	val = type;

	if (val == SYSTEMD_STOP_POWER_OFF ||
	    val == SYSTEMD_STOP_POWER_RESTART ||
	    val == SYSTEMD_STOP_POWER_RESTART_RECOVERY ||
	    val == SYSTEMD_STOP_POWER_RESTART_FOTA) {
		pm_lock_internal(INTERNAL_LOCK_POWEROFF, LCD_OFF, STAY_CUR_STATE, 0);

		if (val == SYSTEMD_STOP_POWER_OFF)
			val = VCONFKEY_SYSMAN_POWER_OFF_DIRECT;
		else
			val = VCONFKEY_SYSMAN_POWER_OFF_RESTART;

		poweroff_stop_systemd_service();

		vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb);
		vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);
	}

	power_off = val;
	if (val == VCONFKEY_SYSMAN_POWER_OFF_DIRECT || val == VCONFKEY_SYSMAN_POWER_OFF_RESTART) {
		system_shutdown_send_system_event();
		poweroff_send_broadcast(val);
	}

	switch (val) {
	case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
		device_notify(DEVICE_NOTIFIER_POWEROFF, &val);
		poweroff();
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
		pwroff_popup();
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
		device_notify(DEVICE_NOTIFIER_POWEROFF, &val);
		power_reboot(recovery);
		break;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_POWEROFF, val);
}

static int power_execute(void *data)
{
	int ret = 0;

	if (strncmp(POWER_POWEROFF, (char *)data, POWER_POWEROFF_LEN) == 0)
		power_off_execute(SYSTEMD_STOP_POWER_OFF);
	else if (strncmp(PWROFF_POPUP, (char *)data, PWROFF_POPUP_LEN) == 0)
		ret = pwroff_popup();
	else if (strncmp(POWER_REBOOT, (char *)data, POWER_REBOOT_LEN) == 0)
		power_off_execute(SYSTEMD_STOP_POWER_RESTART);
	else if (strncmp(POWER_RECOVERY, (char *)data, POWER_RECOVERY_LEN) == 0)
		power_off_execute(SYSTEMD_STOP_POWER_RESTART_RECOVERY);
	else if (strncmp(POWER_FOTA, (char *)data, POWER_FOTA_LEN) == 0)
		power_off_execute(SYSTEMD_STOP_POWER_RESTART_FOTA);
	else if (strncmp(INTERNAL_PWROFF, (char *)data, INTERNAL_PWROFF_LEN) == 0)
		powerdown_ap(NULL);
	return ret;
}

static void poweroff_popup_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *str;
	int ret = 0;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_NAME, SIGNAL_NAME_POWEROFF_POPUP) == 0) {
		_E("there is no power off popup signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == 0) {
		_E("there is no message");
		return;
	}

	ret = power_execute(str);
	if (ret == 0) {
		_E("not supported message : %s", str);
		return;
	}
}

static int booting_done(void *data)
{
	static int done;

	if (data == NULL)
		goto out;

	done = *(int *)data;
	telephony_init();
out:
	return done;
}

static void boot_complete_send_system_event(void)
{
	bundle *b;
	const char *str = EVT_VAL_BOOT_COMPLETED_TRUE;

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_BOOT_COMPLETED, str);
	eventsystem_send_system_event(SYS_EVENT_BOOT_COMPLETED, b);
	bundle_free(b);
}

static void booting_done_edbus_signal_handler(void *data, DBusMessage *msg)
{
	int done;

	if (!dbus_message_is_signal(msg, DEVICED_INTERFACE_CORE, SIGNAL_BOOTING_DONE)) {
		_E("there is no bootingdone signal");
		return;
	}

	_I("real booting done, unlock LCD_OFF");
	pm_unlock_internal(INTERNAL_LOCK_BOOTING, LCD_OFF, PM_SLEEP_MARGIN);
	boot_complete_send_system_event();

	done = booting_done(NULL);
	if (done)
		return;

	_I("signal booting done");
	done = TRUE;
	device_notify(DEVICE_NOTIFIER_BOOTING_DONE, &done);
}

static void early_booting_done_edbus_signal_handler(void *data, DBusMessage *msg)
{
	int done = TRUE;

	if (!dbus_message_is_signal(msg, DEVICED_INTERFACE_CORE, SIGNAL_EARLY_BOOTING_DONE)) {
		_E("there is no early bootingdone signal");
		return;
	}
	_I("signal early booting done");
	device_notify(DEVICE_NOTIFIER_EARLY_BOOTING_DONE, &done);
}

static void poweroff_control_cb(keynode_t *in_key, void *data)
{
	int val;
	int ret;
	int recovery;

	telephony_start();

	if (vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &val) != 0)
		return;

	power_off_execute(val);
}

static void reset_resetkey_disable(const char *sender, void *data)
{
	_D("force reset power resetkey disable to zero");
	device_set_property(DEVICE_TYPE_POWER, PROP_POWER_RESETKEY_DISABLE, 0);
}

static DBusMessage *edbus_resetkeydisable(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	const char *sender;
	int val, ret;

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender name");
		ret = -EINVAL;
		goto error;
	}

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &val,
			DBUS_TYPE_INVALID);
	if (!ret) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = device_set_property(DEVICE_TYPE_POWER, PROP_POWER_RESETKEY_DISABLE, val);
	if (ret < 0)
		goto error;

	if (val)
		register_edbus_watch(sender, reset_resetkey_disable, NULL);
	else
		unregister_edbus_watch(sender, reset_resetkey_disable);

	_D("get power resetkey disable %d, %d", val, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static void reset_wakeupkey(const char *sender, void *data)
{
	_D("force reset wakeupkey to zero");
	device_set_property(DEVICE_TYPE_POWER, PROP_POWER_WAKEUP_KEY, 0);
}

static DBusMessage *edbus_set_wakeup_key(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	const char *sender;
	int val, ret;

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender name");
		ret = -EINVAL;
		goto error;
	}

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
		register_edbus_watch(sender, reset_wakeupkey, NULL);
	else
		unregister_edbus_watch(sender, reset_wakeupkey);

	_D("set power wakeup key %d %d", val, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_power_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
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

	telephony_start();

	ret = power_execute(type_str);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static int power_load_config(struct parse_result *result, void *user_data)
{
	struct power_config *conf = (struct power_config *)user_data;

	if (!result)
		return 0;
	if (!result->section || !result->name || !result->value)
		return 0;
	if (MATCH(result->section, "POWER")) {
		if (MATCH(result->name, "UmountCheck"))
			conf->check_umount = atoi(result->value);
	}
	return 0;
}

static DBusMessage *dbus_get_power_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = power_off;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *request_reboot(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *str;
	int ret;

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	_I("reboot command : %s", str);
	ret = power_execute(POWER_REBOOT);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "setresetkeydisable",   "i",   "i", edbus_resetkeydisable },
	{ "SetWakeupKey", "i", "i", edbus_set_wakeup_key },
	{ POWER_REBOOT, "si", "i", dbus_power_handler },
	{ POWER_RECOVERY, "si", "i", dbus_power_handler },
	{ PWROFF_POPUP, "si", "i", dbus_power_handler },
	{ POWER_POWEROFF, "si", "i", dbus_power_handler },
	/* be linked to device_power_reboot() public API. */
	{ "Reboot",      "s", "i", request_reboot },
	/* Add methods here */
};

static const struct edbus_method poweroff_edbus_methods[] = {
	{ "GetStatus", NULL, "i", dbus_get_power_status},
};

static void power_init(void *data)
{
	int bTelReady = 0;
	int ret;

	/*get power configuration file */
	config_parse(POWER_CONF_PATH, power_load_config, &power_conf);
	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_POWER, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
	ret = register_edbus_method(DEVICED_PATH_POWEROFF, poweroff_edbus_methods, ARRAY_SIZE(poweroff_edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
	if (vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb, NULL) < 0)
		_E("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_SYSMAN_POWER_OFF_STATUS);

	register_edbus_signal_handler(DEVICED_OBJECT_PATH, DEVICED_INTERFACE_NAME,
			SIGNAL_NAME_POWEROFF_POPUP,
		    poweroff_popup_edbus_signal_handler);
	register_edbus_signal_handler(DEVICED_PATH_CORE,
		    DEVICED_INTERFACE_CORE,
		    SIGNAL_BOOTING_DONE,
		    booting_done_edbus_signal_handler);
	register_edbus_signal_handler(DEVICED_PATH_CORE,
		    DEVICED_INTERFACE_CORE,
		    SIGNAL_EARLY_BOOTING_DONE,
		    early_booting_done_edbus_signal_handler);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);

	hall_ic = find_device(HALL_IC_NAME);
}

static const struct device_ops power_device_ops = {
	.name     = POWER_OPS_NAME,
	.init     = power_init,
	.execute  = power_execute,
};

DEVICE_OPS_REGISTER(&power_device_ops)
