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


#include <stdio.h>
#include <device-node.h>
#include <vconf.h>
#include <fcntl.h>

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/config-parser.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "core/udev.h"
#include "extcon.h"

#define EXTCON_PATH "/sys/class/extcon"
#define STATE_NAME  "STATE"

#define BUF_MAX 256

#define ENV_FILTER            "CHGDET"

#define USB_NAME              "usb"
#define EARJACK_NAME          "earjack"
#define HDMI_NAME             "hdmi"
#define CRADLE_NAME           "cradle"

#define METHOD_DEVICE_CHANGED "device_changed"

static dd_list *extcon_list;

void add_extcon(struct extcon_ops *dev)
{
	DD_LIST_APPEND(extcon_list, dev);
}

void remove_extcon(struct extcon_ops *dev)
{
	DD_LIST_REMOVE(extcon_list, dev);
}

static struct extcon_ops *find_extcon(const char *name)
{
	dd_list *l;
	struct extcon_ops *dev;

	if (!name)
		return NULL;

	DD_LIST_FOREACH(extcon_list, l, dev) {
		if (!strcmp(dev->name, name))
			return dev;
	}

	return NULL;
}

int extcon_get_status(const char *name)
{
	struct extcon_ops *dev;

	if (!name)
		return -EINVAL;

	dev = find_extcon(name);
	if (!dev)
		return -ENOENT;

	return dev->status;
}

static const char *change_to_extcon_name(const char *name)
{
	if (strncmp(name, EARJACK_NAME, sizeof(EARJACK_NAME)) == 0)
		return EXTCON_CABLE_HEADPHONE_OUT;
	else if (strncmp(name, CRADLE_NAME, sizeof(CRADLE_NAME)) == 0)
		return EXTCON_CABLE_DOCK;
	else if (strncmp(name, USB_NAME, sizeof(USB_NAME)) == 0)
		return EXTCON_CABLE_USB;
	else if (strncmp(name, HDMI_NAME, sizeof(HDMI_NAME)) == 0)
		return EXTCON_CABLE_HDMI;
	/**
	 * Not support devices in extcon class.
	 * These device use the existing name.
	 * - earkey
	 * - hdcp
	 * - ch_hdmi_audio
	 * - keyboard
	 */
	return name;
}

static int extcon_update(const char *name, const char *value)
{
	struct extcon_ops *dev;
	const char *extcon_name;
	int status = -1;

	if (!name)
		return -EINVAL;

	/* Mobile specific:
	 * changed deprecated device name to extcon standard device name */
	extcon_name = change_to_extcon_name(name);

	dev = find_extcon(extcon_name);
	if (!dev)
		return -EINVAL;

	/**
	 * Mobile specific:
	 * In kernel common extcon class,
	 * there is always a clear status value.
	 * But for kernel v3.0, platform class sometimes send without status value.
	 * So it's a temporary code and will be regards as error case.
	 */
	if (value)
		status = atoi(value);

	/* Do not invoke update func. if it's the same value */
	if (dev->status == status)
		return 0;

	_I("Changed %s device : %d -> %d", name, dev->status, status);

	/* Mobile specific:
	 * do not update dev->status when value is NULL */
	if (value)
		dev->status = status;
	if (dev->update)
		dev->update(status);

	return 0;
}

static int extcon_parsing_value(const char *value)
{
	char *s, *p;
	char name[NAME_MAX];

	if (!value)
		return -EINVAL;

	s = (char*)value;
	while (s && *s != '\0') {
		p = strchr(s, '=');
		if (!p)
			break;
		memset(name, 0, sizeof(name));
		memcpy(name, s, p-s);
		/* name is env_name and p+1 is env_value */
		extcon_update(name, p+1);
		s = strchr(p, '\n');
		if (!s)
			break;
		s += 1;
	}

	return 0;
}

static void uevent_extcon_handler(struct udev_device *dev)
{
	const char *env_value;
	int ret;

	env_value = udev_device_get_property_value(dev, STATE_NAME);
	if (!env_value)
		return;

	ret = extcon_parsing_value(env_value);
	if (ret < 0)
		_E("fail to parse extcon value : %d", ret);
}

static int extcon_load_uevent(struct parse_result *result, void *user_data)
{
	if (!result)
		return 0;

	if (!result->name || !result->value)
		return 0;

	extcon_update(result->name, result->value);

	return 0;
}

static int get_extcon_init_state(void)
{
	DIR *dir;
	struct dirent entry;
	struct dirent *result;
	char node[BUF_MAX];
	int ret;

	dir = opendir(EXTCON_PATH);
	if (!dir) {
		ret = -errno;
		_E("Cannot open dir (%s, errno:%d)", EXTCON_PATH, ret);
		return ret;
	}

	ret = -ENOENT;
	while (readdir_r(dir, &entry, &result) == 0 && result != NULL) {
		if (result->d_name[0] == '.')
			continue;
		snprintf(node, sizeof(node), "%s/%s/state",
				EXTCON_PATH, result->d_name);
		_I("checking node (%s)", node);
		if (access(node, F_OK) != 0)
			continue;

		ret = config_parse(node, extcon_load_uevent, NULL);
		if (ret < 0)
			_E("fail to parse %s data : %d", node, ret);
	}

	if (dir)
		closedir(dir);

	return 0;
}

