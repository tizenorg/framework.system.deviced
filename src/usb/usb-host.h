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

#ifndef __USB_HOST_H__
#define __USB_HOST_H__

#include <stdbool.h>
#include "core/udev.h"
#include "core/log.h"
#include "core/devices.h"
#include "display/poll.h"
#include "core/udev.h"
#include "core/common.h"
#include "core/list.h"
#include "usb-common.h"

#define BLOCK_SUBSYSTEM           "block"
#define INPUT_SUBSYSTEM           "input"
#define USB_SUBSYSTEM             "usb"

#define UDEV_PROP_KEY_STATE       "STATE"
#define UDEV_PROP_VALUE_ADD       "ADD"
#define UDEV_PROP_VALUE_REMOVE    "REMOVE"

#define TICKER_NAME_CONNECTOR_CONNECTED         "connector-connected"
#define TICKER_NAME_CONNECTOR_DISCONNECTED      "connector-disconnected"
#define TICKER_NAME_STORAGE_CONNECTED           "storage-connected"
#define TICKER_NAME_STORAGE_RO_CONNECTED        "storage-ro-connected"
#define TICKER_NAME_STORAGE_DISCONNECTED_SAFE   "storage-disconnected-safe"
#define TICKER_NAME_STORAGE_DISCONNECTED_UNSAFE "storage-disconnected-unsafe"
#define TICKER_NAME_KEYBOARD_CONNECTED          "keyboard-connected"
#define TICKER_NAME_MOUSE_CONNECTED             "mouse-connected"
#define TICKER_NAME_CAMERA_CONNECTED            "camera-connected"
#define TICKER_NAME_PRINTER_CONNECTED           "printer-connected"
#define TICKER_NAME_DEVICE_DISCONNECTED         "device-disconnected"

#define BUF_MAX 256
#define RETRY_MAX 5

enum action_type {
	STORAGE_ADD,
	STORAGE_REMOVE,
	STORAGE_MOUNT,
	STORAGE_UNMOUNT,
	STORAGE_FORMAT,
};

struct pipe_data {
	int type;
	int result;
	void *data;
};

struct usb_device {
	int type;
	char name[BUF_MAX];
	char mntpath[BUF_MAX];
	char vendor[BUF_MAX];
	char model[BUF_MAX];
	char fs[BUF_MAX];
	bool is_mounted;
	int noti_id;
};

struct storage_fs_ops {
	char *name;
	int (*check)(const char *devname);
	int (*mount)(bool smack, const char *devname, const char *mount_point);
	int (*mount_rdonly)(bool smack, const char *devname, const char *mount_point);
	int (*format)(const char *path);
};

enum usb_host_type {
	USBHOST_KEYBOARD,
	USBHOST_MOUSE,
	USBHOST_STORAGE,
	USBHOST_CAMERA,
	USBHOST_PRINTER,
	USBHOST_UNKNOWN,
	/* add type of usb host */
	USBHOST_TYPE_MAX,
};

enum mount_type {
	STORAGE_MOUNT_RW,
	STORAGE_MOUNT_RO,
};

enum device_state {
	DEVICE_DISCONNECTED = 0,
	DEVICE_CONNECTED = 1,
};

void launch_ticker_notification(char *name);
bool is_host_uevent_enabled(void);
dd_list *get_device_list(void);
dd_list *get_storage_list(void);
int get_mount_path(const char *name, char *path, int len);
int get_devname_by_path(char *path, char *name, int len);
bool check_same_mntpath(const char *mntpath);
bool is_storage_mounted(const char *mntpath);
void update_usbhost_state(void);

void register_fs(const struct storage_fs_ops *ops);
void unregister_fs(const struct storage_fs_ops *ops);

int get_storage_list_length(void);

/* Add/Remove usb storage to list */
int add_usb_storage_to_list(const char *name,
		const char *vendor,
		const char *model,
		const char *fstype);
int remove_usb_storage_from_list(const char *name);

/* Add/Remove usb device to list */
int add_usb_device_to_list(int type,
		const char *name,
		const char *vendor,
		const char *model);
int remove_usb_device_from_list(const char *name, int type);

/* Mount/Unmount usb storge */
int mount_usb_storage(const char *name);
int unmount_usb_storage(const char *path, bool safe);

void launch_host_syspopup(char *name, char *method,
		char *key1, char *value1, char *key2, char *value2);

/* Thread Job */
int add_job(int type, struct usb_device *dev, bool safe);

/* Device changed signal */
void send_msg_storage_added(bool mount);
void send_msg_storage_removed(bool mount);
void send_msg_keyboard_added(void);
void send_msg_keyboard_removed(void);
void send_msg_mouse_added(void);
void send_msg_mouse_removed(void);
void send_msg_camera_added(void);
void send_msg_camera_removed(void);

/* Ongoing notification */
int activate_storage_notification(char *path, int mount_type);
int deactivate_storage_notification(int id);
int activate_device_notification(struct usb_device *dev, int len);
int deactivate_device_notification(struct usb_device *dev, int len);

/* Unmount signal handler */
int register_unmount_signal_handler(void);
int unmount_storage_by_dbus_signal(char *path);

/* device info signal handler */
int register_device_all_signal_handler(void);
int send_device_added_info(struct usb_device *dev);
int send_device_removed_info(struct usb_device *dev);
int register_usbhost_dbus_methods(void);
int send_storage_changed_info(struct usb_device *dev, char *type);

/* Vendor, model name */
int verify_vendor_name(const char *vendor, char *buf, int len);
int verify_model_name(const char *model, char *vendor, char *buf, int len);

#endif /* __USB_HOST_H__ */
