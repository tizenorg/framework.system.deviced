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


#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <fnmatch.h>
#include "dd-deviced.h"
#include "queue.h"
#include "log.h"
#include "device-notifier.h"
#include "device-handler.h"
#include "device-node.h"
#include "noti.h"
#include "data.h"
#include "predefine.h"
#include "display/poll.h"
#include "devices.h"
#include "hall/hall-handler.h"
#include "udev.h"
#include "common.h"
#include "common.h"
#include "list.h"
#include "proc/proc-handler.h"
#include "edbus-handler.h"
#include "emulator.h"
#include "devices.h"
#include "power-supply.h"
#include "display/setting.h"
#include "display/core.h"

#define PREDEF_EARJACKCON		"earjack_predef_internal"
#define PREDEF_DEVICE_CHANGED		"device_changed"
#define PREDEF_POWER_CHANGED		POWER_SUBSYSTEM
#define PREDEF_UDEV_CONTROL		UDEV

#define TVOUT_X_BIN			"/usr/bin/xberc"
#define TVOUT_FLAG			0x00000001

#define MOVINAND_MOUNT_POINT		"/opt/media"
#define BUFF_MAX		255
#define SYS_CLASS_INPUT		"/sys/class/input"

#ifdef MICRO_DD
#define USB_STATE_PATH "/sys/devices/platform/jack/usb_online"
#else
#define USB_STATE_PATH "/sys/devices/virtual/switch/usb_cable/state"
#endif

#define HDMI_NOT_SUPPORTED	(-1)
#ifdef ENABLE_EDBUS_USE
#include <E_DBus.h>
static E_DBus_Connection *conn;
#endif				/* ENABLE_EDBUS_USE */

struct input_event {
	long dummy[2];
	unsigned short type;
	unsigned short code;
	int value;
};

enum snd_jack_types {
	SND_JACK_HEADPHONE = 0x0001,
	SND_JACK_MICROPHONE = 0x0002,
	SND_JACK_HEADSET = SND_JACK_HEADPHONE | SND_JACK_MICROPHONE,
	SND_JACK_LINEOUT = 0x0004,
	SND_JACK_MECHANICAL = 0x0008,	/* If detected separately */
	SND_JACK_VIDEOOUT = 0x0010,
	SND_JACK_AVOUT = SND_JACK_LINEOUT | SND_JACK_VIDEOOUT,
};

#define CRADLE_APP_NAME		"com.samsung.desk-dock"
#define VIRTUAL_CONTROLLER_APP_NAME		"com.samsung.virtual-controller"

#define CHANGE_ACTION		"change"
#define ENV_FILTER		"CHGDET"
#define USB_NAME		"usb"
#define USB_NAME_LEN		3

#define CHARGER_NAME 		"charger"
#define CHARGER_NAME_LEN	7

#define EARJACK_NAME 		"earjack"
#define EARJACK_NAME_LEN	7

#define EARKEY_NAME 		"earkey"
#define EARKEY_NAME_LEN	6

#define TVOUT_NAME 		"tvout"
#define TVOUT_NAME_LEN		5

#define HDMI_NAME 		"hdmi"
#define HDMI_NAME_LEN		4

#define HDCP_NAME 		"hdcp"
#define HDCP_NAME_LEN		4

#define HDMI_AUDIO_NAME 	"ch_hdmi_audio"
#define HDMI_AUDIO_LEN		13

#define CRADLE_NAME 		"cradle"
#define CRADLE_NAME_LEN	6

#define KEYBOARD_NAME 		"keyboard"
#define KEYBOARD_NAME_LEN	8

#define POWER_SUPPLY_NAME_LEN	12

#define CHARGE_NAME_LEN	17
#define BATTERY_NAME		"battery"
#define BATTERY_NAME_LEN	7
#define CHARGEFULL_NAME	"Full"
#define CHARGEFULL_NAME_LEN	4
#define CHARGENOW_NAME		"Charging"
#define CHARGENOW_NAME_LEN	8
#define DISCHARGE_NAME		"Discharging"
#define DISCHARGE_NAME_LEN	11
#define NOTCHARGE_NAME		"Not charging"
#define NOTCHARGE_NAME_LEN	12
#define OVERHEAT_NAME		"Overheat"
#define OVERHEAT_NAME_LEN	8
#define TEMPCOLD_NAME		"Cold"
#define TEMPCOLD_NAME_LEN	4
#define OVERVOLT_NAME		"Over voltage"
#define OVERVOLT_NAME_LEN	12

#define SWITCH_DEVICE_USB 	"usb_cable"

#define LAUNCH_APP	"launch-app"
#define DESK_DOCK	"desk-dock"

/* Ticker notification */
#define DOCK_CONNECTED    "dock-connected"
#define HDMI_CONNECTED    "hdmi-connected"
#define HDMI_DISCONNECTED "hdmi-disconnected"

#define METHOD_GET_HDMI		"GetHDMI"
#define METHOD_GET_HDCP		"GetHDCP"
#define METHOD_GET_HDMI_AUDIO	"GetHDMIAudio"
#define SIGNAL_HDMI_STATE	"ChangedHDMI"
#define SIGNAL_HDCP_STATE	"ChangedHDCP"
#define SIGNAL_HDMI_AUDIO_STATE	"ChangedHDMIAudio"

