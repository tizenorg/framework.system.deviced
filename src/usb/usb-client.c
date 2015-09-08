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
#include "usb-client.h"
#include "core/device-handler.h"
#include "core/edbus-handler.h"

#define USB_POPUP_NAME "usb-syspopup"

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static bool client_mode = false;
static char *driver_version = NULL;
static bool wait_configured = false;

void launch_syspopup(char *str)
{
	struct popup_data params;
	static const struct device_ops *apps = NULL;

	FIND_DEVICE_VOID(apps, "apps");
	params.name = USB_POPUP_NAME;
	params.key = POPUP_KEY_CONTENT;
	params.value = str;

	if (apps->init)
		apps->init(&params);
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

int update_usb_state(int state)
{
	int ret;

	ret = vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS, state);
	if (ret == 0)
		send_msg_usb_state_changed();

	return ret;
}

int update_current_usb_mode(int mode)
{
	/*************************************************/
	/* TODO: This legacy vconf key should be removed */
	/* The legacy vconf key is used by mtp and OSP   */
	int legacy, ret;

	switch(mode) {
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

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ONLINE, &state) != 0
			|| (state != 0 && state != 1))
		state = get_usb_state_direct();

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

	ret = vconf_notify_key_changed(
			VCONFKEY_MOBILE_HOTSPOT_MODE,
			tethering_status_changed, NULL);
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

	ret = vconf_ignore_key_changed(
			VCONFKEY_MOBILE_HOTSPOT_MODE,
			tethering_status_changed);
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

	return 0;
}

static int unregister_client_handlers(void)
{
	int ret;

	ret = ignore_vconf_keys();
	if (ret < 0)
		return ret;

	/* TODO: unregister other handler (ex. dbus, ... ) */

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
	wait_configured = false;

	if (vconf_set_int(VCONFKEY_USB_CONFIGURATION_ENABLED, USB_CONF_ENABLED) != 0)
		_E("Failed to set vconf key (%s)", VCONFKEY_USB_CONFIGURATION_ENABLED);

	if (init_client() < 0)
		_E("FAIL: init_client()");

	change_client_setting(SET_CONFIGURATION | SET_OPERATION | SET_NOTIFICATION);

	pm_lock_internal(getpid(), LCD_OFF, STAY_CUR_STATE, 0);
}

static void act_usb_disconnected(void)
{
	int cur_mode;

	wait_configured = false;

	if (vconf_set_int(VCONFKEY_USB_CONFIGURATION_ENABLED, USB_CONF_DISABLED) != 0)
		_E("Failed to set vconf key (%s)", VCONFKEY_USB_CONFIGURATION_ENABLED);

	pm_unlock_internal(getpid(), LCD_OFF, STAY_CUR_STATE);

	cur_mode = get_current_usb_mode();
	if (cur_mode > SET_USB_NONE)
		unset_client_mode(cur_mode, false);

	if (deinit_client() < 0)
		_E("FAIL: deinit_client()");

	if (update_usb_state(VCONFKEY_SYSMAN_USB_DISCONNECTED) < 0)
		_E("FAIL: update_usb_state(%d)", VCONFKEY_SYSMAN_USB_DISCONNECTED);
}

static void subsystem_switch_changed (struct udev_device *dev)
{
	const char *name = NULL;
	const char *state = NULL;
	int ret;
	int cur_mode;

	name = udev_device_get_property_value(dev, UDEV_PROP_KEY_SWITCH_NAME);
	if (!name)
		return;

	if (strncmp(name, UDEV_PROP_VALUE_USB_CABLE, strlen(UDEV_PROP_VALUE_USB_CABLE)))
		return;

	state = udev_device_get_property_value(dev, UDEV_PROP_KEY_SWITCH_STATE);
	if (!state)
		return;

	/* USB cable disconnected */
	if (!strncmp(state, UDEV_PROP_VALUE_DISCON, strlen(UDEV_PROP_VALUE_DISCON))) {
		_I("USB cable is disconnected");
		act_usb_disconnected();
		return;
	}

	/* USB cable connected */
	if (!strncmp(state, UDEV_PROP_VALUE_CON, strlen(UDEV_PROP_VALUE_CON))) {
		_I("USB cable is connected");
		act_usb_connected();
		return;
	}
}

static void subsystem_platform_changed (struct udev_device *dev)
{
	const char *chgdet = NULL;
	int state;
	int ret;

	chgdet = udev_device_get_property_value(dev, UDEV_PROP_KEY_CHGDET);
	if (!chgdet)
		return;

	if (strncmp(chgdet, UDEV_PROP_VALUE_USB, strlen(UDEV_PROP_VALUE_USB)))
		return;

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ONLINE, &state) != 0
			|| (state != 0 && state != 1)) {
		_E("cannot get the usb cable connection status");
		state = get_usb_state_direct();
	}

	/* USB cable disconnected */
	if (state == 0) {
		_I("USB cable is disconnected");
		act_usb_disconnected();
		return;
	}

	/* USB cable connected */
	if (state == 1) {
		_I("USB cable is connected");
		act_usb_connected();
		return;
	}

	_E("USB state is unknown(%d)", state);
}

static void subsystem_usbmode_changed (struct udev_device *dev)
{
	const char *state = NULL;

	if (!wait_configured)
		return;

	wait_configured = false;

	state = udev_device_get_property_value(dev, UDEV_PROP_KEY_USB_STATE);
	if (!state)
		return ;

	if (strncmp(state, UDEV_PROP_VALUE_CONFIGURED, strlen(state)))
		return;

	_I("Real USB cable is connected");
	change_client_setting(SET_OPERATION | SET_NOTIFICATION);
}

const static struct uevent_handler uhs[] = {
	{ SWITCH_SUBSYSTEM     ,     subsystem_switch_changed     ,    NULL    },
	{ USBMODE_SUBSYSTEM    ,     subsystem_usbmode_changed    ,    NULL    },
	{ PLATFORM_SUBSYSTEM   ,     subsystem_platform_changed   ,    NULL    },
};

void usbclient_init_booting_done(void)
{
	int ret, i;

	reconfigure_boot_usb_mode();

	for (i = 0 ; i < ARRAY_SIZE(uhs) ; i++) {
		ret = register_kernel_uevent_control(&uhs[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	if (register_usb_client_change_request() < 0)
		_E("Failed to register the request to change usb mode");

	if (register_usbclient_dbus_methods() < 0)
		_E("Failed to register dbus handler for usbclient");
}

static void usbclient_init(void *data)
{
	wait_until_booting_done();
}

static void usbclient_exit(void *data)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(uhs) ; i++) {
		unregister_kernel_uevent_control(&uhs[i]);
	}
}

static const struct device_ops usbclient_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "usbclient",
	.init     = usbclient_init,
	.exit     = usbclient_exit,
	.start    = control_start,
	.stop     = control_stop,
	.status   = control_status,
};

DEVICE_OPS_REGISTER(&usbclient_device_ops)
