/*
 * LKM in TIMA(TZ based Integrity Measurement Architecture)
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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
#include <assert.h>
#include <limits.h>
#include <libudev.h>
#include <Ecore.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/udev.h"

#include "noti.h"

/*
 * tzapp interface
 */
#define LKM_FAIL		-1
#define LKM_SUCCESS		0
#define LKM_ERROR		1

/*
 * udev
 */
#define TIMA_SUBSYSTEM		"tima"
#define TIMA_STATUS		"TIMA_STATUS"
#define TIMA_RESULT		"TIMA_RESULT"

static struct udev *udev;
static struct udev_monitor *mon;
static int ufd;
static Ecore_Fd_Handler	*ufdh;

static int noti_id = 0;

static void tima_notification(int status)
{
	switch (status) {
	case LKM_FAIL:
		_I("certification failed");
		break;
	default:
		_E("internal error (%d)", status);
		break;
	}

	noti_id = tima_lkm_prevention_noti_on();
	if (noti_id < 0) {
		_E("FAIL: tima_lkm_prevention_noti_on()");
		noti_id = 0;
	}
}

/*
 * tima_uevent_cb - uevent callback
 *
 * This callback receive the uevent from kernel module driver
 */
static Eina_Bool tima_uevent_cb(void *data, Ecore_Fd_Handler *handler)
{
	struct udev_device *dev;
	struct udev_list_entry *list, *entry;
	const char *name, *value, *msg;
	int status;

	dev = udev_monitor_receive_device(mon);
	if (!dev)
		return EINA_TRUE;

	/* Getting the First element of the device entry list */
	list = udev_device_get_properties_list_entry(dev);
	if (!list) {
		_E("can't get udev_device_get_properties_list_entry()");
		goto err_unref_dev;
	}

	udev_list_entry_foreach(entry, list) {
		name = udev_list_entry_get_name(entry);

		if (!strcmp(name, TIMA_RESULT)) {
			msg = udev_list_entry_get_value(entry);
			_D("tzapp: msg(%s)", msg);
		}

		if (!strcmp(name, TIMA_STATUS)) {
			value = udev_list_entry_get_value(entry);
			status = atoi(value);
			_D("tzapp: ret(%d)", status);

			if (status != LKM_SUCCESS) {
				tima_notification(status);
				goto err_unref_dev;
			}
		}
	}
	udev_device_unref(dev);
	return EINA_TRUE;

err_unref_dev:
	udev_device_unref(dev);
err:
	return EINA_FALSE;
}

/*
 * tima_uevent_stop - disable udev event control
 */
static int tima_uevent_stop(void)
{
	if (ufdh) {
		ecore_main_fd_handler_del(ufdh);
		ufdh = NULL;
	}

	if (ufd >= 0) {
		close(ufd);
		ufd = -1;
	}

	if (mon) {
		struct udev_device *dev = udev_monitor_receive_device(mon);
		if (dev) {
			udev_device_unref(dev);
			dev = NULL;
		}
		udev_monitor_unref(mon);
		mon = NULL;
	}

	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}

	return 0;
}

/*
 * tima_uevent_start - enable udev event for TIMA
 */
static int tima_uevent_start(void)
{
	int ret;

	udev = udev_new();
	if (!udev) {
		_E("can't create udev");
		goto stop;
	}

	mon = udev_monitor_new_from_netlink(udev, UDEV);
	if (!mon) {
		_E("can't udev_monitor create");
		goto stop;
	}

	ret = udev_monitor_set_receive_buffer_size(mon, 1024);
	if (ret < 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	ret = udev_monitor_filter_add_match_subsystem_devtype(mon, TIMA_SUBSYSTEM, NULL);
	if (ret < 0) {
		_E("can't apply subsystem filter");
		goto stop;
	}

	ret = udev_monitor_filter_update(mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	ufd = udev_monitor_get_fd(mon);
	if (ufd < 0) {
		_E("can't udev_monitor_get_fd");
		goto stop;
	}

	ufdh = ecore_main_fd_handler_add(ufd, ECORE_FD_READ, tima_uevent_cb, NULL, NULL, NULL);
	if (!ufdh) {
		_E("can't ecore_main_fd_handler_add");
		goto stop;
	}

	ret = udev_monitor_enable_receiving(mon);
	if (ret < 0) {
		_E("can't unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	tima_uevent_stop();
	return -EINVAL;
}

void tima_lkm_init(void)
{
	tima_uevent_start();
}

void tima_lkm_exit(void)
{
	tima_uevent_stop();
}