#define HDCP_HDMI_VALUE(HDCP, HDMI)	((HDCP << 1) | HDMI)

#define METHOD_GET_CRADLE	"GetCradle"
#define SIGNAL_CRADLE_STATE	"ChangedCradle"

struct ticker_data {
	char *name;
	int type;
};

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static int ss_flags = 0;

static int input_device_number;

/* Uevent */
static struct udev *udev = NULL;
/* Kernel Uevent */
static struct udev_monitor *mon = NULL;
static Ecore_Fd_Handler *ufdh = NULL;
static int ufd = -1;
/* Udev Uevent */
static struct udev_monitor *mon_udev = NULL;
static Ecore_Fd_Handler *ufdh_udev = NULL;
static int ufd_udev = -1;

static dd_list *opt_uevent_list = NULL;
static dd_list *opt_kernel_uevent_list = NULL;
static int hdmi_status = 0;
static const struct device_ops *lowbat_ops;

enum udev_subsystem_type {
	UDEV_HALL_IC,
	UDEV_INPUT,
	UDEV_LCD_EVENT,
	UDEV_PLATFORM,
	UDEV_POWER,
	UDEV_SWITCH,
};

static const struct udev_subsystem {
	const enum udev_subsystem_type type;
	const char *str;
	const char *devtype;
} udev_subsystems[] = {
	{ UDEV_HALL_IC,          HALL_IC_SUBSYSTEM, NULL},
	{ UDEV_INPUT,            INPUT_SUBSYSTEM, NULL },
	{ UDEV_LCD_EVENT,        LCD_EVENT_SUBSYSTEM, NULL },
	{ UDEV_PLATFORM,         PLATFORM_SUBSYSTEM, NULL },
	{ UDEV_POWER,		 POWER_SUBSYSTEM, NULL},
	{ UDEV_SWITCH,		 SWITCH_SUBSYSTEM, NULL },
};

static struct extcon_device {
	const enum extcon_type type;
	const char *str;
	int fd;
	int count;
} extcon_devices[] = {
	{ EXTCON_TA, "/csa/factory/batt_cable_count", 0, 0},
	{ EXTCON_EARJACK, "/csa/factory/earjack_count", 0, 0},
};

extern int battery_power_off_act(void *data);
extern int battery_charge_err_act(void *data);

int extcon_set_count(int index)
{
	int r;
	int ret = 0;
	char buf[BUFF_MAX];

	extcon_devices[index].count++;

	if (extcon_devices[index].fd < 0) {
		_E("cannot open file(%s)", extcon_devices[index].str);
		return -ENOENT;
	}
	lseek(extcon_devices[index].fd, 0, SEEK_SET);
	_I("ext(%d) count %d", index, extcon_devices[index].count);
	snprintf(buf, sizeof(buf), "%d", extcon_devices[index].count);

	r = write(extcon_devices[index].fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;
	return ret;
}

static int extcon_get_count(int index)
{
	int fd;
	int r;
	int ret = 0;
	char buf[BUFF_MAX];

	fd = open(extcon_devices[index].str, O_RDWR);
	if (fd < 0)
		return -ENOENT;

	r = read(fd, buf, BUFF_MAX);
	if ((r >= 0) && (r < BUFF_MAX))
		buf[r] = '\0';
	else
		ret = -EIO;

	if (ret != 0) {
		close(fd);
		return ret;
	}
	extcon_devices[index].fd = fd;
	extcon_devices[index].count = atoi(buf);
	_I("get extcon(%d:%x) count %d",
		index, extcon_devices[index].fd, extcon_devices[index].count);

	return ret;
}

static int extcon_create_count(int index)
{
	int fd;
	int r;
	int ret = 0;
	char buf[BUFF_MAX];
	fd = open(extcon_devices[index].str, O_RDWR | O_CREAT, 0644);
	if (fd < 0) {
		_E("cannot open file(%s)", extcon_devices[index].str);
		return -ENOENT;
	}
	snprintf(buf, sizeof(buf), "%d", extcon_devices[index].count);
	r = write(fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;

	if (ret != 0) {
		close(fd);
		_E("cannot write file(%s)", extcon_devices[index].str);
		return ret;
	}
	extcon_devices[index].fd = fd;
	_I("create extcon(%d:%x) %s",
		index, extcon_devices[index].fd, extcon_devices[index].str);
	return ret;
}

static int extcon_count_init(void)
{
	int i;
	int ret = 0;
	for (i = 0; i < ARRAY_SIZE(extcon_devices); i++) {
		if (extcon_get_count(i) >= 0)
			continue;
		ret = extcon_create_count(i);
		if (ret < 0)
			break;
	}
	return ret;
}

int get_usb_state_direct(void)
{
	FILE *fp;
	char str[2];
	int state;

	fp = fopen(USB_STATE_PATH, "r");
	if (!fp) {
		_E("Cannot open jack node");
		return -ENOMEM;
	}

	if (!fgets(str, sizeof(str), fp)) {
		_E("cannot get string from jack node");
		fclose(fp);
		return -ENOMEM;
	}

	fclose(fp);

	return atoi(str);
}

static void usb_chgdet_cb(void *data)
{
	int val = -1;
	int ret = 0;
	char params[BUFF_MAX];

	if (data == NULL)
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ONLINE, &val);
	else
		val = *(int *)data;
	if (ret == 0) {
		if (val < 0)
			val = get_usb_state_direct();

		_I("jack - usb changed %d",val);
		check_lowbat_charge_device(val);
		if (val==1) {
			battery_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
			_D("usb device notification");
		}
	} else {
		_E("fail to get usb_online status");
	}
}

static void show_ticker_notification(char *text, int queue)
{
	struct ticker_data t_data;
	static const struct device_ops *ticker = NULL;

	if (!ticker) {
		ticker = find_device("ticker");
		if (!ticker)
			return;
	}

	t_data.name = text;
	t_data.type = queue;
	if (ticker->init)
		ticker->init(&t_data);
}

static void launch_cradle(int val)
{
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	if (apps == NULL) {
		apps = find_device("apps");
		if (apps == NULL)
			return;
	}
	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return;
	}
	params->name = LAUNCH_APP;
	params->key = DESK_DOCK;
	_I("%s %s %x", params->name, params->key, params);
	if (val == 0) {
		if (apps->exit)
			apps->exit((void *)params);
	} else {
		show_ticker_notification(DOCK_CONNECTED, 0);
		if (apps->init)
			apps->init((void *)params);
	}
	free(params);

}

