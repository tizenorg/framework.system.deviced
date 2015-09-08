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

#include "usb-host.h"
#include "core/edbus-handler.h"

#define RETRY_MAX 5

#define BUS_NAME_PREFIX           "org.tizen.usb"
#define OBJECT_PATH_PREFIX        "/Org/Tizen/Usb"

/* Notice for usb storage mount/unmount success */
#define STORAGE_BUS_NAME           BUS_NAME_PREFIX".storage"
#define STORAGE_OBJECT_PATH        OBJECT_PATH_PREFIX"/Storage"
#define STORAGE_INTERFACE_NAME     STORAGE_BUS_NAME

/* Notice for device changed */
#define HOST_BUS_NAME              BUS_NAME_PREFIX".host"
#define HOST_OBJECT_PATH           OBJECT_PATH_PREFIX"/Host"
#define HOST_INTERFACE_NAME        HOST_BUS_NAME
#define HOST_MOUSE_SIGNAL          "usbmouse"
#define HOST_KEYBOARD_SIGNAL       "usbkeyboard"
#define HOST_STORAGE_SIGNAL        "usbstorage"
#define HOST_CAMERA_SIGNAL         "usbcamera"
#define HOST_ADDED                 "added"
#define HOST_REMOVED               "removed"

/* Notification */
#define POPUP_BUS_NAME             "org.tizen.system.popup"
#define POPUP_OBJECT_PATH          "/Org/Tizen/System/Popup"
#define POPUP_INTERFACE_NAME       POPUP_BUS_NAME

#define POPUP_PATH_USBHOST         POPUP_OBJECT_PATH"/Usbhost"
#define POPUP_INTERFACE_USBHOST        POPUP_INTERFACE_NAME".Usbhost"

#define METHOD_STORAGE_NOTI_ON     "UsbStorageNotiOn"
#define METHOD_STORAGE_RO_NOTI_ON  "UsbStorageRoNotiOn"
#define METHOD_STORAGE_NOTI_OFF    "UsbStorageNotiOff"
#define METHOD_DEVICE_NOTI_ON      "UsbDeviceNotiOn"
#define METHOD_DEVICE_NOTI_UPDATE  "UsbDeviceNotiUpdate"
#define METHOD_DEVICE_NOTI_OFF     "UsbDeviceNotiOff"

/* Unmount */
#define SIGNAL_NAME_UNMOUNT        "unmount_storage"

/* Send device info to host-devices app */
#define SIGNAL_NAME_DEVICE_ALL     "host_device_all"
#define SIGNAL_NAME_DEVICE_ADDED   "host_device_add"
#define SIGNAL_NAME_DEVICE_REMOVED "host_device_remove"

/* USB storage update signal */
#define SIGNAL_NAME_USB_STORAGE_CHANGED    "usb_storage_changed"
#define STORAGE_ADDED                      "storage_added"
#define STORAGE_REMOVED                    "storage_removed"
#define STORAGE_UPDATED                    "storage_updated"


struct noti_keyword {
	int type;
	char *key;
};

static struct noti_keyword noti_key[] = {
	{ USBHOST_KEYBOARD   , "keyboard" },
	{ USBHOST_MOUSE      , "mouse"    },
	{ USBHOST_CAMERA     , "camera"   },
	{ USBHOST_PRINTER    , "printer"  },
	{ USBHOST_UNKNOWN    , "unknown"  },
};

static int noti_id = 0;

void send_msg_storage_added(bool mount)
{
	char *param[1];

	param[0] = HOST_ADDED;

	if (mount) {
		if (broadcast_edbus_signal(STORAGE_OBJECT_PATH,
				STORAGE_INTERFACE_NAME,
				HOST_STORAGE_SIGNAL,
				"s", param) < 0)
			_E("Failed to send dbus signal");
	} else {
		if (broadcast_edbus_signal(HOST_OBJECT_PATH,
				HOST_INTERFACE_NAME,
				HOST_STORAGE_SIGNAL,
				"s", param) < 0)
			_E("Failed to send dbus signal");
	}
}

void send_msg_storage_removed(bool mount)
{
	char *param[1];

	param[0] = HOST_REMOVED;

	if (mount) {
		if (broadcast_edbus_signal(STORAGE_OBJECT_PATH,
				STORAGE_INTERFACE_NAME,
				HOST_STORAGE_SIGNAL,
				"s", param) < 0)
			_E("Failed to send dbus signal");
	} else {
		if (broadcast_edbus_signal(HOST_OBJECT_PATH,
				HOST_INTERFACE_NAME,
				HOST_STORAGE_SIGNAL,
				"s", param) < 0)
			_E("Failed to send dbus signal");
	}
}

