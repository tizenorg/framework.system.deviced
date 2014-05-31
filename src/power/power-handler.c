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
#include <vconf.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include <device-node.h>
#include "dd-deviced.h"
#include "core/log.h"
#include "core/launch.h"
#include "core/queue.h"
#include "core/device-handler.h"
#include "core/device-notifier.h"
#include "core/predefine.h"
#include "core/data.h"
#include "core/common.h"
#include "core/devices.h"
#include "proc/proc-handler.h"
#include "display/poll.h"
#include "display/setting.h"
#include "core/edbus-handler.h"
#include "display/core.h"
#include "power-handler.h"
#include "hall/hall-handler.h"

#define SIGNAL_NAME_POWEROFF_POPUP	"poweroffpopup"
#define SIGNAL_BOOTING_DONE		"BootingDone"

#define PREDEF_PWROFF_POPUP		"pwroff-popup"
#define PREDEF_INTERNAL_POWEROFF	"internal_poweroff"

#define POWEROFF_NOTI_NAME		"power_off_start"
#define POWEROFF_DURATION		2
#define MAX_RETRY			2

#define SYSTEMD_STOP_POWER_OFF				4

#define SIGNAL_POWEROFF_STATE	"ChangeState"

#define POWEROFF_POPUP_NAME	"poweroff-syspopup"
#define UMOUNT_RW_PATH "/opt/usr"


struct popup_data {
	char *name;
	char *key;
};

static struct timeval tv_start_poweroff;

static int power_off = 0;
static const struct device_ops *telephony;
static const struct device_ops *hall_ic = NULL;
static void telephony_init(void)
{
	if (telephony)
		return;
	telephony = find_device("telephony");
	_I("telephony (%d)", telephony);
}

static void telephony_start(void)
{
	telephony_init();
	if (telephony)
		telephony->start();
}

static void telephony_stop(void)
{
	if (telephony)
		telephony->stop();
}

static int telephony_exit(void *data)
{
	if (!telephony)
		return -EINVAL;
	telephony->exit(data);
	return 0;
}

static void poweroff_popup_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *str;
	int val = 0;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_NAME, SIGNAL_NAME_POWEROFF_POPUP) == 0) {
		_E("there is no power off popup signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == 0) {
		_E("there is no message");
		return;
	}

	if (strncmp(str, PREDEF_PWROFF_POPUP, strlen(PREDEF_PWROFF_POPUP)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_POPUP;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_POWEROFF)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_DIRECT;
	else if (strncmp(str, PREDEF_REBOOT, strlen(PREDEF_REBOOT)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_RESTART;
	else if (strncmp(str, PREDEF_FOTA_REBOOT, strlen(PREDEF_FOTA_REBOOT)) == 0)
		val = SYSTEMD_STOP_POWER_RESTART_FOTA;
	if (val == 0) {
		_E("not supported message : %s", str);
		return;
	}
	vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);
}

static void booting_done_edbus_signal_handler(void *data, DBusMessage *msg)
{
	if (!dbus_message_is_signal(msg, DEVICED_INTERFACE_CORE, SIGNAL_BOOTING_DONE)) {
		_E("there is no bootingdone signal");
		return;
	}

	device_notify(DEVICE_NOTIFIER_BOOTING_DONE, (void *)TRUE);
	telephony_init();
}

static void poweroff_send_broadcast(int status)
{
	static int old = 0;
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
	umount2("/sys/fs/cgroup", MNT_FORCE |MNT_DETACH);
}

static void poweroff_start_animation(void)
{
	char params[128];
	snprintf(params, sizeof(params), "/usr/bin/boot-animation --stop --clear");
	launch_app_cmd_with_nice(params, -20);
	launch_evenif_exist("/usr/bin/sound_server", "--poweroff");
	device_notify(DEVICE_NOTIFIER_POWEROFF_HAPTIC, NULL);
}

static void poweroff_control_cb(keynode_t *in_key, struct main_data *ad)
{
	int val;
	int ret;
	int recovery;

	telephony_start();

	if (vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &val) != 0)
		return;

	recovery = val;

	if (val == SYSTEMD_STOP_POWER_OFF ||
	    val == SYSTEMD_STOP_POWER_RESTART ||
	    val == SYSTEMD_STOP_POWER_RESTART_RECOVERY ||
	    val == SYSTEMD_STOP_POWER_RESTART_FOTA) {
		poweroff_stop_systemd_service();
		if (val == SYSTEMD_STOP_POWER_OFF)
			val = VCONFKEY_SYSMAN_POWER_OFF_DIRECT;
		else
			val = VCONFKEY_SYSMAN_POWER_OFF_RESTART;
		vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void*)poweroff_control_cb);
		vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);
	}

	if (val == VCONFKEY_SYSMAN_POWER_OFF_DIRECT || val == VCONFKEY_SYSMAN_POWER_OFF_RESTART)
		poweroff_send_broadcast(val);

	switch (val) {
	case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
		device_notify(DEVICE_NOTIFIER_POWEROFF, (void *)val);
		notify_action(PREDEF_POWEROFF, 0);
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
		notify_action(PREDEF_PWROFF_POPUP, 0);
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
		device_notify(DEVICE_NOTIFIER_POWEROFF, (void *)val);
		if (recovery == SYSTEMD_STOP_POWER_RESTART_RECOVERY)
			notify_action(PREDEF_RECOVERY, 0);
		else if (recovery == SYSTEMD_STOP_POWER_RESTART_FOTA)
			notify_action(PREDEF_FOTA_REBOOT, 0);
		else
			notify_action(PREDEF_REBOOT, 0);
		break;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_POWEROFF, val);
}

