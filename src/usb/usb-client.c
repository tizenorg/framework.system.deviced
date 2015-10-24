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
#include <bundle.h>
#include <eventsystem.h>

#include "usb-client.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"
#include "extcon/extcon.h"
#include "apps/apps.h"

#define USB_POPUP_NAME "usb-syspopup"

#ifndef SETTING_USB_CONNECTION_MODE_CHARGING_ONLY
#define SETTING_USB_CONNECTION_MODE_CHARGING_ONLY 0
#endif

#define USB_STATE_PATH_PLATFORM "/sys/devices/platform/jack/usb_online"
#define USB_STATE_PATH_SWITCH   "/sys/devices/virtual/switch/usb_cable/state"

enum usb_connection {
	USB_DISCONNECTED = 0,
	USB_CONNECTED    = 1,
};

static bool client_mode;
static char *driver_version;
static bool wait_configured;
static unsigned int noti_id;

static struct extcon_ops usbclient_extcon_ops;

void launch_syspopup(char *str)
{
	launch_system_app(APP_DEFAULT,
			2, APP_KEY_TYPE, str);
}

void show_noti(void)
{
	int ret;

	if (noti_id)
		return;

	if (media_device_state() == MEDIA_DEVICE_OFF)
		return;

	ret = dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_USB,
			POPUP_INTERFACE_USB,
			"MediaDeviceNotiOn", NULL, NULL);
	pm_change_internal(getpid(), LCD_NORMAL);

	if (ret > 0)
		noti_id = ret;
}

void terminate_noti(void)
{
	int ret;
	char *param[1];
	char content[8];

	if (!noti_id)
		return;

	snprintf(content, sizeof(content), "%d", noti_id);
	param[0] = content;
	ret = dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_USB,
			POPUP_INTERFACE_USB,
			"MediaDeviceNotiOff", "i", param);
	pm_change_internal(getpid(), LCD_NORMAL);

	if (ret == 0)
		noti_id = ret;
}

bool get_wait_configured(void)
{
	return wait_configured;
}

int get_debug_mode(void)
{
	int debug;
	if (vconf_get_bool(VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL, &debug) != 0)
		return 1; /* 0 means debug mode is on */

	return debug;
}

int get_default_mode(void)
{
	if (get_debug_mode() == 0)
		return SET_USB_DEFAULT;

	return SET_USB_SDB;
}

static void usb_state_send_system_event(int state)
{
	bundle *b;
	const char *str;

	if (state == USB_STATE_DISCONNECTED)
		str = EVT_VAL_USB_DISCONNECTED;
	else if (state == USB_STATE_CONNECTED)
		str = EVT_VAL_USB_CONNECTED;
	else if (state == USB_STATE_AVAILABLE)
		str = EVT_VAL_USB_AVAILABLE;
	else
		return;

	_D("system_event (%s)", str);

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_USB_STATUS, str);
	eventsystem_send_system_event(SYS_EVENT_USB_STATUS, b);
	bundle_free(b);
}

void update_usb_state(enum usb_state state)
{
	int ret;

	usb_state_send_system_event(state);
	ret = vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS, state);
	if (ret == 0)
		send_msg_usb_state_changed();
	else
		_E("Failed to set usb state(%d)", ret);
}

int update_current_usb_mode(int mode)
{
	/*************************************************/
	/* TODO: This legacy vconf key should be removed */
	/* The legacy vconf key is used by mtp and OSP   */
	int legacy, ret;

	switch (mode) {
	case SET_USB_DEFAULT:
	case SET_USB_SDB:
	case SET_USB_SDB_DIAG:
		legacy = SETTING_USB_SAMSUNG_KIES;
		break;
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_DIAG:
	case SET_USB_RNDIS_SDB:
		legacy = SETTING_USB_DEBUG_MODE;
		break;
	case SET_USB_RNDIS_TETHERING:
		legacy = SETTING_USB_TETHERING_MODE;
		break;
	case SET_USB_NONE:
	default:
		legacy = SETTING_USB_NONE_MODE;
		break;
	}

	if (vconf_set_int(VCONFKEY_SETAPPL_USB_MODE_INT, legacy) != 0)
		_E("Failed to set legacy vconf key for current usb mode");
	/****************************************************/

	ret = vconf_set_int(VCONFKEY_USB_CUR_MODE, mode);
	if (ret == 0) {
		send_msg_usb_mode_changed();
		send_msg_usb_state_changed();
	}
	return ret;
}

int get_current_usb_logical_state(void)
{
	int value;

	if (vconf_get_int(VCONFKEY_SYSMAN_USB_STATUS, &value) != 0)
		return -ENOMEM;

	return value;
}

int get_current_usb_physical_state(void)
{
	int state;

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ONLINE, &state) != 0)
		return -ENOMEM;

	return state;
}

int get_current_usb_mode(void)
{
	int ret;
	int mode;

	ret = vconf_get_int(VCONFKEY_USB_CUR_MODE, &mode);
	if (ret != 0)
		return -ENOMEM;

	return mode;
}