static int display_changed(void *data)
{
	enum state_t state = (enum state_t)data;
	int ret, cradle = 0;

	if (battery.charge_now == CHARGER_ABNORMAL && battery.health == HEALTH_BAD) {
		pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);
		return 0;
	}

	if (state != S_NORMAL)
		return 0;

	ret = vconf_get_int(VCONFKEY_SYSMAN_CRADLE_STATUS, &cradle);
	if (ret >= 0 && cradle == DOCK_SOUND) {
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
		_I("sound dock is connected! dim lock is on.");
	}
	if (hdmi_status) {
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
		_I("hdmi is connected! dim lock is on.");
	}
	return 0;
}

static void cradle_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast cradle status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_CRADLE_STATE, "i", arr);
}

static int cradle_cb(void *data)
{
	static int old = 0;
	int val = 0;
	int ret = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;

	if (old == val)
		return old;

	old = val;
	cradle_send_broadcast(old);
	return old;
}

static void cradle_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	pm_change_internal(getpid(), LCD_NORMAL);

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_CRADLE_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}

	_I("jack - cradle changed %d", val);
	cradle_cb((void *)&val);
	if (vconf_set_int(VCONFKEY_SYSMAN_CRADLE_STATUS, val) != 0) {
		_E("failed to set vconf status");
		return;
	}

	if (val == DOCK_SOUND)
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
	else if (val == DOCK_NONE)
		pm_unlock_internal(getpid(), LCD_DIM, PM_SLEEP_MARGIN);

	launch_cradle(val);
}

void sync_cradle_status(void)
{
	int val;
	int status;
	if ((device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_CRADLE_ONLINE, &val) != 0) ||
	    vconf_get_int(VCONFKEY_SYSMAN_CRADLE_STATUS, &status) != 0)
		return;
	if ((val != 0 && status == 0) || (val == 0 && status != 0))
		cradle_chgdet_cb(NULL);
}

static void ta_chgdet_cb(struct main_data *ad)
{
	int val = -1;
	int ret = -1;
	int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
	char params[BUFF_MAX];

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_TA_ONLINE, &val) == 0) {
		_I("jack - ta changed %d",val);
		check_lowbat_charge_device(val);
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, val);
		if (val == 0) {
			ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_INSUSPEND_CHARGING_SUPPORT, &val);
			if (ret != 0)
				val = 0;
			if (val == 0)
				pm_unlock_internal(INTERNAL_LOCK_TA, LCD_OFF, STAY_CUR_STATE);
			device_notify(DEVICE_NOTIFIER_BATTERY_CHARGING, (void*)FALSE);
		} else {
			ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_INSUSPEND_CHARGING_SUPPORT, &val);
			if (ret != 0)
				val = 0;
			if (val == 0)
				pm_lock_internal(INTERNAL_LOCK_TA, LCD_OFF, STAY_CUR_STATE, 0);
			battery_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
			_D("ta device notification");
			device_notify(DEVICE_NOTIFIER_BATTERY_CHARGING, (void*)TRUE);
		}
		sync_cradle_status();
	}
	else
		_E("failed to get ta status");
}

static void earjack_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARJACK_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}
	_I("jack - earjack changed %d", val);
	if (CONNECTED(val)) {
		extcon_set_count(EXTCON_EARJACK);
		predefine_pm_change_state(LCD_NORMAL);
	}
	vconf_set_int(VCONFKEY_SYSMAN_EARJACK, val);
}

static void earkey_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARKEY_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}
	_I("jack - earkey changed %d", val);
	vconf_set_int(VCONFKEY_SYSMAN_EARJACKKEY, val);
}

static void tvout_chgdet_cb(void *data)
{
	_I("jack - tvout changed");
	pm_change_internal(getpid(), LCD_NORMAL);
}

static void hdcp_hdmi_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast hdmi status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_STATE, "i", arr);
}

static int hdcp_hdmi_cb(void *data)
{
	static int old = 0;
	int val = 0;
	int ret = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;
	val = HDCP_HDMI_VALUE(val, hdmi_status);

	if (old == val)
		return old;

	old = val;
	hdcp_hdmi_send_broadcast(old);
	return old;
}

