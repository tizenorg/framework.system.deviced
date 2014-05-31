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

#define UDEV_PROP_ACTION           "ACTION"
#define UDEV_PROP_SYSTEMD_WANTS    "SYSTEMD_WANTS"
#define UDEV_PROP_DEVPATH          "DEVPATH"
#define UDEV_PROP_MODEL      "ID_MODEL"
#define UDEV_PROP_MODEL_ID   "ID_MODEL_ID"
#define UDEV_PROP_VENDOR     "ID_VENDOR"
#define UDEV_PROP_VENDOR_ID  "ID_VENDOR_ID"

static void printer_device_added (struct udev_device *dev)
{
	const char *printer = NULL;
	const char *path = NULL;
	const char *model = NULL;
	const char *model_id = NULL;
	const char *vendor = NULL;
	const char *vendor_id = NULL;
	char v_vendor[BUF_MAX];
	char v_model[BUF_MAX];
	char *temp;
	int dev_type;

	if (!dev)
		return;

	printer = udev_device_get_property_value(dev, UDEV_PROP_SYSTEMD_WANTS);
	if (!printer)
		return;

	if (!strstr(printer, "printer"))
		return;

	path = udev_device_get_property_value(dev, UDEV_PROP_DEVPATH);
	if (!path)
		return;

	temp = strrchr(path, '/');
	if (!temp)
		return;

	if (strstr(temp, "lp"))
		return;

	dev_type = USBHOST_PRINTER;

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

	launch_ticker_notification(TICKER_NAME_PRINTER_CONNECTED);
}

static void printer_device_removed (struct udev_device *dev)
{
	const char *path = NULL;
	const char *printer = NULL;

	if (!dev)
		return;

	printer = udev_device_get_property_value(dev, UDEV_PROP_SYSTEMD_WANTS);
	if (!printer)
		return;
	if (!strstr(printer, "printer"))
		return;

	path = udev_device_get_property_value(dev, UDEV_PROP_DEVPATH);
	if (!path)
		return;

	if (remove_usb_device_from_list(path, USBHOST_PRINTER) < 0)
		return;

	launch_ticker_notification(TICKER_NAME_DEVICE_DISCONNECTED);
}

static void subsystem_usb_changed (struct udev_device *dev)
{
	const char *action = NULL;
	if (!dev)
		return;

	if (!is_host_uevent_enabled())
		return;

	action = udev_device_get_property_value(dev, UDEV_PROP_ACTION);
	if (!action)
		return;

	if (!strncmp(action, "add", strlen("add"))) {
		printer_device_added(dev);
		return;
	}

	if (!strncmp(action, "remove", strlen("remove"))) {
		printer_device_removed(dev);
		return;
	}
}

const static struct uevent_handler uhs_printer[] = {
	{ USB_SUBSYSTEM       ,     subsystem_usb_changed       ,    NULL    },
};

static int usbhost_printer_init_booting_done(void *data)
{
	int ret, i;

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_printer_init_booting_done);

	for (i = 0 ; i < ARRAY_SIZE(uhs_printer) ; i++) {
		ret = register_uevent_control(&uhs_printer[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	return 0;
}

static void usbhost_printer_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_printer_init_booting_done);
}

static void usbhost_printer_exit(void *data)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(uhs_printer) ; i++) {
		unregister_uevent_control(&uhs_printer[i]);
	}
}

static const struct device_ops usbhost_device_printer_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "usbhost_printer",
	.init     = usbhost_printer_init,
	.exit     = usbhost_printer_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_printer_ops)