int get_selected_usb_mode(void)
{
	int ret;
	int mode;

	ret = vconf_get_int(VCONFKEY_USB_SEL_MODE, &mode);
	if (ret != 0)
		return -ENOMEM;

	return mode;
}

bool charge_only_mode(bool update)
{
	return false;
}

int change_selected_usb_mode(int mode)
{
	if (mode <= SET_USB_NONE)
		return -EINVAL;
	return vconf_set_int(VCONFKEY_USB_SEL_MODE, mode);
}

static void reconfigure_boot_usb_mode(void)
{
	int prev, now;

	prev = get_selected_usb_mode();

	switch (prev) {
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_TETHERING:
		now = SET_USB_DEFAULT;
		break;
	case SET_USB_RNDIS_DIAG:
		now = SET_USB_SDB_DIAG;
		break;
	case SET_USB_RNDIS_SDB:
		now = SET_USB_SDB;
		break;
	default:
		return;
	}

	if (change_selected_usb_mode(now) != 0)
		_E("Failed to set selected usb mode");
}

static int hotspot_mode_changed(void *data)
{
	tethering_status_changed();
	return 0;
}

static int notify_vconf_keys(void)
{
	int ret;

	ret = vconf_notify_key_changed(
			VCONFKEY_USB_SEL_MODE,
			client_mode_changed, NULL);
	if (ret != 0) {
		_E("FAIL: vconf_notify_key_changed()");
		return ret;
	}

	ret = vconf_notify_key_changed(
			VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL,
			debug_mode_changed, NULL);
	if (ret != 0)
		_E("FAIL: vconf_notify_key_changed()");

	return 0;
}

static int ignore_vconf_keys(void)
{
	int ret;

	ret = vconf_ignore_key_changed(
			VCONFKEY_USB_SEL_MODE,
			client_mode_changed);
	if (ret != 0) {
		_E("FAIL: vconf_ignore_key_changed()");
		return ret;
	}

	ret = vconf_ignore_key_changed(
			VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL,
			debug_mode_changed);
	if (ret != 0)
		_E("FAIL: vconf_ignore_key_changed()");

	return 0;
}


static int register_client_handlers(void)
{
	int ret;

	ret = notify_vconf_keys();
	if (ret < 0)
		return ret;

	/* TODO: register other handler (ex. dbus, ... ) */
	register_notifier(DEVICE_NOTIFIER_MOBILE_HOTSPOT_MODE,
	    hotspot_mode_changed);

	return 0;
}

static int unregister_client_handlers(void)
{
	int ret;

	ret = ignore_vconf_keys();
	if (ret < 0)
		return ret;

	/* TODO: unregister other handler (ex. dbus, ... ) */
	unregister_notifier(DEVICE_NOTIFIER_MOBILE_HOTSPOT_MODE,
	    hotspot_mode_changed);

	return 0;
}

static void deinit_client_values(void)
{
	int sel_mode;

	sel_mode = get_selected_usb_mode();
	switch (sel_mode) {
	case SET_USB_RNDIS_TETHERING:
	case SET_USB_RNDIS_DIAG:
		if (change_selected_usb_mode(get_default_mode()) != 0)
			_E("Failed to set selected usb mode");
		break;
	default:
		break;
	}
}

static int init_client(void)
{
	int ret;

	client_mode = true;

	ret = register_client_handlers();
	if (ret < 0)
		return ret;

	return 0;
}

static int deinit_client(void)
{
	int ret;

	client_mode = false;

	ret = unregister_client_handlers();
	if (ret < 0)
		_E("FAIL: unregister_client_handlers()");

	deinit_client_values();

	return 0;
}

void act_usb_connected(void)
{
	int set_val = SET_CONFIGURATION;
	struct usb_common_config *usb_conf = get_usb_common_config();

	update_usb_state(USB_STATE_CONNECTED);

	if (charge_only_mode(false))
		return;

	wait_configured = usb_conf->wait_configured_connected;

	send_msg_usb_config_enabled(USB_CONF_ENABLED);

	if (init_client() < 0)
		_E("FAIL: init_client()");

	if (usb_conf->set_operation_connected)
		set_val |= SET_OPERATION;

	if (usb_conf->set_notification_connected)
		set_val |= SET_NOTIFICATION;

	change_client_setting(set_val);

	pm_lock_internal(getpid(), LCD_OFF, STAY_CUR_STATE, 0);
}

void act_usb_disconnected(void)
{
	int cur_mode;

	wait_configured = false;

	update_usb_state(USB_STATE_DISCONNECTED);
	send_msg_usb_config_enabled(USB_CONF_DISABLED);

	pm_unlock_internal(getpid(), LCD_OFF, STAY_CUR_STATE);

	cur_mode = get_current_usb_mode();
	if (cur_mode > SET_USB_NONE)
		unset_client_mode(cur_mode, false);

	if (deinit_client() < 0)
		_E("FAIL: deinit_client()");
}

