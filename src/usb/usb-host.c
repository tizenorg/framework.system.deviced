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
#include "core/device-notifier.h"

static dd_list *storage_list;
static dd_list *device_list;

static int noti_id = 0;

struct ticker_data {
	char *name;
	int type;
};

struct popup_data {
	char *name;
	char *method;
	char *key1;
	char *value1;
	char *key2;
	char *value2;
};

/* Do not handle usb host uevents if host_uevent_enable is false
 * host_uevent_enable is set to true when usb host connector is connected first*/
static bool host_uevent_enabled = false;

bool is_host_uevent_enabled(void)
{
	return host_uevent_enabled;
}

int get_storage_list_length(void)
{
	return DD_LIST_LENGTH(storage_list);
}

dd_list *get_storage_list(void)
{
	return storage_list;
}

dd_list *get_device_list(void)
{
	return device_list;
}

void launch_ticker_notification(char *name)
{
	struct ticker_data ticker;
	const struct device_ops *ticker_ops;

	if (!name) {
		_E("ticker noti name is NULL");
		return;
	}

	ticker.name = name;
	ticker.type = 0; /* WITHOUT_QUEUE */

	ticker_ops = find_device("ticker");

	if (ticker_ops && ticker_ops->init)
		ticker_ops->init(&ticker);
	else
		_E("cannot find \"ticker\" ops");
}

void launch_host_syspopup(char *name, char *method,
		char *key1, char *value1,
		char *key2, char *value2)
{
	struct popup_data params;
	static const struct device_ops *apps = NULL;

	if (!name || !method)
		return;

	if (apps == NULL) {
		apps = find_device("apps");
		if (apps == NULL)
			return;
	}

	params.name = name;
	params.method = method;
	params.key1 = key1;
	params.value1 = value1;
	params.key2 = key2;
	params.value2 = value2;

	if (apps->init)
		apps->init(&params);
}

static int get_number_of_mounted_storages(void)
{
	int num;
	dd_list *l;
	struct usb_device *dev;

	if (!storage_list || DD_LIST_LENGTH(storage_list) == 0)
		return 0;

	num = 0;
	DD_LIST_FOREACH(storage_list, l, dev) {
		if (dev && dev->is_mounted)
			num++;
	}

	return num;
}

void update_usbhost_state(void)
{
	int prev, curr;

	if (vconf_get_int(VCONFKEY_SYSMAN_USB_HOST_STATUS, &prev) != 0)
		prev = -1;

	if (get_number_of_mounted_storages() == 0
			&& (!device_list || DD_LIST_LENGTH(device_list) == 0))
		curr = VCONFKEY_SYSMAN_USB_HOST_DISCONNECTED;
	else
		curr = VCONFKEY_SYSMAN_USB_HOST_CONNECTED;

	if (prev == curr)
		return;

	if (vconf_set_int(VCONFKEY_SYSMAN_USB_HOST_STATUS, curr) != 0)
		_E("Failed to set vconf key");
}

bool check_same_mntpath(const char *mntpath)
{
	dd_list *l;
	struct usb_device *dev;

	DD_LIST_FOREACH(storage_list, l, dev) {
		if (!strncmp(dev->mntpath, mntpath, strlen(dev->mntpath)))
			return true;
	}
	return false;
}

bool is_storage_mounted(const char *mntpath)
{
	dd_list *l;
	struct usb_device *dev;

	DD_LIST_FOREACH(storage_list, l, dev) {
		if (!strncmp(dev->mntpath, mntpath, strlen(dev->mntpath)))
			break;
	}
	if (l && dev)
		return dev->is_mounted;
	return false;
}

static int add_usb_host_device_to_list(int type,
		const char *name,
		const char *vendor,
		const char *model,
		const char *fstype,
		char *path)
{
	struct usb_device *dev;
	int id;

	if (!name || type < 0)
		return -EINVAL;

	if (type == USBHOST_STORAGE && !path)
		return -EINVAL;

	dev = (struct usb_device *)malloc(sizeof(struct usb_device));
	if (!dev) {
		_E("Failed to assign memory");
		return -ENOMEM;
	}

	dev->type = type;
	dev->is_mounted = false;
	dev->noti_id = 0;
	snprintf(dev->name, sizeof(dev->name), "%s", name);
	snprintf(dev->mntpath, sizeof(dev->mntpath), "%s", path);

	if (model)
		snprintf(dev->model, sizeof(dev->model), "%s", model);
	else
		memset(dev->model, 0, sizeof(dev->model));

	if (vendor)
		snprintf(dev->vendor, sizeof(dev->vendor), "%s", vendor);
	else
		memset(dev->vendor, 0, sizeof(dev->vendor));

	if (fstype)
		snprintf(dev->fs, sizeof(dev->fs), "%s", fstype);
	else
		memset(dev->fs, 0, sizeof(dev->fs));

	if (type == USBHOST_STORAGE) {
		if (!check_same_mntpath(path))
			DD_LIST_APPEND(storage_list, dev);
		else
			free(dev);
	} else {
		dev->noti_id = activate_device_notification(dev, DD_LIST_LENGTH(device_list));
		DD_LIST_APPEND(device_list, dev);
		if (send_device_added_info(dev) < 0)
			_E("Failed to send device info");
	}

	return 0;
}

