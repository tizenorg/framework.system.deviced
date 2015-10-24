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

#include <vconf.h>
#include <limits.h>
#include "usb-client.h"
#include "core/edbus-handler.h"

#define CHANGE_USB_MODE "ChangeUsbMode"

#define METHOD_GET_STATE      "GetState"
#define METHOD_GET_MODE       "GetMode"
#define SIGNAL_STATE_CHANGED  "StateChanged"
#define SIGNAL_MODE_CHANGED   "ModeChanged"
#define SIGNAL_CONFIG_ENABLED "ConfigEnabled"

#define USB_STATE_MAX   UINT_MAX
#define USB_MODE_MAX    UINT_MAX

enum usbclient_state {
	USBCLIENT_STATE_DISCONNECTED = 0x00,
	USBCLIENT_STATE_CONNECTED    = 0x01,
	USBCLIENT_STATE_AVAILABLE    = 0x02,
};

static void change_usb_client_mode(void *data, DBusMessage *msg)
{
	DBusError err;
	int mode, debug;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_USB, CHANGE_USB_MODE) == 0) {
		_E("The signal is not for changing usb mode");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &mode, DBUS_TYPE_INVALID) == 0) {
		_E("FAIL: dbus_message_get_args");
		goto out;
	}

	switch (mode) {
	case SET_USB_DEFAULT:
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_DIAG:
	case SET_USB_DIAG_RMNET:
		debug = 0;
		break;
	case SET_USB_SDB:
	case SET_USB_SDB_DIAG:
	case SET_USB_RNDIS_SDB:
		debug = 1;
		break;
	default:
		_E("(%d) is unknown usb mode", mode);
		goto out;
	}

	if (vconf_set_int(VCONFKEY_USB_SEL_MODE, mode) != 0)
		_E("Failed to set usb mode (%d)", mode);

	if (vconf_set_bool(VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL, debug) != 0)
		_E("Failed to set usb debug toggle (%d)", debug);

out:
	dbus_error_free(&err);
	return;
}


int register_usb_client_change_request(void)
{
	return register_edbus_signal_handler(DEVICED_PATH_USB,
			DEVICED_INTERFACE_USB,
			CHANGE_USB_MODE, change_usb_client_mode);
}

static unsigned int get_usb_state(void)
{
	unsigned int state = USBCLIENT_STATE_DISCONNECTED;

	if (get_current_usb_physical_state() == 0) {
		state |= USBCLIENT_STATE_DISCONNECTED;
		goto out;
	}

	state |= USBCLIENT_STATE_CONNECTED;

	if (get_current_usb_logical_state() > 0
			&& get_current_usb_mode() > SET_USB_NONE)
		state |= USBCLIENT_STATE_AVAILABLE;

out:
	return state;
}

static unsigned int get_usb_mode(void)
{
	return get_current_usb_gadget_info(get_current_usb_mode());
}

void send_msg_usb_state_changed(void)
{
	char *param[1];
	char text[16];
	unsigned int state;
	static unsigned int prev_state = USB_STATE_MAX;

	state = get_usb_state();
	if (state == prev_state)
		return;
	prev_state = state;

	_I("USB state changed (%u)", state);

	snprintf(text, sizeof(text), "%u", state);
	param[0] = text;

	if (broadcast_edbus_signal(
				DEVICED_PATH_USB,
				DEVICED_INTERFACE_USB,
				SIGNAL_STATE_CHANGED,
				"u", param) < 0)
		_E("Failed to send dbus signal");
}

void send_msg_usb_mode_changed(void)
{
	char *param[1];
	char text[16];
	unsigned int mode;
	static unsigned int prev_mode = USB_MODE_MAX;

	mode = get_usb_mode();
	if (mode == prev_mode)
		return;
	prev_mode = mode;

	snprintf(text, sizeof(text), "%u", mode);
	param[0] = text;

	_I("USB mode changed (%u)", mode);

	if (broadcast_edbus_signal(
				DEVICED_PATH_USB,
				DEVICED_INTERFACE_USB,
				SIGNAL_MODE_CHANGED,
				"u", param) < 0)
		_E("Failed to send dbus signal");
}

void send_msg_usb_config_enabled(int state)
{
	int ret;
	char *param[1];
	char buf[2];

	snprintf(buf, sizeof(buf), "%d", state);
	param[0] = buf;

	_I("USB config enabled (%d)", state);

	ret = broadcast_edbus_signal(DEVICED_PATH_USB,
	    DEVICED_INTERFACE_USB, SIGNAL_CONFIG_ENABLED, "i", param);

	if (ret < 0)
		_E("Failed to send dbus signal");
}

static DBusMessage *get_usb_client_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int state;

	state = get_usb_state();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &state);
	return reply;
}

static DBusMessage *get_usb_client_mode(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int mode;

	mode = get_usb_mode();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &mode);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_GET_STATE    ,  NULL, "u" ,  get_usb_client_state },
	{ METHOD_GET_MODE     ,  NULL, "u" ,  get_usb_client_mode  },
};

int register_usbclient_dbus_methods(void)
{
	return register_edbus_interface_and_method(DEVICED_PATH_USB,
			DEVICED_INTERFACE_USB,
			edbus_methods, ARRAY_SIZE(edbus_methods));
}