static void hdmi_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	pm_change_internal(getpid(), LCD_NORMAL);
	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_HDMI_SUPPORT, &val) == 0) {
		if (val!=1) {
			_I("target is not support HDMI");
			vconf_set_int(VCONFKEY_SYSMAN_HDMI, HDMI_NOT_SUPPORTED);
			return;
		}
	}

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_HDMI_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}

	_I("jack - hdmi changed %d", val);
	vconf_set_int(VCONFKEY_SYSMAN_HDMI, val);
	hdmi_status = val;
	device_notify(DEVICE_NOTIFIER_HDMI, (void *)val);

	if(val == 1) {
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, STAY_CUR_STATE, 0);
		show_ticker_notification(HDMI_CONNECTED, 0);
	} else {
		show_ticker_notification(HDMI_DISCONNECTED, 0);
		pm_unlock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, PM_SLEEP_MARGIN);
	}
}

static void hdcp_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast hdcp status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDCP_STATE, "i", arr);
}

static int hdcp_chgdet_cb(void *data)
{
	static int old = 0;
	int val = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;
	if (old == val)
		return old;

	old = val;
	hdcp_send_broadcast(old);
	return old;
}

static void hdmi_audio_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast hdmi audio status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_AUDIO_STATE, "i", arr);
}

static int hdmi_audio_chgdet_cb(void *data)
{
	static int old = 0;
	int val = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;
	if (old == val)
		return old;

	old = val;
	hdmi_audio_send_broadcast(old);
	return old;
}

static void keyboard_chgdet_cb(void *data)
{
	int val = -1;
	int ret = 0;

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_KEYBOARD_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			vconf_set_int(VCONFKEY_SYSMAN_SLIDING_KEYBOARD, VCONFKEY_SYSMAN_SLIDING_KEYBOARD_NOT_SUPPORTED);
			return;
		}
	}
	_I("jack - keyboard changed %d", val);
	if(val != 1)
		val = 0;
	vconf_set_int(VCONFKEY_SYSMAN_SLIDING_KEYBOARD, val);
}

static void ums_unmount_cb(void *data)
{
	umount(MOVINAND_MOUNT_POINT);
}

#ifdef ENABLE_EDBUS_USE
static void cb_xxxxx_signaled(void *data, DBusMessage * msg)
{
	char *args;
	DBusError err;
	struct main_data *ad;

	ad = data;

	dbus_error_init(&err);
	if (dbus_message_get_args
	    (msg, &err, DBUS_TYPE_STRING, &args, DBUS_TYPE_INVALID)) {
		if (!strcmp(args, "action")) ;	/* action */
	}

	return;
}
#endif				/* ENABLE_EDBUS_USE */

static void check_capacity_status(const char *env_value)
{
	if (env_value == NULL)
		return;
	battery.capacity = atoi(env_value);
}

static void check_charge_status(const char *env_value)
{
	if (env_value == NULL)
		return;
	if (strncmp(env_value, CHARGEFULL_NAME , CHARGEFULL_NAME_LEN) == 0) {
		battery.charge_full = CHARGING_FULL;
		battery.charge_now = CHARGER_DISCHARGING;
	} else if (strncmp(env_value, CHARGENOW_NAME, CHARGENOW_NAME_LEN) == 0) {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_CHARGING;
	} else if (strncmp(env_value, DISCHARGE_NAME, DISCHARGE_NAME_LEN) == 0) {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_DISCHARGING;
	} else if (strncmp(env_value, NOTCHARGE_NAME, NOTCHARGE_NAME_LEN) == 0) {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_ABNORMAL;
	} else {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_DISCHARGING;
	}
}

static void check_health_status(const char *env_value)
{
	if (env_value == NULL) {
		battery.health = HEALTH_GOOD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_NORMAL;
		return;
	}
	if (strncmp(env_value, OVERHEAT_NAME, OVERHEAT_NAME_LEN) == 0) {
		battery.health = HEALTH_BAD;
		battery.temp = TEMP_HIGH;
		battery.ovp = OVP_NORMAL;
	} else if (strncmp(env_value, TEMPCOLD_NAME, TEMPCOLD_NAME_LEN) == 0) {
		battery.health = HEALTH_BAD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_NORMAL;
	} else if (strncmp(env_value, OVERVOLT_NAME, OVERVOLT_NAME_LEN) == 0) {
		battery.health = HEALTH_GOOD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_ABNORMAL;
	} else {
		battery.health = HEALTH_GOOD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_NORMAL;
	}
}

static void check_online_status(const char *env_value)
{
	if (env_value == NULL)
		return;
	battery.online = atoi(env_value);
}

static void check_present_status(const char *env_value)
{
	if (env_value == NULL) {
		battery.present = PRESENT_NORMAL;
		return;
	}
	battery.present = atoi(env_value);
}

