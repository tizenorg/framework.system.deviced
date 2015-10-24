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

#ifndef __USB_CLIENT_H__
#define __USB_CLIENT_H__

#include <stdbool.h>
#include <device-node.h>

#include "core/log.h"
#include "core/devices.h"
#include "display/poll.h"
#include "core/udev.h"
#include "core/common.h"
#include "core/list.h"
#include "apps/apps.h"
#include "usb-common.h"

/* Switch uevent */
#define UDEV_PROP_KEY_SWITCH_NAME  "SWITCH_NAME"
#define UDEV_PROP_VALUE_USB_CABLE  "usb_cable"
#define UDEV_PROP_VALUE_USB_CABLE_LEN 9

#define UDEV_PROP_KEY_SWITCH_STATE "SWITCH_STATE"
#define UDEV_PROP_KEY_SWITCH_STATE_LEN 12

#define UDEV_PROP_VALUE_DISCON     "0"
#define UDEV_PROP_VALUE_CON_SDP    "1" /* Standard downstream port */
#define UDEV_PROP_VALUE_CON_CDP    "2" /* Charging downstream port */
#define UDEV_PROP_VALUE_LEN        1
/* usb_mode uevnet */
#define USBMODE_SUBSYSTEM          "usb_mode"
#define UDEV_PROP_KEY_USB_STATE    "USB_STATE"
#define UDEV_PROP_VALUE_CONFIGURED "CONFIGURED"
#define UDEV_PROP_VALUE_CONFIGURED_LEN 10

#define USB_CON_START  "start"
#define USB_CON_STOP   "stop"

#define USB_RESTRICT  "restrict"
#define USB_ERROR     "error"

#define USB_BUF_LEN 64

enum internal_usb_mode {
	SET_USB_DIAG_RMNET = 11,
};

enum usbclient_setting_option {
	SET_CONFIGURATION = 0x0001,
	SET_OPERATION     = 0x0010,
	SET_NOTIFICATION  = 0x0100,
};

enum usb_enabled {
	USB_CONF_DISABLED,
	USB_CONF_ENABLED,
};

struct usb_configuration {
	char name[USB_BUF_LEN];
	char value[USB_BUF_LEN];
};

struct usb_operation {
	char name[USB_BUF_LEN];
	char oper[USB_BUF_LEN];
};

struct usb_common_config {
	int wait_configured_connected;
	int set_operation_connected;
	int set_notification_connected;
};

enum usb_state {
	USB_STATE_DISCONNECTED = 0,
	USB_STATE_CONNECTED,
	USB_STATE_AVAILABLE,
};

/* config */
int make_configuration_list(int usb_mode);
void release_configuration_list(void);
int make_operation_list(int usb_mode, char *action);
void release_operations_list(void);
struct usb_common_config *get_usb_common_config(void);
void init_common_config(void);

int get_root_path(char **path);
int get_operations_list(dd_list **list);
int get_configurations_list(dd_list **list);

/* vconf callbacks */
void client_mode_changed(keynode_t* key, void *data);
void prev_client_mode_changed(keynode_t* key, void *data);
void debug_mode_changed(keynode_t* key, void *data);
void charge_only_changed(keynode_t* key, void *data);

void tethering_status_changed(void);

/* Check usb state */
int get_default_mode(void);
int get_current_usb_physical_state(void);
int get_current_usb_logical_state(void);
int get_current_usb_mode(void);
int get_selected_usb_mode(void);
int get_debug_mode(void);
int change_selected_usb_mode(int mode);
int update_current_usb_mode(int mode);
void update_usb_state(enum usb_state state);
unsigned int get_current_usb_gadget_info(int mode);
bool charge_only_mode(bool update);

/* Unset usb mode */
void unset_client_mode(int mode, bool change);

/* USB control */
int control_start(enum device_flags flags);
int control_stop(enum device_flags flags);
int control_status(void);
void wait_until_booting_done(void);
void usbclient_init_booting_done(void);
void act_usb_connected(void);
void act_usb_disconnected(void);
int register_usbclient_dbus_methods(void);
void send_msg_usb_state_changed(void);
void send_msg_usb_mode_changed(void);
void send_msg_usb_config_enabled(int state);

/* USB syspopup */
void launch_syspopup(char *str);
void launch_restrict_popup(void);

void show_noti(void);
void terminate_noti(void);

/* Change usb mode */
void change_client_setting(int options);
bool get_wait_configured(void);

/* Action for usb state */
void action_usb_connected(void);
void action_usb_disconnected(void);

/* Uevent */
void register_usb_uevent_handler(void);
void unregister_usb_uevent_handler(void);
void check_usb_state_direct(void);

/* Gadgets */
enum media_device_state {
	MEDIA_DEVICE_OFF,
	MEDIA_DEVICE_ON,
};

int media_device_state(void);

#endif /* __USB_CLIENT_H__ */
