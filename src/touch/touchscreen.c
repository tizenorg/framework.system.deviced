/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/devices.h"
#include "core/common.h"
#include "core/log.h"

#define TOUCH_ON	1
#define TOUCH_OFF	0

static char *touchscreen_node;

/*
 * touchscreen_init - Initialize Touchscreen module
 */
static void touchscreen_init(void *data)
{
	if (touchscreen_node)
		return;

	touchscreen_node = getenv("PM_TOUCHSCREEN");
	_D("touchscreen node : %s", touchscreen_node);
}

static int touchscreen_start(enum device_flags flags)
{
	int ret;

	touchscreen_init((void *)NORMAL_MODE);
	if (!touchscreen_node)
		return -ENOENT;

	if (flags & TOUCH_SCREEN_OFF_MODE)
		return 0;

	ret = sys_set_int(touchscreen_node, TOUCH_ON);
	if (ret < 0)
		_E("Failed to on touch screen!");

	return ret;
}

static int touchscreen_stop(enum device_flags flags)
{
	int ret;

	touchscreen_init((void *)NORMAL_MODE);
	if (!touchscreen_node)
		return -ENOENT;

	if (flags & AMBIENT_MODE)
		return 0;

	ret = sys_set_int(touchscreen_node, TOUCH_OFF);
	if (ret < 0)
		_E("Failed to off touch screen!");

	return ret;
}

static const struct device_ops touchscreen_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "touchscreen",
	.init     = touchscreen_init,
	.start    = touchscreen_start,
	.stop     = touchscreen_stop,
};

DEVICE_OPS_REGISTER(&touchscreen_device_ops)

