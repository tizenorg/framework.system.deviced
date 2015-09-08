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
#include <stdbool.h>
#include "usb-client.h"
#include "core/device-notifier.h"

#define VCONFKEY_USB_CONTROL "db/private/usb/usb_control"

static int usb_control = DEVICE_OPS_STATUS_START;

int control_start(enum device_flags flags)
{
	int state;

	if (usb_control == DEVICE_OPS_STATUS_START)
		return 0;

	usb_control = DEVICE_OPS_STATUS_START;
	if (vconf_set_int(VCONFKEY_USB_CONTROL, usb_control) != 0)
		_E("Failed to set vconf");

	state = get_current_usb_physical_state();
	usb_state_changed(state);

	if (state > 0)
		act_usb_connected();

	return 0;
}

int control_stop(enum device_flags flags)
{
	int cur_mode;
	if (usb_control == DEVICE_OPS_STATUS_STOP)
		return 0;

	usb_control = DEVICE_OPS_STATUS_STOP;
	if (vconf_set_int(VCONFKEY_USB_CONTROL, usb_control) != 0)
		_E("Failed to set vconf");

	cur_mode = get_current_usb_mode();
	if (cur_mode <= SET_USB_NONE) {
		_E("Current usb mode is already none");
		return 0;
	}

	unset_client_mode(cur_mode, false);

	launch_syspopup(USB_RESTRICT);

	return 0;
}

int control_status(void)
{
	return usb_control;
}

static void check_prev_control_status(void)
{
	if (vconf_get_int(VCONFKEY_USB_CONTROL, &usb_control) != 0)
		usb_control = DEVICE_OPS_STATUS_START;
}

static int usb_client_booting_done(void *data)
{
	int state;

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usb_client_booting_done);
	check_prev_control_status();

	usbclient_init_booting_done();

	state = get_current_usb_physical_state();
	usb_state_changed(state);

	if (state > 0)
		act_usb_connected();

	return 0;
}

void wait_until_booting_done(void)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usb_client_booting_done);
	usb_control = DEVICE_OPS_STATUS_STOP;
}