static Eina_Bool uevent_kernel_control_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct udev_device *dev = NULL;
	struct udev_list_entry *list_entry = NULL;
	const char *subsystem = NULL;
	const char *env_name = NULL;
	const char *env_value = NULL;
	const char *devpath;
	const char *devnode;
	const char *action;
	const char *siop_level;
	const char *rear_level;
	dd_list *l;
	void *l_data;
	struct uevent_handler *uh;
	int ret = -1;
	int i, len;

	if ((dev = udev_monitor_receive_device(mon)) == NULL)
		return EINA_TRUE;

	subsystem = udev_device_get_subsystem(dev);

	for (i = 0; i < ARRAY_SIZE(udev_subsystems); i++) {
		len = strlen(udev_subsystems[i].str);
		if (!strncmp(subsystem, udev_subsystems[i].str, len))
			break;
	}

	if (i >= ARRAY_SIZE(udev_subsystems))
		goto out;

	devpath = udev_device_get_devpath(dev);

	switch (udev_subsystems[i].type) {
	case UDEV_HALL_IC:
		if (!strncmp(devpath, HALL_IC_PATH, strlen(HALL_IC_PATH))) {
			notify_action(PREDEF_HALL_IC, 1, HALL_IC_NAME);
			goto out;
		}
		break;
	case UDEV_INPUT:
		/* check new input device */
		if (!fnmatch(INPUT_PATH, devpath, 0)) {
			action = udev_device_get_action(dev);
			devnode = udev_device_get_devnode(dev);
			if (!strcmp(action, UDEV_ADD))
				device_notify(DEVICE_NOTIFIER_INPUT_ADD, (void *)devnode);
			else if (!strcmp(action, UDEV_REMOVE))
				device_notify(DEVICE_NOTIFIER_INPUT_REMOVE, (void *)devnode);
			goto out;
		}
		break;
	case UDEV_LCD_EVENT:
		/* check new esd device */
		if (!fnmatch(LCD_ESD_PATH, devpath, 0)) {
			action = udev_device_get_action(dev);
			if (!strcmp(action, UDEV_CHANGE))
				device_notify(DEVICE_NOTIFIER_LCD_ESD, "ESD");
			goto out;
		}
		break;
	case UDEV_SWITCH:
		env_name = udev_device_get_property_value(dev, "SWITCH_NAME");
		env_value = udev_device_get_property_value(dev, "SWITCH_STATE");
		notify_action(PREDEF_DEVICE_CHANGED, 2, env_name, env_value);
		break;
	case UDEV_PLATFORM:
		if (!fnmatch(THERMISTOR_PATH, devpath, 0)) {
			siop_level = udev_device_get_property_value(dev, "TEMPERATURE");
			rear_level = udev_device_get_property_value(dev, "REAR_TEMPERATURE");
			notify_action(SIOP_CHANGED, 2, siop_level, rear_level);
			goto out;
		}

		env_value = udev_device_get_property_value(dev, ENV_FILTER);
		if (!env_value)
			break;
		notify_action(PREDEF_DEVICE_CHANGED, 1, env_value);
		break;
	case UDEV_POWER:
		udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(dev)) {
			env_name = udev_list_entry_get_name(list_entry);
			if (env_name == NULL)
				continue;
			if (strncmp(env_name, CHARGE_NAME, CHARGE_NAME_LEN) == 0) {
				env_value = udev_list_entry_get_value(list_entry);
				if (env_value == NULL)
					continue;
				if (strncmp(env_value, BATTERY_NAME, BATTERY_NAME_LEN) == 0) {
					ret = 0;
					break;
				}
			}
		}
		if (ret != 0)
			goto out;
		env_value = udev_device_get_property_value(dev, CHARGE_STATUS);
		check_charge_status(env_value);
		env_value = udev_device_get_property_value(dev, CHARGE_ONLINE);
		check_online_status(env_value);
		env_value = udev_device_get_property_value(dev, CHARGE_HEALTH);
		check_health_status(env_value);
		env_value = udev_device_get_property_value(dev, CHARGE_PRESENT);
		check_present_status(env_value);
		env_value = udev_device_get_property_value(dev, CAPACITY);
		check_capacity_status(env_value);
		battery_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
		if (env_value)
			notify_action(PREDEF_DEVICE_CHANGED, 2, subsystem, env_value);
		else
			notify_action(PREDEF_DEVICE_CHANGED, 1, subsystem);
		break;
	}

out:
	DD_LIST_FOREACH(opt_kernel_uevent_list, l, l_data) {
		uh = (struct uevent_handler *)l_data;
		if (strncmp(subsystem, uh->subsystem, strlen(uh->subsystem)))
			continue;
		uh->uevent_func(dev);
	}

	udev_device_unref(dev);
	return EINA_TRUE;
}

static Eina_Bool uevent_udev_control_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct udev_device *dev = NULL;
	dd_list *l;
	void *l_data;
	struct uevent_handler *uh;
	const char *subsystem = NULL;

	dev = udev_monitor_receive_device(mon_udev);
	if (!dev)
		return EINA_TRUE;

	subsystem = udev_device_get_subsystem(dev);
	if (!subsystem) {
		_E("Failed to get subsystem from uevent");
		goto out;
	}

	DD_LIST_FOREACH(opt_uevent_list, l, l_data) {
		uh = (struct uevent_handler *)l_data;
		if (strncmp(subsystem, uh->subsystem, strlen(uh->subsystem)))
			continue;
		uh->uevent_func(dev);
	}

out:
	udev_device_unref(dev);
	return EINA_TRUE;
}

