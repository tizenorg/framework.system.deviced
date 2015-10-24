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
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <vconf.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/timerfd.h>

#include "core/log.h"
#include "core/devices.h"
#include "display/poll.h"
#include "display/core.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "core/device-notifier.h"

#ifndef TFD_TIMER_CANCELON_SET
#define TFD_TIMER_CANCELON_SET (1<<1)
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC	0x2000000
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	0x4000
#endif

#ifndef TFD_CLOEXEC
#define TFD_CLOEXEC	O_CLOEXEC
#endif

#ifndef TFD_NONBLOCK
#define TFD_NONBLOCK	O_NONBLOCK
#endif

#define TIME_CHANGE_SIGNAL     "STimeChanged"

static const time_t default_time = 2147483645; /* max(32bit) -3sec */
static Ecore_Fd_Handler *tfdh; /* tfd change noti */

static Eina_Bool tfd_cb(void *data, Ecore_Fd_Handler *fd_handler);
static int timerfd_check_stop(int fd);
static int timerfd_check_start(void);

static void time_changed_broadcast(void)
{
	broadcast_edbus_signal(DEVICED_PATH_TIME, DEVICED_INTERFACE_TIME,
			TIME_CHANGE_SIGNAL, NULL, NULL);
}

static int timerfd_check_start(void)
{
	int tfd;
	int ret;
	struct itimerspec tmr;

	tfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK|TFD_CLOEXEC);
	if (tfd == -1) {
		_E("error timerfd_create() %d", errno);
		tfdh = NULL;
		return -1;
	}

	tfdh = ecore_main_fd_handler_add(tfd, ECORE_FD_READ, tfd_cb, NULL, NULL, NULL);
	if (!tfdh) {
		_E("error ecore_main_fd_handler_add");
		return -1;
	}
	memset(&tmr, 0, sizeof(tmr));
	tmr.it_value.tv_sec = default_time;
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME|TFD_TIMER_CANCELON_SET, &tmr, NULL);
	if (ret < 0) {
		_E("error timerfd_settime() %d", errno);
		return -1;
	}
	return 0;
}

static int timerfd_check_stop(int tfd)
{
	if (tfdh) {
		ecore_main_fd_handler_del(tfdh);
		tfdh = NULL;
	}
	if (tfd >= 0) {
		close(tfd);
		tfd = -1;
	}
	return 0;
}

static Eina_Bool tfd_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	int tfd = -1;
	u_int64_t ticks;
	int ret = -1;

	ret = ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ);
	if (!ret) {
		_E("error ecore_main_fd_handler_get()");
		goto out;
	}

	tfd = ecore_main_fd_handler_fd_get(fd_handler);
	if (tfd == -1) {
		_E("error ecore_main_fd_handler_fd_get()");
		goto out;
	}

	ret = read(tfd, &ticks, sizeof(ticks));
	if (ret < 0 && errno == ECANCELED) {
		time_changed_broadcast();
		timerfd_check_stop(tfd);
		_D("NOTIFICATION here");
		timerfd_check_start();
	} else {
		_E("unexpected read (err:%d)", errno);
	}
out:
	return EINA_TRUE;
}

static int time_lcd_changed_cb(void *data)
{
	int lcd_state;
	int tfd = -1;

	if (!data)
		goto out;

	lcd_state = *(int *)data;

	if (lcd_state < S_LCDOFF)
		goto restart;

	lcd_state = check_lcdoff_lock_state();
	if (lcd_state || !tfdh)
		goto out;
	tfd = ecore_main_fd_handler_fd_get(tfdh);
	if (tfd == -1)
		goto out;

	_D("stop tfd");
	timerfd_check_stop(tfd);
	goto out;
restart:
	if (tfdh)
		return 0;
	_D("restart tfd");
	timerfd_check_start();
out:
	return 0;
}

static void time_init(void *data)
{
	if (timerfd_check_start() == -1)
		_E("fail system time change detector init");
	register_notifier(DEVICE_NOTIFIER_LCD, time_lcd_changed_cb);
}

static const struct device_ops time_device_ops = {
	.name     = "time",
	.init     = time_init,
};

DEVICE_OPS_REGISTER(&time_device_ops)