static void send_msg_device_changed(char *signal, char* action)
{
	char *param[1];

	param[0] = action;

	if (broadcast_edbus_signal(HOST_OBJECT_PATH,
			HOST_INTERFACE_NAME,
			signal,
			"s", param) < 0)
		_E("Failed to send dbus signal");
}

void send_msg_keyboard_added(void)
{
	send_msg_device_changed(HOST_KEYBOARD_SIGNAL, HOST_ADDED);
}

void send_msg_keyboard_removed(void)
{
	send_msg_device_changed(HOST_KEYBOARD_SIGNAL, HOST_REMOVED);
}

void send_msg_mouse_added(void)
{
	send_msg_device_changed(HOST_MOUSE_SIGNAL, HOST_ADDED);
}

void send_msg_mouse_removed(void)
{
	send_msg_device_changed(HOST_MOUSE_SIGNAL, HOST_REMOVED);
}

void send_msg_camera_added(void)
{
	send_msg_device_changed(HOST_CAMERA_SIGNAL, HOST_ADDED);
}

void send_msg_camera_removed(void)
{
	send_msg_device_changed(HOST_CAMERA_SIGNAL, HOST_REMOVED);
}

int activate_storage_notification(char *path, int mount_type)
{
	char devpath[BUF_MAX];
	char *arr[1];
	int ret, i;
	char *method;

	if (!path)
		return -EINVAL;

	switch (mount_type) {
	case STORAGE_MOUNT_RW:
		method = METHOD_STORAGE_NOTI_ON;
		break;
	case STORAGE_MOUNT_RO:
		method = METHOD_STORAGE_RO_NOTI_ON;
		break;
	default:
		_E("Unknown mount type(%d)", mount_type);
		return -EINVAL;
	}

	snprintf(devpath, sizeof(devpath), "%s", path);
	arr[0] = devpath;

	i = 0;
	do {
		ret = dbus_method_sync(POPUP_BUS_NAME,
				POPUP_PATH_USBHOST,
				POPUP_INTERFACE_USBHOST,
				METHOD_STORAGE_NOTI_ON,
				"s", arr);
		if (ret < 0)
			_E("Retry to activate notification (path: %s, ret: %d, retry: %d)", path, ret, i);
		else
			break;
	} while (i++ < RETRY_MAX);

	return ret;
}

int deactivate_storage_notification(int id)
{
	char devpath[BUF_MAX];
	char *arr[1];
	int ret, i;

	if (id <= 0)
		return -EINVAL;

	snprintf(devpath, sizeof(devpath), "%d", id);
	arr[0] = devpath;

	i = 0;
	do {
		ret = dbus_method_sync(POPUP_BUS_NAME,
				POPUP_PATH_USBHOST,
				POPUP_INTERFACE_USBHOST,
				METHOD_STORAGE_NOTI_OFF,
				"i", arr);
		if (ret < 0)
			_E("Retry to deactivate notification (ret: %d, retry: %d)", ret, i);
		else
			break;
	} while (i++ < RETRY_MAX);

	return ret;
}

static void unmount_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *path;
	struct usb_device dev;
	int ret;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_USBHOST, SIGNAL_NAME_UNMOUNT) == 0) {
		_E("The signal is not for unmounting storage");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID) == 0) {
		_E("FAIL: dbus_message_get_args");
		return;
	}

	snprintf(dev.mntpath, sizeof(dev.mntpath), "%s", path);
	if (is_storage_mounted(path)) {
		if (add_job(STORAGE_UNMOUNT, &dev, true) < 0)
			_E("Failed to add job(%d, %s)", STORAGE_UNMOUNT, path);
	} else {
		_E("Failed to unmount since the storage (%s) is not mounted", path);
	}
}

int register_unmount_signal_handler(void)
{
	return register_edbus_signal_handler(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			SIGNAL_NAME_UNMOUNT, unmount_signal_handler);
}

static int get_product_name(struct usb_device *dev, char *product, int len)
{
	int v_len, m_len;
	if (!dev || !product || len <= 0)
		return -EINVAL;

	v_len = strlen(dev->vendor);
	m_len = strlen(dev->model);

	if (v_len > 0 && m_len > 0)
		snprintf(product, len, "%s %s", dev->vendor, dev->model);
	else if (v_len > 0)
		snprintf(product, len, "%s", dev->vendor);
	else if (m_len > 0)
		snprintf(product, len, "%s", dev->model);
	else
		memset(product, 0, len);

	return 0;
}

static int launch_device_notification(int type, char *product)
{
	char *noti_type = NULL;
	int i;
	char devpath[BUF_MAX];
	char *arr[2];
	int ret, ret_val;

	for (i = 0; i < ARRAY_SIZE(noti_key) ; i++) {
		if (noti_key[i].type == type) {
			noti_type = noti_key[i].key;
			break;
		}
	}
	if (!noti_type)
		return -EINVAL;

	arr[0] = noti_type;
	arr[1] = product;

	i = 0;
	do {
		ret = dbus_method_sync(POPUP_BUS_NAME,
				POPUP_PATH_USBHOST,
				POPUP_INTERFACE_USBHOST,
				METHOD_DEVICE_NOTI_ON,
				"ss", arr);
		if (ret < 0)
			_E("Retry to activate notification (product: %s, ret: %d, retry: %d)", product, ret, i);
		else
			break;
	} while (i++ < RETRY_MAX);

	return ret;
}

