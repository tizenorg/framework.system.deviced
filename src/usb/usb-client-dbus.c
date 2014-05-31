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
#include "usb-client.h"
#include "core/edbus-handler.h"

#define CHANGE_USB_MODE "ChangeUsbMode"

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

	switch (mode){
	case SET_USB_DEFAULT:
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_DIAG:
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