/* umount usr data partition */
static void unmount_rw_partition()
{
	int retry = 0;
	sync();
	if (!mount_check(UMOUNT_RW_PATH))
		return;
	while (1) {
		switch (retry++) {
		case 0:
			/* Second, kill app with SIGTERM */
			_I("Kill app with SIGTERM");
			terminate_process(UMOUNT_RW_PATH, false);
			sleep(3);
			break;
		case 1:
			/* Last time, kill app with SIGKILL */
			_I("Kill app with SIGKILL");
			terminate_process(UMOUNT_RW_PATH, true);
			sleep(1);
			break;
		default:
			if (umount2(UMOUNT_RW_PATH, 0) != 0) {
				_I("Failed to unmount %s", UMOUNT_RW_PATH);
				return;
			}
			_I("%s unmounted successfully", UMOUNT_RW_PATH);
			return;
		}
		if (umount2(UMOUNT_RW_PATH, 0) == 0) {
			_I("%s unmounted successfully", UMOUNT_RW_PATH);
			return;
		}
	}
}

static void powerdown(void)
{
	static int wait = 0;
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	int check_duration = 0;
	char *buf;

	if (power_off == 1) {
		_E("during power off");
		return;
	}
	telephony_stop();
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void*)poweroff_control_cb);
	power_off = 1;
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
		usleep(100000);
		gettimeofday(&now, NULL);
		check_duration = now.tv_sec - tv_start_poweroff.tv_sec;
		if (check_duration < 0)
			break;
	}
	unmount_rw_partition();
}

static void restart_by_mode(int mode)
{
	if (mode == SYSTEMD_STOP_POWER_RESTART_RECOVERY)
		launch_evenif_exist("/usr/sbin/reboot", "recovery");
	else if (mode == SYSTEMD_STOP_POWER_RESTART_FOTA)
		launch_evenif_exist("/usr/sbin/reboot", "fota");
	else
		reboot(RB_AUTOBOOT);
}

int internal_poweroff_def_predefine_action(int argc, char **argv)
{
	int ret;
	const struct device_ops *display_device_ops;

	telephony_start();

	display_device_ops = find_device("display");
	if (!display_device_ops) {
		_E("Can't find display_device_ops");
		return -ENODEV;
	}

	display_device_ops->exit(NULL);
	sync();

	gettimeofday(&tv_start_poweroff, NULL);

	ret = telephony_exit(PREDEF_POWEROFF);

	if (ret < 0) {
		powerdown_ap(NULL);
		return 0;
	}
	return ret;
}

int do_poweroff(int argc, char **argv)
{
	return internal_poweroff_def_predefine_action(argc, argv);
}

int poweroff_def_predefine_action(int argc, char **argv)
{
	int retry_count = 0;
	poweroff_start_animation();
	while (retry_count < MAX_RETRY) {
		if (notify_action(PREDEF_INTERNAL_POWEROFF, 0) < 0) {
			_E("failed to request poweroff to deviced");
			retry_count++;
			continue;
		}
		vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void*)poweroff_control_cb);
		return 0;
	}
	return -1;
}

static int hall_ic_status(void)
{
	if (!hall_ic)
		return HALL_IC_OPENED;
	return hall_ic->status();
}