static int uevent_kernel_control_stop(void)
{
	struct udev_device *dev = NULL;

	if (ufdh) {
		ecore_main_fd_handler_del(ufdh);
		ufdh = NULL;
	}
	if (ufd >= 0) {
		close(ufd);
		ufd = -1;
	}
	if (mon) {
		dev = udev_monitor_receive_device(mon);
		if (dev) {
			udev_device_unref(dev);
			dev = NULL;
		}
		udev_monitor_unref(mon);
		mon = NULL;
	}
	if (udev && !mon_udev) {
		udev_unref(udev);
		udev = NULL;
	}
	return 0;
}

static int uevent_udev_control_stop(void)
{
	struct udev_device *dev = NULL;

	if (ufdh_udev) {
		ecore_main_fd_handler_del(ufdh_udev);
		ufdh_udev = NULL;
	}
	if (ufd_udev >= 0) {
		close(ufd_udev);
		ufd_udev = -1;
	}
	if (mon_udev) {
		dev = udev_monitor_receive_device(mon_udev);
		if (dev) {
			udev_device_unref(dev);
			dev = NULL;
		}
		udev_monitor_unref(mon_udev);
		mon_udev = NULL;
	}
	if (udev && !mon) {
		udev_unref(udev);
		udev = NULL;
	}
	return 0;
}