static DBusMessage *dbus_get_usb_id(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ID, &val);
	if (ret >= 0)
		ret = val;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_get_muic_adc_enable(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_MUIC_ADC_ENABLE, &val);
	if (ret >= 0)
		ret = val;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_set_muic_adc_enable(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	ret = device_set_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_MUIC_ADC_ENABLE, val);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;

}

static DBusMessage *dbus_device_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret = 0;
	int argc;
	char *type_str;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	extcon_update(argv[0], argv[1]);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_get_extcon_status(E_DBus_Object *obj,
		DBusMessage *msg)
{
	DBusError err;
	struct extcon_ops *dev;
	char *str;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
				DBUS_TYPE_STRING, &str,
				DBUS_TYPE_INVALID)) {
		_E("fail to get message : %s - %s", err.name, err.message);
		dbus_error_free(&err);
		ret = -EINVAL;
		goto error;
	}

	dev = find_extcon(str);
	if (!dev) {
		_E("fail to matched extcon device : %s", str);
		ret = -ENOENT;
		goto error;
	}

	ret = dev->status;
	_D("Extcon device : %s, status : %d", dev->name, dev->status);

error:
	return make_reply_message(msg, ret);
}

static DBusMessage *dbus_enable_device(E_DBus_Object *obj, DBusMessage *msg)
{
	char *device;
	int ret;

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_STRING, &device, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	ret = extcon_update(device, "1");

out:
	return make_reply_message(msg, ret);
}

static DBusMessage *dbus_disable_device(E_DBus_Object *obj, DBusMessage *msg)
{
	char *device;
	int ret;

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_STRING, &device, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	ret = extcon_update(device, "0");

out:
	return make_reply_message(msg, ret);
}

static void switch_uevent_changed(struct udev_device *dev)
{
	const char *env_name;
	const char *env_value;
	int ret;

	env_name = udev_device_get_property_value(dev, "SWITCH_NAME");
	env_value = udev_device_get_property_value(dev, "SWITCH_STATE");
	if (!env_name || !env_value)
		return;

	_D("switch : %s %s", env_name, env_value);
	ret = extcon_update(env_name, env_value);
	if (ret < 0)
		_E("fail to update extcon status (%s-%s) : %d",
				env_name, env_value, ret);
}

static void platform_uevent_changed(struct udev_device *dev)
{
	const char *env_value;
	int ret;

	env_value = udev_device_get_property_value(dev, ENV_FILTER);
	if (!env_value)
		return;

	_D("platform : %s", env_value);
	ret = extcon_update(env_value, NULL);
	if (ret < 0)
		_E("fail to update extcon status (%s) : %d",
				env_value, ret);
}

static const struct uevent_handler extcon_uevent_ops[] = {
	/**
	 * Mobile specific:
	 * The switch and platform are not a standard way
	 * to get the status of extcon devices.
	 */
	{.subsystem = SWITCH_SUBSYSTEM,
	.uevent_func = switch_uevent_changed},
	{.subsystem = PLATFORM_SUBSYSTEM,
	.uevent_func = platform_uevent_changed},
	{.subsystem = EXTCON_SUBSYSTEM,
	.uevent_func = uevent_extcon_handler},
};

static const struct edbus_method edbus_methods[] = {
	{ "GetUsbId",              NULL,    "i", dbus_get_usb_id },
	{ "GetMuicAdcEnable",      NULL,    "i", dbus_get_muic_adc_enable },
	{ "SetMuicAdcEnable",       "i",    "i", dbus_set_muic_adc_enable },
	{ METHOD_DEVICE_CHANGED, "siss",    "i", dbus_device_handler },
	{ "GetStatus",              "s",    "i", dbus_get_extcon_status },
	{ "enable",                 "s",    "i", dbus_enable_device },  /* for devicectl */
	{ "disable",                "s",    "i", dbus_disable_device }, /* for devicectl */
};

static void extcon_init(void *data)
{
	int ret;
	int i;
	dd_list *l;
	struct extcon_ops *dev;

	if (!extcon_list)
		return;

	/* initialize extcon devices */
	DD_LIST_FOREACH(extcon_list, l, dev) {
		_I("[extcon] init (%s)", dev->name);
		if (dev->init)
			dev->init(data);
	}

	/* register extcon uevent */
	for (i = 0; i < ARRAY_SIZE(extcon_uevent_ops); i++)
		register_kernel_uevent_control(&extcon_uevent_ops[i]);

	/* load the initialize value by accessing the node directly */
	ret = get_extcon_init_state();
	if (ret < 0)
		_E("fail to init extcon nodes : %d", ret);

	/* register extcon object first */
	ret = register_edbus_interface_and_method(DEVICED_PATH_EXTCON,
			DEVICED_INTERFACE_EXTCON,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static void extcon_exit(void *data)
{
	dd_list *l;
	struct extcon_ops *dev;
	int i;

	/* unreigster extcon uevent */
	for (i = 0; i < ARRAY_SIZE(extcon_uevent_ops); i++)
		unregister_kernel_uevent_control(&extcon_uevent_ops[i]);

	/* deinitialize extcon devices */
	DD_LIST_FOREACH(extcon_list, l, dev) {
		_I("[extcon] deinit (%s)", dev->name);
		if (dev->exit)
			dev->exit(data);
	}
}

static const struct device_ops extcon_device_ops = {
	.name = "extcon",
	.init = extcon_init,
	.exit = extcon_exit,
};

DEVICE_OPS_REGISTER(&extcon_device_ops)
