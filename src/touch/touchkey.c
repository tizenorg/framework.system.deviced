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

static char *touchkey_node;

static int touchkey_probe(void *data)
{
	int state, ret;

	touchkey_node = find_device_node(DEVICE_TOUCHKEY);
	_D("touchkey node : %s", touchkey_node);

	if (!touchkey_node || !(*touchkey_node)) {
		touchkey_node = NULL;
		return -ENOENT;
	}

	ret = sys_get_int(touchkey_node, &state);
	if (ret < 0)
		touchkey_node = NULL;

	return ret;
}

/*
 * touchkey_init - Initialize Touchkey module
 */
static void touchkey_init(void *data)
{
	_I("touchkey is available!");
}

static int touchkey_start(enum device_flags flags)
{
	int ret;

	if (!touchkey_node || !(*touchkey_node))
		return -ENOENT;

	ret = sys_set_int(touchkey_node, TOUCH_ON);
	if (ret < 0)
		_E("Failed to on touch key!");

	return ret;
}

static int touchkey_stop(enum device_flags flags)
{
	int ret;

	if (!touchkey_node || !(*touchkey_node))
		return -ENOENT;

	ret = sys_set_int(touchkey_node, TOUCH_OFF);
	if (ret < 0)
		_E("Failed to off touch key!");

	return ret;
}

static int touchkey_dump(FILE *fp, int mode, void *dump_data)
{
	if (touchkey_node)
		LOG_DUMP(fp, "touchkey is available! %s\n", touchkey_node);
	else
		LOG_DUMP(fp, "touchkey is unavailable!\n");

	return 0;
}

static const struct device_ops touchkey_device_ops = {
	.name     = "touchkey",
	.probe    = touchkey_probe,
	.init     = touchkey_init,
	.start    = touchkey_start,
	.stop     = touchkey_stop,
	.dump     = touchkey_dump,
};

DEVICE_OPS_REGISTER(&touchkey_device_ops)

