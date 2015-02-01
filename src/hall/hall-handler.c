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
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <device-node.h>
#include <Ecore.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "hall-handler.h"

#define SIGNAL_HALL_STATE	"ChangeState"
#define HALL_PMQOS_TIME		1000 //ms
enum hall_ic_init_type{
	HALL_IC_NOT_READY = 0,
	HALL_IC_INITIALIZED = 1,
};

static int hall_ic_status = HALL_IC_OPENED;
static int hall_ic_initialized = HALL_IC_NOT_READY;

static int hall_ic_get_status(void)
{
	int ret;
	int val = HALL_IC_OPENED;

	if (hall_ic_initialized == HALL_IC_INITIALIZED)
		return hall_ic_status;

	ret = device_get_property(DEVICE_TYPE_HALL, PROP_HALL_STATUS, &val);
	if (ret == 0 && (val == HALL_IC_OPENED || val == HALL_IC_CLOSED))
		hall_ic_status = val;

	return hall_ic_status;
}

static void hall_ic_chgdet_cb(void)
{
	int val = -1;
	int fd, r, ret;
	char buf[2];
	char *arr[1];
	char str_status[32];

	fd = open(HALL_IC_STATUS, O_RDONLY);
	if (fd == -1) {
		_E("%s open error: %s", HALL_IC_STATUS, strerror(errno));
		return;
	}
	r = read(fd, buf, 1);
	close(fd);
	if (r != 1) {
		_E("fail to get hall status %d", r);
		return;
	}

	buf[1] = '\0';

	hall_ic_initialized = HALL_IC_INITIALIZED;

	val = atoi(buf);
	if (val != HALL_IC_OPENED && val != HALL_IC_CLOSED)
		val = HALL_IC_OPENED;
	hall_ic_status = val;

	_I("cover is opened(%d)", hall_ic_status);

	device_notify(DEVICE_NOTIFIER_HALLIC_OPEN, (void *)hall_ic_status);

	snprintf(str_status, sizeof(str_status), "%d", hall_ic_status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_HALL, DEVICED_INTERFACE_HALL,
			SIGNAL_HALL_STATE, "i", arr);

	/* Set touch screen flip mode when cover is closed */
	ret = device_set_property(DEVICE_TYPE_TOUCH,
	    PROP_TOUCH_SCREEN_FLIP_MODE, (hall_ic_status ? 0 : 1));
	if (ret < 0)
		_E("Failed to set touch screen flip mode!");

	/* Set touch key flip mode when cover is closed */
	ret = device_set_property(DEVICE_TYPE_TOUCH,
	    PROP_TOUCH_KEY_FLIP_MODE, (hall_ic_status ? 0 : 1));
	if (ret < 0)
		_E("Failed to set touch key flip mode!");

}

static void hall_edbus_signal_handler(void *data, DBusMessage *msg)
{
	_D("hall_edbus_signal_handler occurs!!!");
}

static DBusMessage *edbus_getstatus_cb(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val;

	val = hall_ic_get_status();
	_D("get hall status %d", val);
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &val);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "getstatus",       NULL,   "i", edbus_getstatus_cb },
	/* Add methods here */
};

static void hall_ic_init(void *data)
{
	int ret, val;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_HALL, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	register_edbus_signal_handler(DEVICED_PATH_HALL, DEVICED_INTERFACE_HALL,
			SIGNAL_HALL_STATE,
			hall_edbus_signal_handler);
	val = hall_ic_get_status();
	_D("get hall status %d", val);
}

static int hall_ic_execute(void *data)
{
	device_notify(DEVICE_NOTIFIER_PMQOS_HALL, (void*)HALL_PMQOS_TIME);
	hall_ic_chgdet_cb();
	return 0;
}

static const struct device_ops hall_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = HALL_IC_NAME,
	.init     = hall_ic_init,
	.status   = hall_ic_get_status,
	.execute  = hall_ic_execute,
};

DEVICE_OPS_REGISTER(&hall_device_ops)