static int remove_device_notification(void)
{
	char *arr[1];
	int ret, ret_val, i;
	char id[8];

	snprintf(id, sizeof(id), "%d", noti_id);
	arr[0] = id;

	i = 0;
	do {
		ret = dbus_method_sync(POPUP_BUS_NAME,
				POPUP_PATH_USBHOST,
				POPUP_INTERFACE_USBHOST,
				METHOD_DEVICE_NOTI_OFF,
				"i", arr);
		if (ret < 0)
			_E("Retry to deactivate notification (ret: %d, retry: %d)", ret, i);
		else
			break;
	} while (i++ < RETRY_MAX);

	return ret;
}

static int update_device_notification(int devlen, int type, char *product)
{
	char *noti_type = NULL;
	int i, ret;
	char devpath[BUF_MAX];
	char *arr[4];
	char id[8];
	char len[4];

	if (noti_id <= 0)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(noti_key) ; i++) {
		if (noti_key[i].type == type) {
			noti_type = noti_key[i].key;
			break;
		}
	}
	if (!noti_type)
		return -EINVAL;

	snprintf(id, sizeof(id), "%d", noti_id);
	snprintf(len, sizeof(len), "%d", devlen);
	arr[0] = id;
	arr[1] = len;
	arr[2] = noti_type;
	arr[3] = product;

	i = 0;
	do {
		ret = dbus_method_sync(POPUP_BUS_NAME,
				POPUP_PATH_USBHOST,
				POPUP_INTERFACE_USBHOST,
				METHOD_DEVICE_NOTI_UPDATE,
				"isss", arr);
		if (ret < 0)
			_E("Retry to update notification (ret: %d, retry: %d)", ret, i);
		else
			break;
	} while (i++ < RETRY_MAX);

	return ret;
}

int activate_device_notification(struct usb_device *dev, int len)
{
	char product[BUF_MAX];
	int ret;

	if (!dev)
		return -EINVAL;

	ret = get_product_name(dev, product, sizeof(product));
	if (ret < 0) {
		_E("cannot get product name to show");
		return ret;
	}

	if (len > 0) {
		return update_device_notification(len + 1, dev->type, product);
	} else {
		noti_id= launch_device_notification(dev->type, product);
		if (noti_id < 0)
			return noti_id;
		else
			return 0;
	}
}

int deactivate_device_notification(struct usb_device *dev, int len)
{
	char product[BUF_MAX];
	int ret;

	if (len <= 0) {
		ret = remove_device_notification();
		noti_id = 0;
		if (ret < 0)
			_E("Faile to remove notification");
		return ret;
	}

	ret = get_product_name(dev, product, sizeof(product));
	if (ret < 0) {
		_E("cannot get product name to show");
		return ret;
	}

	return update_device_notification(len, dev->type, product);
}

static int send_device_changed_info(struct usb_device *dev, char *signal)
{
	char *param[3];
	char v_vendor[BUF_MAX];
	char v_model[BUF_MAX];
	int i;
	char *noti_type;

	int ret;

	if (!dev | !signal)
		return -EINVAL;

	ret = verify_vendor_name(dev->vendor, v_vendor, sizeof(v_vendor));
	if (ret < 0)
		return ret;
	ret = verify_model_name(dev->model, v_vendor, v_model, sizeof(v_model));
	if (ret < 0)
		return ret;

	noti_type = NULL;
	for (i = 0; i < ARRAY_SIZE(noti_key) ; i++) {
		if (noti_key[i].type == dev->type) {
			noti_type = noti_key[i].key;
			break;
		}
	}
	if (!noti_type)
		return -EINVAL;

	param[0] = noti_type;
	param[1] = v_vendor;
	param[2] = v_model;

	ret = broadcast_edbus_signal(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			signal,
			"sss", param);
	return ret;
}

int send_device_added_info(struct usb_device *dev)
{
	return send_device_changed_info(dev, SIGNAL_NAME_DEVICE_ADDED);
}

int send_device_removed_info(struct usb_device *dev)
{
	return send_device_changed_info(dev, SIGNAL_NAME_DEVICE_REMOVED);
}

static int send_all_device_info(void)
{
	dd_list *l;
	dd_list *dev_list;
	struct usb_device *dev;

	dev_list = get_device_list();
	if (!dev_list) {
		_E("cannot get device list()");
		return -ENOMEM;
	}

	_I("device list length: %d", DD_LIST_LENGTH(dev_list));

	DD_LIST_FOREACH(dev_list, l, dev) {
		if (send_device_added_info(dev) < 0)
			_E("Failed to send device info");
	}

	return 0;
}

