/*
 * TIMA(TZ based Integrity Measurement Architecture)
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

#include "tima-lkm.h"
#include "tima-pkm.h"

static void tima_init(void *data)
{
	tima_lkm_init();
	tima_pkm_init();
}

static void tima_exit(void *data)
{
	tima_lkm_exit();
	tima_pkm_exit();
}

static const struct device_ops tima_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "tima",
	.init     = tima_init,
	.exit     = tima_exit,
};

DEVICE_OPS_REGISTER(&tima_device_ops)
