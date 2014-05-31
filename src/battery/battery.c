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

#define USE_LOGD 1

#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include "core/log.h"
#include "core/devices.h"
#include "core/udev.h"
#include "device-node.h"
#include "core/device-handler.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"

/*
 * get_capacity - get battery capacity value
 * using device_get_property
 */
static int get_capacity(void)
{
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY, &val);
	if (ret < 0) {
		_E("fail to get battery capacity value");
		val = ret;
	}
	return val;
}

static int update_battery(void *data)
{
	static int capacity = -1;
	static int charge_now = -1;

	if (battery.capacity != capacity) {
		_D("capacity is changed %d -> %d", capacity, battery.capacity);
		capacity = battery.capacity;
	}

	if (battery.charge_now != charge_now) {
		_D("chaging status is changed %d -> %d", charge_now, battery.charge_now);
		charge_now = battery.charge_now;
	}
	return 0;
}

static DBusMessage *edbus_get_percent(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.capacity;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_percent_raw(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret, val;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY_RAW, &val);
	if (ret < 0)
		goto exit;

	if (val > 10000)
		val = 10000;

	ret = val;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_is_full(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.charge_full;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_health(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.health;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "GetPercent",    NULL,   "i", edbus_get_percent },
	{ "GetPercentRaw", NULL,   "i", edbus_get_percent_raw },
	{ "IsFull",        NULL,   "i", edbus_is_full },
	{ "GetHealth",     NULL,   "i", edbus_get_health },
	/* Add methods here */
};

/* battery ops init funcion */
static void battery_init(void *data)
{
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_BATTERY,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	/* get initial battery capacity */
	battery.capacity = get_capacity();
	battery.charge_now = POWER_SUPPLY_STATUS_UNKNOWN;

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_POWER_SUPPLY, update_battery);
}

/* battery ops exit funcion */
static void battery_exit(void *data)
{
	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_POWER_SUPPLY, update_battery);
}

static const struct device_ops battery_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "battery",
	.init     = battery_init,
	.exit     = battery_exit,
};

DEVICE_OPS_REGISTER(&battery_device_ops)
