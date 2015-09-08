/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#include "icd-integrity.h"

static void icd_init(void *data)
{
	icd_check_integrity();
}

static void icd_exit(void *data)
{
}

static const struct device_ops icd_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "icd",
	.init     = icd_init,
	.exit     = icd_exit,
};

DEVICE_OPS_REGISTER(&icd_device_ops)