static void subsystem_switch_changed(struct udev_device *dev)
{
	const char *name = NULL;
	const char *state = NULL;
	int ret;
	int cur_mode;

	name = udev_device_get_property_value(dev, UDEV_PROP_KEY_SWITCH_NAME);
	if (!name)
		return;

	if (strncmp(name, UDEV_PROP_VALUE_USB_CABLE, UDEV_PROP_VALUE_USB_CABLE_LEN))
		return;

	state = udev_device_get_property_value(dev, UDEV_PROP_KEY_SWITCH_STATE);
	if (!state)
		return;

	/* USB cable disconnected */
	if (!strncmp(state, UDEV_PROP_VALUE_DISCON, UDEV_PROP_VALUE_LEN)) {
		_I("USB cable is disconnected");
		terminate_noti();
		act_usb_disconnected();
		return;
	}

	/* USB cable connected */
	if (!strncmp(state, UDEV_PROP_VALUE_CON_SDP, UDEV_PROP_VALUE_LEN) ||
	    !strncmp(state, UDEV_PROP_VALUE_CON_CDP, UDEV_PROP_VALUE_LEN)) {
		_I("USB cable is connected");
		charge_only_mode(true);
		act_usb_connected();
		return;
	}
}

static void subsystem_usbmode_changed(struct udev_device *dev)
{
	const char *state = NULL;
	struct usb_common_config *usb_conf = get_usb_common_config();

	if (!wait_configured) {
		if (!usb_conf->wait_configured_connected)
			return;
		_I("check again charge only mode");
		if (charge_only_mode(true))
			return;
	}

	wait_configured = false;

	state = udev_device_get_property_value(dev, UDEV_PROP_KEY_USB_STATE);
	if (!state)
		return;

	if (strncmp(state, UDEV_PROP_VALUE_CONFIGURED, UDEV_PROP_VALUE_CONFIGURED_LEN)) {
		_E("state %s", state);
		return;
	}

	_I("Real USB cable is connected(%s)", state);
	change_client_setting(SET_OPERATION | SET_NOTIFICATION);
}

static const struct uevent_handler uhs[] = {
	{ SWITCH_SUBSYSTEM     ,     subsystem_switch_changed     ,    NULL    },
	{ USBMODE_SUBSYSTEM    ,     subsystem_usbmode_changed    ,    NULL    },
};

void usbclient_init_booting_done(void)
{
	int ret, i;
	bool only;
	dd_list *l;
	struct uevent_handler *ops;

	reconfigure_boot_usb_mode();

	for (i = 0; i < ARRAY_SIZE(uhs); i++) {
		ret = register_kernel_uevent_control(&uhs[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	if (register_usb_client_change_request() < 0)
		_E("Failed to register the request to change usb mode");

	if (register_usbclient_dbus_methods() < 0)
		_E("Failed to register dbus handler for usbclient");

	/* set initial charge mode */
	only = charge_only_mode(true);
	_I("charge only mode (%s)", only ? "enabled" : "disabled");
}

static void usbclient_init(void *data)
{
	init_common_config();
	wait_until_booting_done();
}

static void usbclient_exit(void *data)
{
	int i;
	struct uevent_handler *ops;
	dd_list *l;

	for (i = 0; i < ARRAY_SIZE(uhs); i++)
		unregister_kernel_uevent_control(&uhs[i]);
}

static int get_usb_state_direct(void)
{
	FILE *fp;
	char str[2], *path;
	int state;

	if (access(USB_STATE_PATH_PLATFORM, F_OK) == 0)
		path = USB_STATE_PATH_PLATFORM;
	else if (access(USB_STATE_PATH_SWITCH, F_OK) == 0)
		path = USB_STATE_PATH_SWITCH;
	else
		return -ENOENT;

	fp = fopen(path, "r");
	if (!fp) {
		_E("Cannot open usb state node");
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

static int extcon_usb_update(int status)
{
	int ret, val = -1;
	static int prev = -1;

	if (status < 0) {
		ret = device_get_property(DEVICE_TYPE_EXTCON,
				PROP_EXTCON_USB_ONLINE, &val);
		if (ret < 0 || val < 0)
			val = get_usb_state_direct();

		status = val;
	}

	if (prev == status)
		return 0;

	prev = status;

	_I("jack - usb changed %d", status);

	switch (status) {
	case USB_CONNECTED:
		action_usb_connected();
		break;
	case USB_DISCONNECTED:
		action_usb_disconnected();
		break;
	default:
		_E("Invalid status (%d)", status);
		return -EINVAL;
	}

	return 0;
}

static void extcon_usb_init(void *data)
{
	if (extcon_usb_update(-1) < 0)
		_E("Failed to update usb state");
}

static const struct device_ops usbclient_device_ops = {
	.name     = "usbclient",
	.init     = usbclient_init,
	.exit     = usbclient_exit,
	.start    = control_start,
	.stop     = control_stop,
	.status   = control_status,
};

DEVICE_OPS_REGISTER(&usbclient_device_ops)

static struct extcon_ops usbclient_extcon_ops = {
	.name   = EXTCON_CABLE_USB,
	.init   = extcon_usb_init,
	.update = extcon_usb_update,
};

EXTCON_OPS_REGISTER(usbclient_extcon_ops)
