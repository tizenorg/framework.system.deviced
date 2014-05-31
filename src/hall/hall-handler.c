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
#include "core/data.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "hall-handler.h"

#define SIGNAL_HALL_STATE	"ChangeState"

static int hall_ic_status = HALL_IC_OPENED;

static int hall_ic_get_status(void)
{
	return hall_ic_status;
}

static void hall_ic_chgdet_cb(struct main_data *ad)
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

	hall_ic_status = atoi(buf);
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

static int hall_action(int argc, char **argv)
{
	int i;
	int pid;
	int prop;

	if (strncmp(argv[0], HALL_IC_NAME, strlen(HALL_IC_NAME)) == 0)
		hall_ic_chgdet_cb(NULL);
	return 0;
}

static void hall_edbus_signal_handler(void *data, DBusMessage *msg)
{
	_D("hall_edbus_signal_handler occurs!!!");
}

static DBusMessage *edbus_getstatus_cb(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_HALL, PROP_HALL_STATUS, &val);
	if (ret >= 0)
		ret = val;

	_D("get hall status %d, %d", val, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
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

	register_action(PREDEF_HALL_IC, hall_action, NULL, NULL);

	register_edbus_signal_handler(DEVICED_PATH_HALL, DEVICED_INTERFACE_HALL,
			SIGNAL_HALL_STATE,
			hall_edbus_signal_handler);

	if (device_get_property(DEVICE_TYPE_HALL, PROP_HALL_STATUS, &val) >= 0)
		hall_ic_status = val;
}

static const struct device_ops hall_device_ops = {
	.priority	= DEVICE_PRIORITY_NORMAL,
	.name		= HALL_IC_NAME,
	.init		= hall_ic_init,
	.status		= hall_ic_get_status,
};

DEVICE_OPS_REGISTER(&hall_device_ops)
