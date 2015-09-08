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
#include <vconf.h>
#include "usb-client.h"

#ifdef EMULATOR

static void act_usb_connected_sdk(void)
{
	if (vconf_set_int(VCONFKEY_USB_CONFIGURATION_ENABLED, USB_CONF_ENABLED) != 0)
		_E("Failed to set vconf key (%s)", VCONFKEY_USB_CONFIGURATION_ENABLED);

	update_current_usb_mode(SET_USB_SDB);

	if (update_usb_state(VCONFKEY_SYSMAN_USB_AVAILABLE) < 0)
		_E("FAIL: update_usb_state(%d)", VCONFKEY_SYSMAN_USB_AVAILABLE);

	pm_lock_internal(getpid(), LCD_OFF, STAY_CUR_STATE, 0);
}

static void act_usb_disconnected_sdk(void)
{
	pm_unlock_internal(getpid(), LCD_OFF, STAY_CUR_STATE);

	if (vconf_set_int(VCONFKEY_USB_CONFIGURATION_ENABLED, USB_CONF_DISABLED) != 0)
		_E("Failed to set vconf key (%s)", VCONFKEY_USB_CONFIGURATION_ENABLED);

	update_current_usb_mode(SET_USB_NONE);

	if (update_usb_state(VCONFKEY_SYSMAN_USB_DISCONNECTED) < 0)
		_E("FAIL: update_usb_state(%d)", VCONFKEY_SYSMAN_USB_DISCONNECTED);
}

void usb_state_changed(int state)
{
	if (state == 0)
		act_usb_disconnected_sdk();
	else if (state == 1)
		act_usb_connected_sdk();
	else
		_E("Invalid USB state (%d)", state);
}
#else
void usb_state_changed(int state)
{
	/* Do nothing */
}
#endif