static void host_device_all_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *str;
	char *path;

	_I("All device info are requested");

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_USBHOST, SIGNAL_NAME_DEVICE_ALL) == 0) {
		_E("The signal is not for unmounting storage");
		return;
	}

	if (send_all_device_info() < 0)
		_E("Failed to send device info");
}

int register_device_all_signal_handler(void)
{
	return register_edbus_signal_handler(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			SIGNAL_NAME_DEVICE_ALL, host_device_all_signal_handler);
}

int send_storage_changed_info(struct usb_device *dev, char *type)
{
	char *param[3];
	char mounted[2];

	if (!dev | !type)
		return -EINVAL;

	param[0] = type;
	param[1] = dev->mntpath;
	snprintf(mounted, sizeof(mounted), "%d", dev->is_mounted);
	param[2] = mounted;

	return broadcast_edbus_signal(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			SIGNAL_NAME_USB_STORAGE_CHANGED,
			"ssi", param);
}

static int send_all_storage_info(void)
{
	dd_list *storage_list, *l;
	struct usb_device *dev;

	storage_list = get_storage_list();
	if (!storage_list)
		return 0;

	DD_LIST_FOREACH (storage_list, l, dev) {
		if (send_storage_changed_info(dev, STORAGE_UPDATED) < 0)
			_E("Failed to send storage info (%s, %s)", dev->name, dev->mntpath);
	}
	return 0;
}

static DBusMessage *send_storage_info_all(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = send_all_storage_info();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *send_storage_mount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *path;
	int ret;
	struct usb_device dev;


	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INVALID);
	if (ret == 0) {
		_E("Failed to get path information");
		ret = -ENOMEM;
		goto out;
	}

	snprintf(dev.mntpath, sizeof(dev.mntpath), "%s", path);
	if (!is_storage_mounted(path)) {
		ret = add_job(STORAGE_MOUNT, &dev, true);
		if (ret < 0)
			_E("Failed to add job(%d, %s)", STORAGE_MOUNT, path);
	} else {
		_E("Failed to mount since the storage (%s) is already mounted", path);
	}

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *send_storage_unmount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *path;
	int ret;
	struct usb_device dev;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INVALID);
	if (ret == 0) {
		_E("Failed to get path information");
		ret = -ENOMEM;
		goto out;
	}

	snprintf(dev.mntpath, sizeof(dev.mntpath), "%s", path);
	if (is_storage_mounted(path)) {
		ret = add_job(STORAGE_UNMOUNT, &dev, true);
		if (ret < 0)
			_E("Failed to add job(%d, %s)", STORAGE_UNMOUNT, path);
	} else {
		_E("Failed to unmount since the storage (%s) is not mounted", path);
		ret = -ECANCELED;
	}

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *send_storage_format(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *path;
	int ret;
	struct usb_device dev;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INVALID);
	if (ret == 0) {
		_E("Failed to get path information");
		ret = -ENOMEM;
		goto out;
	}

	snprintf(dev.mntpath, sizeof(dev.mntpath), "%s", path);
	ret = add_job(STORAGE_FORMAT, &dev, true);
	if (ret < 0)
		_E("Failed to add job(%d, %s)", STORAGE_FORMAT, path);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static int is_device_connected(int type)
{
	int ret;
	dd_list *device_list, *l;
	struct usb_device *dev;

	ret = DEVICE_DISCONNECTED;

	device_list = get_device_list();
	if (!device_list) {
		_E("Device list is empty");
		return ret;
	}

	DD_LIST_FOREACH(device_list, l, dev) {
		if (dev->type == type) {
			ret = DEVICE_CONNECTED;
			break;
		}
	}
	_I("Device (%d) state is (%d)", type, ret);
	return ret;
}

static DBusMessage *send_keyboard_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = is_device_connected(USBHOST_KEYBOARD);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *send_mouse_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = is_device_connected(USBHOST_MOUSE);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "StorageInfoAll"  ,  NULL, "i" ,  send_storage_info_all },
	{ "StorageMount"    ,   "s", "i" ,  send_storage_mount    },
	{ "StorageUnmount"  ,   "s", "i" ,  send_storage_unmount  },
	{ "StorageFormat"   ,   "s", "i" ,  send_storage_format   },
	{ "GetKeyboardState",  NULL, "i" ,  send_keyboard_state   },
	{ "GetMouseState"   ,  NULL, "i" ,  send_mouse_state      },
};

int register_usbhost_dbus_methods(void)
{
	return register_edbus_method(DEVICED_PATH_USBHOST, edbus_methods, ARRAY_SIZE(edbus_methods));
}
