/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include <time.h>

#include "core/log.h"
#include "core/device-notifier.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "display/core.h"
#include "sleep.h"

#define SLEEP_CHECK_DURATION 3600 /* 1hour */

enum sleep_enum {
	DEVICE_AWAKE = 0,
	DEVICE_SLEEP = 1,
};

struct sleep_type {
	int sleep_status;
};

static struct sleep_type monitor = {
	.sleep_status = -1,
};
static time_t old = 0;
static time_t diff = 0;

static void sleep_changed(int status)
{
	if (monitor.sleep_status == status)
		return;
	monitor.sleep_status = status;
	_I("sleep status %d", status);
}

static void check_sleep(void)
{
	time_t now;

	if (monitor.sleep_status == DEVICE_SLEEP)
		return;

	time(&now);
	diff = now - old;

	if (diff > SLEEP_CHECK_DURATION)
		sleep_changed(DEVICE_SLEEP);
}

static int display_changed(void *data)
{
	enum state_t state = (enum state_t)data;

	if (state == S_LCDOFF || state == S_SLEEP)
		goto out;

	time(&old);
	diff = 0;

	sleep_changed(DEVICE_AWAKE);
out:
	return 0;
}

static int booting_done(void *data)
{
	time(&old);
	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);
	return 0;
}

static void sleep_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static void sleep_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);
}

static int sleep_execute(void *data)
{
	check_sleep();
	return 0;
}

static const struct device_ops sleep_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = DEVICE_SLEEP_OPS,
	.init     = sleep_init,
	.exit     = sleep_exit,
	.execute  = sleep_execute,
};

DEVICE_OPS_REGISTER(&sleep_device_ops)