int add_usb_storage_to_list(const char *name,
		const char *vendor,
		const char *model,
		const char *fstype)
{
	char path[BUF_MAX];
	int ret;

	if (!name)
		return -EINVAL;

	ret = get_mount_path(name, path, sizeof(path));
	if (ret < 0) {
		_E("Failed to get mount path");
		return ret;
	}

	ret = add_usb_host_device_to_list(USBHOST_STORAGE,
			name, vendor, model, fstype, path);

	update_usbhost_state();

	return ret;
}

int add_usb_device_to_list(int type,
		const char *name,
		const char *vendor,
		const char *model)
{
	int ret;

	ret = add_usb_host_device_to_list(type,
			name, vendor, model, NULL, NULL);

	update_usbhost_state();

	return ret;
}

int remove_usb_storage_from_list(const char *name)
{
	dd_list *l;
	struct usb_device *dev;
	bool found;

	if (!name)
		return -EINVAL;

	found = false;
	DD_LIST_FOREACH(storage_list, l, dev) {
		if (strncmp(dev->name, name, strlen(name)))
			continue;

		found = true;
		break;
	}
	if (!found) {
		return -EINVAL;
	}

	DD_LIST_REMOVE(storage_list, dev);
	free(dev);

	update_usbhost_state();

	return 0;
}

int remove_usb_device_from_list(const char *name, int type)
{
	dd_list *l;
	struct usb_device *dev;
	bool found;
	int len;

	if (!name)
		return -EINVAL;

	found = false;
	DD_LIST_FOREACH(device_list, l, dev) {
		if (type == USBHOST_PRINTER) {
			if (!strstr(name, dev->name))
				continue;
		} else {
			if (strncmp(dev->name, name, strlen(name)))
				continue;
		}

		found = true;
		break;
	}
	if (!found) {
		return -EINVAL;
	}

	if (send_device_removed_info(dev) < 0)
		_E("Failed to send device info");

	DD_LIST_REMOVE(device_list, dev);
	free(dev);

	len = DD_LIST_LENGTH(device_list);
	if (len <= 0)
		dev = NULL;
	else
		dev = DD_LIST_NTH(device_list, 0); /* First element */

	if (deactivate_device_notification(dev, len))
		_E("Failed to remove notification");

	update_usbhost_state();

	return 0;
}

static void subsystem_host_changed (struct udev_device *dev)
{
	const char *state = NULL;
	int ret;
	static int cradle;

	state = udev_device_get_property_value(dev, UDEV_PROP_KEY_STATE);
	if (!state)
		return;

	if (!strncmp(state, UDEV_PROP_VALUE_ADD, strlen(UDEV_PROP_VALUE_ADD))) {
		_I("USB host connector is added");

		if (!host_uevent_enabled)
			host_uevent_enabled = true;

		cradle = get_cradle_status();
		if (cradle == 0) {
			launch_ticker_notification(TICKER_NAME_CONNECTOR_CONNECTED);
		}

		return;
	}

	if (!strncmp(state, UDEV_PROP_VALUE_REMOVE, strlen(UDEV_PROP_VALUE_REMOVE))) {
		_I("USB host connector is removed");

		if (cradle == 0) {
			launch_ticker_notification(TICKER_NAME_CONNECTOR_DISCONNECTED);
		} else {
			cradle = 0;
		}

		return;
	}
}

const static struct uevent_handler uhs[] = {
	{ HOST_SUBSYSTEM       ,     subsystem_host_changed       ,    NULL    },
};

static int usbhost_init_booting_done(void *data)
{
	int ret, i;

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_init_booting_done);

	for (i = 0 ; i < ARRAY_SIZE(uhs) ; i++) {
		ret = register_uevent_control(&uhs[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	if (register_unmount_signal_handler() < 0)
		_E("Failed to register handler for unmount signal");

	if (register_device_all_signal_handler() < 0)
		_E("Failed to register handler for device info");

	if (register_usbhost_dbus_methods() < 0)
		_E("Failed to register dbus handler for usbhost");

	return 0;
}

static void usbhost_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_init_booting_done);
}

static void usbhost_exit(void *data)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(uhs) ; i++) {
		unregister_uevent_control(&uhs[i]);
	}
}

static const struct device_ops usbhost_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "usbhost",
	.init     = usbhost_init,
	.exit     = usbhost_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_ops)
