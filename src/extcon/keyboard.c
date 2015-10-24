/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <vconf.h>

#include "core/log.h"
#include "core/common.h"
#include "extcon.h"

static int keyboard_update(int status)
{
	_I("jack - keyboard changed %d", status);
	if (status != 1)
		status = 0;
	vconf_set_int(VCONFKEY_SYSMAN_SLIDING_KEYBOARD, status);

	return 0;
}

static struct extcon_ops extcon_keyboard_ops = {
	.name   = EXTCON_CABLE_KEYBOARD,
	.update = keyboard_update,
};

EXTCON_OPS_REGISTER(extcon_keyboard_ops)
