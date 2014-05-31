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

#define UDEV_PROP_GPHOTO2_DRIVER   "GPHOTO2_DRIVER"
#define UDEV_PROP_ACTION           "ACTION"
#define UDEV_PROP_DEVNAME          "DEVNAME"
#define UDEV_PROP_MODEL      "ID_MODEL"
#define UDEV_PROP_MODEL_ID   "ID_MODEL_ID"
#define UDEV_PROP_VENDOR     "ID_VENDOR"
#define UDEV_PROP_VENDOR_ID  "ID_VENDOR_ID"

/* Popup */
#define USBOTG_SYSPOPUP        "usbotg-syspopup"
#define METHOD_CAMERA          "CameraPopupLaunch"
#define PARAM_CAMERA_ADD       "camera_add"
#define PARAM_CAMERA_REMOVE    "camera_remove"

static void camera_device_added (struct udev_device *dev)
{
	const char *driver = NULL;
	const char *name = NULL;
	const char *model = NULL;
	const char *model_id = NULL;
	const char *vendor = NULL;
	const char *vendor_id = NULL;
	char v_vendor[BUF_MAX];
	char v_model[BUF_MAX];
	int dev_type;

	if (!dev)
		return;

	driver = udev_device_get_property_value(dev, UDEV_PROP_GPHOTO2_DRIVER);
	if (!driver)
		return;

	dev_type = USBHOST_CAMERA;

	name = udev_device_get_property_value(dev, UDEV_PROP_DEVNAME);
	if (!name) {
		_E("cannot get device name");
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


	if (add_usb_device_to_list(dev_type, name, v_model, v_vendor) < 0) {
		_E("Failed to add device to dev_list");
		return;
	}

	launch_ticker_notification(TICKER_NAME_CAMERA_CONNECTED);

	launch_host_syspopup(USBOTG_SYSPOPUP, METHOD_CAMERA,
			POPUP_KEY_CONTENT, PARAM_CAMERA_ADD,
			NULL, NULL);

	send_msg_camera_added();
}

static void camera_device_removed (struct udev_device *dev)
{
	const char *name = NULL;

	if (!dev)
		return;

	name= udev_device_get_property_value(dev, UDEV_PROP_DEVNAME);
	if (!name) {
		return;
	}

	if (remove_usb_device_from_list(name, USBHOST_CAMERA) < 0) {
		return;
	}

	launch_ticker_notification(TICKER_NAME_DEVICE_DISCONNECTED);

	launch_host_syspopup(USBOTG_SYSPOPUP, METHOD_CAMERA,
			POPUP_KEY_CONTENT, PARAM_CAMERA_REMOVE,
			NULL, NULL);

	send_msg_camera_removed();
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
		camera_device_added(dev);
		return;
	}

	if (!strncmp(action, "remove", strlen("remove"))) {
		camera_device_removed(dev);
		return;
	}
}

const static struct uevent_handler uhs_camera[] = {
	{ USB_SUBSYSTEM       ,     subsystem_usb_changed       ,    NULL    },
};

static int usbhost_camera_init_booting_done(void *data)
{
	int ret, i;

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_camera_init_booting_done);

	for (i = 0 ; i < ARRAY_SIZE(uhs_camera) ; i++) {
		ret = register_uevent_control(&uhs_camera[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	return 0;
}

static void usbhost_camera_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_camera_init_booting_done);
}

static void usbhost_camera_exit(void *data)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(uhs_camera) ; i++) {
		unregister_uevent_control(&uhs_camera[i]);
	}
}

static const struct device_ops usbhost_device_camera_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "usbhost_camera",
	.init     = usbhost_camera_init,
	.exit     = usbhost_camera_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_camera_ops)
