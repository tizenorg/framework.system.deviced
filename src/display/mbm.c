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

#include <stdbool.h>
#include <fcntl.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "weaks.h"
#include "core/common.h"
#include "core/device-nodes.h"

#define ON		"on"
#define OFF		"off"

static char *mbm_path;

int mbm_get_state(void)
{
	char state[4];
	int ret, mbm;

	if (!mbm_path)
		return -ENODEV;

	ret = sys_get_str(mbm_path, state);
	if (ret < 0)
		return ret;

	if (!strncmp(state, ON, strlen(ON)))
		mbm = true;
	else if (!strncmp(state, OFF, strlen(OFF)))
		mbm = false;
	else
		mbm = -EINVAL;

	return mbm;
}

int mbm_set_state(int mbm)
{
	if (!mbm_path)
		return -ENODEV;

	return sys_set_str(mbm_path, (mbm ? ON : OFF));
}

void mbm_turn_on(void)
{
	if (!mbm_get_state())
		mbm_set_state(true);
}

void mbm_turn_off(void)
{
	if (mbm_get_state())
		mbm_set_state(false);
}

static void mbm_init(void *data)
{
	int fd, ret;

	/* Check MBM node is valid */
	mbm_path = find_device_node(DEVICE_MBM);
	fd = open(mbm_path, O_RDONLY);
	if (fd < 0) {
		_E("Failed to open mbm node %s", mbm_path);
		mbm_path = NULL;
		return;
	}
	_I("mbm node : %s", mbm_path);
	close(fd);
}

static const struct display_ops display_mbm_ops = {
	.name     = "mbm",
	.init     = mbm_init,
};

DISPLAY_OPS_REGISTER(&display_mbm_ops)

