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
#include <string.h>

#define UDEV_PROP_DEVLINKS   "DEVLINKS"
#define UDEV_PROP_PATH       "ID_PATH"
#define UDEV_PROP_KEYBOARD   "ID_INPUT_KEYBOARD"
#define UDEV_PROP_MOUSE      "ID_INPUT_MOUSE"
#define UDEV_PROP_MODEL      "ID_MODEL"
#define UDEV_PROP_MODEL_ID   "ID_MODEL_ID"
#define UDEV_PROP_VENDOR     "ID_VENDOR"
#define UDEV_PROP_VENDOR_ID  "ID_VENDOR_ID"

static bool check_same_hid(const char *name)
{
	dd_list *dev_list, *l;
	struct usb_device *dev;
	char head[BUF_MAX];
	char *term;

	if (!name)
		return false;

	dev_list = get_device_list();
	if (!dev_list)
		return false;

	snprintf(head, sizeof(head), "%s", name);
	term = strrchr(head, ':');
	if (!term)
		return false;
	*term = '\0';
	_I("hid name head:(%s)", head);

	DD_LIST_FOREACH(dev_list, l, dev) {
		if (strstr(dev->name, head))
			return true;
	}

	return false;
}

static void hid_device_added (struct udev_device *dev)
{
	const char *path = NULL;
	const char *type = NULL;
	const char *model = NULL;
	const char *model_id = NULL;
	const char *vendor = NULL;
	const char *vendor_id = NULL;
	char v_vendor[BUF_MAX];
	char v_model[BUF_MAX];
	int dev_type;

	if (!dev)
		return;

	if (udev_device_get_property_value(dev, UDEV_PROP_DEVLINKS))
		return;

	path = udev_device_get_property_value(dev, UDEV_PROP_PATH);
	if (!path) {
		_E("cannot get device path");
		return;
	}

	if (check_same_hid(path)) {
		_E("Same device is already connected");
		return;
	}

	type = udev_device_get_property_value(dev, UDEV_PROP_KEYBOARD);
	if (type)
		dev_type = USBHOST_KEYBOARD;
	else {
		type = udev_device_get_property_value(dev, UDEV_PROP_MOUSE);
		if (type)
			dev_type = USBHOST_MOUSE;
	}
	if (!type) {
		_E("cannot get device type");
		return;
	}

	vendor = udev_device_get_property_value(dev, UDEV_PROP_VENDOR);
	vendor_id = udev_device_get_property_value(dev, UDEV_PROP_VENDOR_ID);
	if (vendor && vendor_id
			&& !strncmp(vendor, vendor_id, strlen(vendor)))
		memset(v_vendor, 0, sizeof(v_vendor));
	else {
		if (verify_vendor_name(vendor, v_vendor, sizeof(v_vendor)) < 0)
			memset(v_vendor, 0, sizeof(v_vendor));
	}

	model = udev_device_get_property_value(dev, UDEV_PROP_MODEL);
	model_id = udev_device_get_property_value(dev, UDEV_PROP_MODEL_ID);
	if (model && model_id
			&& !strncmp(model, model_id, strlen(model)))
		memset(v_model, 0, sizeof(v_model));
	else {
		if (verify_model_name(model, v_vendor, v_model, sizeof(v_vendor)) < 0)
			memset(v_model, 0, sizeof(v_model));
	}

	if (add_usb_device_to_list(dev_type, path, v_model, v_vendor) < 0) {
		_E("Failed to add device to dev_list");
		return;
	}

	if (dev_type == USBHOST_KEYBOARD) {
		launch_ticker_notification(TICKER_NAME_KEYBOARD_CONNECTED);
		send_msg_keyboard_added();
	} else if (dev_type == USBHOST_MOUSE) {
		launch_ticker_notification(TICKER_NAME_MOUSE_CONNECTED);
		send_msg_mouse_added();
	}
}

static void hid_device_removed (struct udev_device *dev)
{
	const char *path = NULL;
	const char *type = NULL;
	int dev_type;

	if (!dev)
		return;

	if (udev_device_get_property_value(dev, UDEV_PROP_DEVLINKS))
		return;

	type = udev_device_get_property_value(dev, UDEV_PROP_KEYBOARD);
	if (type)
		dev_type = USBHOST_KEYBOARD;
	else {
		type = udev_device_get_property_value(dev, UDEV_PROP_MOUSE);
		if (type)
			dev_type = USBHOST_MOUSE;
		else
			return;
	}

	path = udev_device_get_property_value(dev, UDEV_PROP_PATH);
	if (!path) {
		_E("cannot get device path");
		return;
	}

	if (remove_usb_device_from_list(path, dev_type) < 0) {
		_E("Failed to remove device from dev_list");
		return;
	}

	launch_ticker_notification(TICKER_NAME_DEVICE_DISCONNECTED);

	if (dev_type == USBHOST_KEYBOARD)
		send_msg_keyboard_removed();
	else if (dev_type == USBHOST_MOUSE)
		send_msg_mouse_removed();
}

static void subsystem_input_changed (struct udev_device *dev)
{
	const char *action = NULL;
	if (!dev)
		return;

	if (!is_host_uevent_enabled())
		return;

	action = udev_device_get_property_value(dev, "ACTION");
	if (!action)
		return;

	if (!strncmp(action, "add", strlen("add"))) {
		hid_device_added(dev);
		return;
	}

	if (!strncmp(action, "remove", strlen("remove"))) {
		hid_device_removed(dev);
		return;
	}
}

const static struct uevent_handler uhs_hid[] = {
	{ INPUT_SUBSYSTEM       ,     subsystem_input_changed       ,    NULL    },
};

static int usbhost_hid_init_booting_done(void *data)
{
	int ret, i;

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_hid_init_booting_done);

	for (i = 0 ; i < ARRAY_SIZE(uhs_hid) ; i++) {
		ret = register_uevent_control(&uhs_hid[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	return 0;
}

static void usbhost_hid_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_hid_init_booting_done);
}

static void usbhost_hid_exit(void *data)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(uhs_hid) ; i++) {
		unregister_uevent_control(&uhs_hid[i]);
	}
}

static const struct device_ops usbhost_device_hid_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "usbhost_hid",
	.init     = usbhost_hid_init,
	.exit     = usbhost_hid_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_hid_ops)
