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

#include <stdbool.h>
#include "core/devices.h"
#include "core/common.h"
#include "core/log.h"
#include "core/device-nodes.h"

#define TOUCH_ON	1
#define TOUCH_OFF	0

#define TOUCH_POWERSAVING_NODE "/sys/class/sec/tsp/mode"

static char *touchscreen_node;
static int powersaving_support;

static int touchscreen_probe(void *data)
{
	int state, ret;

	touchscreen_node = find_device_node(DEVICE_TOUCHSCREEN);
	_D("touchscreen node : %s", touchscreen_node);

	if (!touchscreen_node || !(*touchscreen_node)) {
		touchscreen_node = NULL;
		return -ENOENT;
	}

	ret = sys_get_int(touchscreen_node, &state);
	if (ret < 0)
		touchscreen_node = NULL;

	return ret;
}

/*
 * touchscreen_init - Initialize Touchscreen module
 */
static void touchscreen_init(void *data)
{
	int ret;

	ret = sys_set_int(TOUCH_POWERSAVING_NODE, 0);
	if (ret < 0) {
		_I("There's no powersaving node!");
		powersaving_support = false;
	} else {
		powersaving_support = true;
	}

	_I("touchscreen is available!");
}

static int touchscreen_start(enum device_flags flags)
{
	int ret;

	if (!touchscreen_node || !(*touchscreen_node))
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

	if (!touchscreen_node || !(*touchscreen_node))
		return -ENOENT;

	if (flags & AMBIENT_MODE)
		return 0;

	ret = sys_set_int(touchscreen_node, TOUCH_OFF);
	if (ret < 0)
		_E("Failed to off touch screen!");

	return ret;
}

static int touchscreen_execute(void *data)
{
	int flags = (int)data;
	int state = false;
	int ret;

	if (!powersaving_support)
		return 0;

	if (flags & NORMAL_MODE)
		state = false;
	else if (flags & AMBIENT_MODE)
		state = true;
	else
		return -EINVAL;

	ret = sys_set_int(TOUCH_POWERSAVING_NODE, state);
	if (ret < 0) {
		_E("Failed to set touch powersaving node!");
		return -EIO;
	}

	return 0;
}

static const struct device_ops touchscreen_device_ops = {
	.name     = "touchscreen",
	.probe     = touchscreen_probe,
	.init     = touchscreen_init,
	.start    = touchscreen_start,
	.stop     = touchscreen_stop,
	.execute  = touchscreen_execute,
};

DEVICE_OPS_REGISTER(&touchscreen_device_ops)

