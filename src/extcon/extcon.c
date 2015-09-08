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

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/devices.h"

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

static const struct edbus_method edbus_methods[] = {
	{ "GetUsbId",           NULL,    "i", dbus_get_usb_id },
	{ "GetMuicAdcEnable",   NULL,    "i", dbus_get_muic_adc_enable },
	{ "SetMuicAdcEnable",    "i",    "i", dbus_set_muic_adc_enable },
};

static void extcon_init(void *data)
{
	int ret, otg_mode = 0;

	ret = register_edbus_method(DEVICED_PATH_EXTCON, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	if (vconf_get_bool(VCONFKEY_SETAPPL_USB_OTG_MODE, &otg_mode) < 0)
		_E("failed to get VCONFKEY_SETAPPL_USB_OTG_MODE");

	/* if otg_mode key is on, deviced turns on the otg_mode */
	if (otg_mode)
		device_set_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_MUIC_ADC_ENABLE, otg_mode);
}

static const struct device_ops extcon_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "extcon",
	.init     = extcon_init,
};

DEVICE_OPS_REGISTER(&extcon_device_ops)