static int uevent_kernel_control_start(void)
{
	int i, ret;

	if (udev && mon) {
		_E("uevent control routine is alreay started");
		return -EINVAL;
	}

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EINVAL;
		}
	}

	mon = udev_monitor_new_from_netlink(udev, UDEV);
	if (mon == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	if (udev_monitor_set_receive_buffer_size(mon, UDEV_MONITOR_SIZE) != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	for (i = 0; i < ARRAY_SIZE(udev_subsystems); i++) {
		ret = udev_monitor_filter_add_match_subsystem_devtype(mon,
			    udev_subsystems[i].str, udev_subsystems[i].devtype);
		if (ret < 0) {
			_E("error apply subsystem filter");
			goto stop;
		}
	}

	ret = udev_monitor_filter_update(mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	ufd = udev_monitor_get_fd(mon);
	if (ufd == -1) {
		_E("error udev_monitor_get_fd");
		goto stop;
	}

	ufdh = ecore_main_fd_handler_add(ufd, ECORE_FD_READ,
			uevent_kernel_control_cb, NULL, NULL, NULL);
	if (!ufdh) {
		_E("error ecore_main_fd_handler_add");
		goto stop;
	}

	if (udev_monitor_enable_receiving(mon) < 0) {
		_E("error unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	uevent_kernel_control_stop();
	return -EINVAL;

}

static int uevent_udev_control_start(void)
{
	int i, ret;

	if (udev && mon_udev) {
		_E("uevent control routine is alreay started");
		return 0;
	}

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EINVAL;
		}
	}

	mon_udev = udev_monitor_new_from_netlink(udev, "udev");
	if (mon_udev == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	if (udev_monitor_set_receive_buffer_size(mon_udev, UDEV_MONITOR_SIZE_LARGE) != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	ufd_udev = udev_monitor_get_fd(mon_udev);
	if (ufd_udev == -1) {
		_E("error udev_monitor_get_fd");
		goto stop;
	}

	ufdh_udev = ecore_main_fd_handler_add(ufd_udev, ECORE_FD_READ,
			uevent_udev_control_cb, NULL, NULL, NULL);
	if (!ufdh_udev) {
		_E("error ecore_main_fd_handler_add");
		goto stop;
	}

	if (udev_monitor_enable_receiving(mon_udev) < 0) {
		_E("error unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	uevent_udev_control_stop();
	return -EINVAL;
}

int register_uevent_control(const struct uevent_handler *uh)
{
	int ret;

	if (!mon || !uh)
		return -EINVAL;

	ret = udev_monitor_filter_add_match_subsystem_devtype(mon_udev,
			uh->subsystem, NULL);
	if (ret < 0) {
		_E("FAIL: udev_monitor_filter_add_match_subsystem_devtype()");
		return -ENOMEM;
	}

	ret = udev_monitor_filter_update(mon_udev);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	DD_LIST_APPEND(opt_uevent_list, uh);

	return 0;
}

void unregister_uevent_control(const struct uevent_handler *uh)
{
	dd_list *l;
	void *data;
	struct uevent_handler *handler;

	DD_LIST_FOREACH(opt_uevent_list, l, data) {
		handler = (struct uevent_handler *)data;
		if (strncmp(handler->subsystem, uh->subsystem, strlen(uh->subsystem)))
			continue;
		if (handler->uevent_func != uh->uevent_func)
			continue;

		DD_LIST_REMOVE(opt_uevent_list, handler);
		free(handler);
		break;
	}
}

int register_kernel_uevent_control(const struct uevent_handler *uh)
{
	int ret;

	if (!mon || !uh)
		return -EINVAL;

	ret = udev_monitor_filter_add_match_subsystem_devtype(mon,
			uh->subsystem, NULL);
	if (ret < 0) {
		_E("FAIL: udev_monitor_filter_add_match_subsystem_devtype()");
		return -ENOMEM;
	}

	ret = udev_monitor_filter_update(mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	DD_LIST_APPEND(opt_kernel_uevent_list, uh);

	return 0;
}

void unregister_kernel_uevent_control(const struct uevent_handler *uh)
{
	dd_list *l;
	void *data;
	struct uevent_handler *handler;

	DD_LIST_FOREACH(opt_kernel_uevent_list, l, data) {
		handler = (struct uevent_handler *)data;
		if (strncmp(handler->subsystem, uh->subsystem, strlen(uh->subsystem)))
			continue;
		if (handler->uevent_func != uh->uevent_func)
			continue;

		DD_LIST_REMOVE(opt_kernel_uevent_list, data);
		break;
	}
}

int uevent_udev_get_path(const char *subsystem, dd_list **list)
{
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices, *dev_list_entry;
	int ret;

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EIO;
		}
	}

	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
		return -EIO;

	ret = udev_enumerate_add_match_subsystem(enumerate, subsystem);
	if (ret < 0)
		return -EIO;

	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0)
		return -EIO;

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		path = udev_list_entry_get_name(dev_list_entry);
		_D("subsystem : %s, path : %s", subsystem, path);
		DD_LIST_APPEND(*list, (void*)path);
	}

	return 0;
}

static int changed_device(int argc, char **argv)
{
	int val = 0;
	int *state = NULL;
	int i;

	for (i = 0 ; i < argc ; i++) {
		if (argv[i] == NULL) {
			_E("param is failed");
			return -EINVAL;
		}
	}

	if (argc ==  2) {
		if (argv[1] == NULL)
			val = 0;
		else
			val = atoi(argv[1]);
		state = &val;
	}

	if (strncmp(argv[0], USB_NAME, USB_NAME_LEN) == 0)
		usb_chgdet_cb((void *)state);
	else if (strncmp(argv[0], EARJACK_NAME, EARJACK_NAME_LEN) == 0)
		earjack_chgdet_cb((void *)state);
	else if (strncmp(argv[0], EARKEY_NAME, EARKEY_NAME_LEN) == 0)
		earkey_chgdet_cb((void *)state);
	else if (strncmp(argv[0], TVOUT_NAME, TVOUT_NAME_LEN) == 0)
		tvout_chgdet_cb((void *)state);
	else if (strncmp(argv[0], HDMI_NAME, HDMI_NAME_LEN) == 0)
		hdmi_chgdet_cb((void *)state);
	else if (strncmp(argv[0], HDCP_NAME, HDCP_NAME_LEN) == 0) {
		hdcp_chgdet_cb((void *)state);
		hdcp_hdmi_cb((void *)state);
	}
	else if (strncmp(argv[0], HDMI_AUDIO_NAME, HDMI_AUDIO_LEN) == 0)
		hdmi_audio_chgdet_cb((void *)state);
	else if (strncmp(argv[0], CRADLE_NAME, CRADLE_NAME_LEN) == 0)
		cradle_chgdet_cb((void *)state);
	else if (strncmp(argv[0], KEYBOARD_NAME, KEYBOARD_NAME_LEN) == 0)
		keyboard_chgdet_cb((void *)state);
	else if (strncmp(argv[0], POWER_SUBSYSTEM, POWER_SUPPLY_NAME_LEN) == 0)
		power_supply((void *)state);

	return 0;
}

static int changed_dev_earjack(int argc, char **argv)
{
	int val;

	/* TODO
	 * if we can recognize which type of jack is changed,
	 * should move following code for TVOUT to a new action function */
	/*
	   if(device_get_property(DEVTYPE_JACK,JACK_PROP_TVOUT_ONLINE,&val) == 0)
	   {
	   if (val == 1 &&
	   vconf_get_int(VCONFKEY_SETAPPL_TVOUT_TVSYSTEM_INT, &val) == 0) {
	   _E("Start TV out %s\n", (val==0)?"NTSC":(val==1)?"PAL":"Unsupported");
	   switch (val) {
	   case 0:              // NTSC
	   launch_after_kill_if_exist(TVOUT_X_BIN, "-connect 2");
	   break;
	   case 1:              // PAL
	   launch_after_kill_if_exist(TVOUT_X_BIN, "-connect 3");
	   break;
	   default:
	   _E("Unsupported TV system type:%d \n", val);
	   return -1;
	   }
	   // UI clone on
	   launch_evenif_exist(TVOUT_X_BIN, "-clone 1");
	   ss_flags |= TVOUT_FLAG;
	   return vconf_set_int(VCONFKEY_SYSMAN_EARJACK, VCONFKEY_SYSMAN_EARJACK_TVOUT);
	   }
	   else {
	   if(val == 1) {
	   _E("TV type is not set. Please set the TV type first.\n");
	   return -1;
	   }
	   if (ss_flags & TVOUT_FLAG) {
	   _E("TV out Jack is disconnected.\n");
	   // UI clone off
	   launch_after_kill_if_exist(TVOUT_X_BIN, "-clone 0");
	   ss_flags &= ~TVOUT_FLAG;
	   return vconf_set_int(VCONFKEY_SYSMAN_EARJACK, VCONFKEY_SYSMAN_EARJACK_REMOVED);
	   }
	   // Current event is not TV out event. Fall through
	   }
	   }
	 */
	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARJACK_ONLINE, &val) == 0) {
		if (CONNECTED(val))
			extcon_set_count(EXTCON_EARJACK);
		return vconf_set_int(VCONFKEY_SYSMAN_EARJACK, val);
	}

	return -1;
}

static DBusMessage *dbus_cradle_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = cradle_cb(NULL);
	_I("cradle %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_hdcp_hdmi_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = hdcp_hdmi_cb(NULL);
	_I("hdmi %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_hdcp_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = hdcp_chgdet_cb(NULL);
	_I("hdcp %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_hdmi_audio_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = hdmi_audio_chgdet_cb(NULL);
	_I("hdmi audio %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_device_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
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

	changed_device(argc, (char **)&argv);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_battery_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[5];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1],
		    DBUS_TYPE_STRING, &argv[2],
		    DBUS_TYPE_STRING, &argv[3],
		    DBUS_TYPE_STRING, &argv[4], DBUS_TYPE_INVALID)) {
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
	check_capacity_status(argv[0]);
	check_charge_status(argv[1]);
	check_health_status(argv[2]);
	check_online_status(argv[3]);
	check_present_status(argv[4]);
	_I("%d %d %d %d %d %d %d %d",
		battery.capacity,
		battery.charge_full,
		battery.charge_now,
		battery.health,
		battery.online,
		battery.ovp,
		battery.present,
		battery.temp);
	battery_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
	notify_action(PREDEF_DEVICE_CHANGED, 2, POWER_SUBSYSTEM, argv[0]);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_udev_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
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

	if (strncmp(argv, "start", strlen("start")) == 0) {
		uevent_kernel_control_start();
		uevent_udev_control_start();
	} else if (strncmp(argv, "stop", strlen("stop")) == 0) {
		uevent_kernel_control_stop();
		uevent_udev_control_stop();
	}

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_DEVICE_CHANGED,"siss",		"i",	dbus_device_handler },
	{ PREDEF_POWER_CHANGED,	"sisssss",	"i",	dbus_battery_handler },
	{ PREDEF_UDEV_CONTROL,	"sis",		"i",	dbus_udev_handler },
	{ METHOD_GET_HDCP,	NULL, 		"i",	dbus_hdcp_handler },
	{ METHOD_GET_HDMI_AUDIO,NULL,		"i",	dbus_hdmi_audio_handler },
	{ METHOD_GET_HDMI,	NULL, 		"i",	dbus_hdcp_hdmi_handler },
	{ METHOD_GET_CRADLE,	NULL, 		"i",	dbus_cradle_handler },
};

static int booting_done(void *data)
{
	static int done = 0;
	int ret;
	int val;

	if (data == NULL)
		return done;
	done = (int)data;
	if (done == 0)
		return done;

	_I("booting done");

	register_action(PREDEF_EARJACKCON, changed_dev_earjack, NULL, NULL);
	register_action(PREDEF_DEVICE_CHANGED, changed_device, NULL, NULL);

	power_supply_timer_stop();
	power_supply_init(NULL);

	if (uevent_kernel_control_start() != 0) {
		_E("fail uevent control init");
		return 0;
	}

	if (uevent_udev_control_start() != 0) {
		_E("fail uevent control init");
		return 0;
	}

	/* set initial state for devices */
	input_device_number = 0;
	cradle_chgdet_cb(NULL);
	keyboard_chgdet_cb(NULL);
	hdmi_chgdet_cb(NULL);

	ret = vconf_get_int(VCONFKEY_SYSMAN_CRADLE_STATUS, &val);
	if (ret == 0 && val != 0)
		launch_cradle(val);
	return done;
}

static int device_change_poweroff(void *data)
{
	uevent_kernel_control_stop();
	uevent_udev_control_stop();
	return 0;
}

static void device_change_init(void *data)
{
	int ret;

	power_supply_timer_start();
	if (extcon_count_init() != 0)
		_E("fail to init extcon files");
	register_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);
	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	/* for simple noti change cb */
	emulator_add_callback("device_usb_chgdet", (void *)usb_chgdet_cb, NULL);
	emulator_add_callback("device_ta_chgdet", (void *)ta_chgdet_cb, data);
	emulator_add_callback("device_earjack_chgdet", (void *)earjack_chgdet_cb, data);
	emulator_add_callback("device_earkey_chgdet", (void *)earkey_chgdet_cb, data);
	emulator_add_callback("device_tvout_chgdet", (void *)tvout_chgdet_cb, data);
	emulator_add_callback("device_hdmi_chgdet", (void *)hdmi_chgdet_cb, data);
	emulator_add_callback("device_cradle_chgdet", (void *)cradle_chgdet_cb, data);
	emulator_add_callback("device_keyboard_chgdet", (void *)keyboard_chgdet_cb, data);

	emulator_add_callback("unmount_ums", (void *)ums_unmount_cb, NULL);

	/* check and set earjack init status */
	changed_dev_earjack(0, NULL);
	/* dbus noti change cb */
#ifdef ENABLE_EDBUS_USE
	e_dbus_init();
	conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!conn)
		_E("check system dbus running!\n");

	e_dbus_signal_handler_add(conn, NULL, "/system/uevent/xxxxx",
				  "system.uevent.xxxxx",
				  "Change", cb_xxxxx_signaled, data);
#endif				/* ENABLE_EDBUS_USE */
}

static void device_change_exit(void *data)
{
	int i;
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);
	for (i = 0; i < ARRAY_SIZE(extcon_devices); i++) {
		if (extcon_devices[i].fd <= 0)
			continue;
		close(extcon_devices[i].fd);
	}

}

static const struct device_ops change_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "device change",
	.init     = device_change_init,
	.exit     = device_change_exit,
};

DEVICE_OPS_REGISTER(&change_device_ops)