int launching_predefine_action(int argc, char **argv)
{
	struct popup_data *params;
	static const struct device_ops *apps = NULL;
	int val;

	val = hall_ic_status();
	if (val == HALL_IC_CLOSED) {
		_I("cover is closed");
		return 0;
	}
	if (apps == NULL) {
		apps = find_device("apps");
		if (apps == NULL)
			return 0;
	}
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

int restart_def_predefine_action(int argc, char **argv)
{
	int ret;
	int data = 0;

	const struct device_ops *display_device_ops;
	poweroff_start_animation();
	telephony_start();

	display_device_ops = find_device("display");
	if (!display_device_ops) {
		_E("Can't find display_device_ops");
		return -ENODEV;
	}

	pm_change_internal(getpid(), LCD_NORMAL);
	display_device_ops->exit(NULL);
	sync();

	gettimeofday(&tv_start_poweroff, NULL);

	if (argc == 1 && argv)
		data = atoi(argv[0]);
	if (data == SYSTEMD_STOP_POWER_RESTART_RECOVERY)
		ret = telephony_exit(PREDEF_RECOVERY);
	else if (data == SYSTEMD_STOP_POWER_RESTART_FOTA)
		ret = telephony_exit(PREDEF_FOTA_REBOOT);
	else
		ret = telephony_exit(PREDEF_REBOOT);

	if (ret < 0) {
		restart_ap((void *)data);
		return 0;
	}
	return ret;
}

int restart_recovery_def_predefine_action(int argc, char **argv)
{
	int ret;
	char *param[1];
	char status[32];

	snprintf(status, sizeof(status), "%d", SYSTEMD_STOP_POWER_RESTART_RECOVERY);
	param[0] = status;

	return restart_def_predefine_action(1, param);
}

int restart_fota_def_predefine_action(int argc, char **argv)
{
	int ret;
	char *param[1];
	char status[32];

	snprintf(status, sizeof(status), "%d", SYSTEMD_STOP_POWER_RESTART_FOTA);
	param[0] = status;

	return restart_def_predefine_action(1, param);
}

int reset_resetkey_disable(char *name, enum watch_id id)
{
	_D("force reset power resetkey disable to zero");
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_RESETKEY_DISABLE, 0);
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

	ret = device_set_property(DEVICE_TYPE_POWER, PROP_POWER_RESETKEY_DISABLE, val);
	if (ret < 0)
		goto error;

	if (val)
		register_edbus_watch(msg, WATCH_POWER_RESETKEY_DISABLE, reset_resetkey_disable);
	else
		unregister_edbus_watch(msg, WATCH_POWER_RESETKEY_DISABLE);

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

	if(strncmp(type_str, PREDEF_REBOOT, strlen(PREDEF_REBOOT)) == 0)
		restart_def_predefine_action(0, NULL);
	else if(strncmp(type_str, PREDEF_RECOVERY, strlen(PREDEF_RECOVERY)) == 0)
		restart_recovery_def_predefine_action(0, NULL);
	else if(strncmp(type_str, PREDEF_PWROFF_POPUP, strlen(PREDEF_PWROFF_POPUP)) == 0)
		launching_predefine_action(0, NULL);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

void powerdown_ap(void *data)
{
	_I("Power off");
	powerdown();
	reboot(RB_POWER_OFF);
}

void restart_ap(void *data)
{
	_I("Restart %d", (int)data);
	powerdown();
	restart_by_mode((int)data);
}

static const struct edbus_method edbus_methods[] = {
	{ "setresetkeydisable",   "i",   "i", edbus_resetkeydisable },
	{ "SetWakeupKey", "i", "i", edbus_set_wakeup_key },
	{ PREDEF_REBOOT, "si", "i", dbus_power_handler },
	{ PREDEF_RECOVERY, "si", "i", dbus_power_handler },
	{ PREDEF_PWROFF_POPUP, "si", "i", dbus_power_handler },
	/* Add methods here */
};

static void power_init(void *data)
{
	int bTelReady = 0;
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_POWER, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	register_action(PREDEF_POWEROFF,
				     poweroff_def_predefine_action, NULL, NULL);
	register_action(PREDEF_PWROFF_POPUP,
				     launching_predefine_action, NULL, NULL);
	register_action(PREDEF_REBOOT,
				     restart_def_predefine_action, NULL, NULL);
	register_action(PREDEF_RECOVERY,
				     restart_recovery_def_predefine_action, NULL, NULL);
	register_action(PREDEF_FOTA_REBOOT,
				     restart_fota_def_predefine_action, NULL, NULL);

	register_action(PREDEF_INTERNAL_POWEROFF,
				     internal_poweroff_def_predefine_action, NULL, NULL);

	if (vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb, NULL) < 0) {
		_E("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
	}

	register_edbus_signal_handler(DEVICED_OBJECT_PATH, DEVICED_INTERFACE_NAME,
			SIGNAL_NAME_POWEROFF_POPUP,
		    poweroff_popup_edbus_signal_handler);
	register_edbus_signal_handler(DEVICED_PATH_CORE,
		    DEVICED_INTERFACE_CORE,
		    SIGNAL_BOOTING_DONE,
		    booting_done_edbus_signal_handler);
	hall_ic = find_device(HALL_IC_NAME);
}

static const struct device_ops power_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "power",
	.init     = power_init,
};

DEVICE_OPS_REGISTER(&power_device_ops)
