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
#include "core/device-notifier.h"

static int hotspot_mode_changed(void *data)
{
	tethering_status_changed();
	return 0;
}

static void register_usb_handlers(void)
{
	if (vconf_notify_key_changed(
				VCONFKEY_USB_SEL_MODE,
				client_mode_changed, NULL) != 0)
		_E("Failed to notify vconf key");

	if (vconf_notify_key_changed(
				VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL,
				debug_mode_changed, NULL) != 0)
		_E("Failed to notify vconf key");

	register_notifier(DEVICE_NOTIFIER_MOBILE_HOTSPOT_MODE,
	    hotspot_mode_changed);
}

static void unregister_usb_handlers(void)
{
	vconf_ignore_key_changed(
			VCONFKEY_USB_SEL_MODE,
			client_mode_changed);

	vconf_ignore_key_changed(
			VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL,
			debug_mode_changed);

	unregister_notifier(DEVICE_NOTIFIER_MOBILE_HOTSPOT_MODE,
	    hotspot_mode_changed);
}

static void set_usb_config_state(int state)
{
	send_msg_usb_config_enabled(state);
}

#ifdef EMULATOR
void action_usb_connected(void)
{
	update_current_usb_mode(SET_USB_SDB);

	update_usb_state(USB_STATE_CONNECTED);

	update_usb_state(USB_STATE_AVAILABLE);

	pm_lock_internal(getpid(), LCD_OFF, STAY_CUR_STATE, 0);
}

void action_usb_disconnected(void)
{
	pm_unlock_internal(getpid(), LCD_OFF, STAY_CUR_STATE);

	update_current_usb_mode(SET_USB_NONE);

	update_usb_state(USB_STATE_DISCONNECTED);
}
#else
void action_usb_connected(void)
{
	set_usb_config_state(USB_CONF_ENABLED);

	update_usb_state(USB_STATE_CONNECTED);

	register_usb_handlers();

	change_client_setting(SET_CONFIGURATION | SET_OPERATION | SET_NOTIFICATION);

	pm_lock_internal(getpid(), LCD_OFF, STAY_CUR_STATE, 0);
}

void action_usb_disconnected(void)
{
	int cur_mode;

	pm_unlock_internal(getpid(), LCD_OFF, STAY_CUR_STATE);

	set_usb_config_state(USB_CONF_DISABLED);

	cur_mode = get_current_usb_mode();
	if (cur_mode > SET_USB_NONE)
		unset_client_mode(cur_mode, false);

	unregister_usb_handlers();

	update_usb_state(USB_STATE_DISCONNECTED);
}
#endif
