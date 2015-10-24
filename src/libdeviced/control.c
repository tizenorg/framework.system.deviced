/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <vconf.h>
#include <errno.h>

#include "log.h"
#include "dbus.h"
#include "common.h"
#include "dd-control.h"

#define CONTROL_HANDLER_NAME		"control"
#define CONTROL_GETTER_NAME			"getcontrol"

static int deviced_control_common(int device, bool enable)
{
	char *pa[5];
	int ret;
	char buf_pid[6];
	char buf_dev[3];
	char buf_enable[2];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_dev, sizeof(buf_dev), "%d", device);
	snprintf(buf_enable, sizeof(buf_enable), "%d", enable);

	pa[0] = CONTROL_HANDLER_NAME;
	pa[1] = "3";
	pa[2] = buf_pid;
	pa[3] = buf_dev;
	pa[4] = buf_enable;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			pa[0], "sisss", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_SYSNOTI, pa[0], ret);
	return ret;
}

static int deviced_get_control(int device, void *data)
{
	char *pa[4];
	int ret;
	char buf_pid[6];
	char buf_dev[3];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_dev, sizeof(buf_dev), "%d", device);

	pa[0] = CONTROL_GETTER_NAME;
	pa[1] = "2";
	pa[2] = buf_pid;
	pa[3] = buf_dev;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			pa[0], "siss", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_SYSNOTI, pa[0], ret);
	return ret;
}


/*
 * example of control api
 * API int deviced_display_control(bool enable)
 * {
 *	return deviced_control_common(DEVICE_CONTROL_DISPLAY, enable);
 * }
 */

API int deviced_mmc_control(bool enable)
{
	return deviced_control_common(DEVICE_CONTROL_MMC, enable);
}

API int deviced_usb_control(bool enable)
{
	return deviced_control_common(DEVICE_CONTROL_USBCLIENT, enable);
}

API int deviced_get_usb_control(void)
{
	return deviced_get_control(DEVICE_CONTROL_USBCLIENT, NULL);
}

API int deviced_rgbled_control(bool enable)
{
	return deviced_control_common(DEVICE_CONTROL_RGBLED, enable);
}
