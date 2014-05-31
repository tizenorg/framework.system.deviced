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


#include "core/log.h"
#include "core/common.h"
#include "apps.h"
#include "core/edbus-handler.h"
#include "core/launch.h"

#ifdef MICRO_DD
#define POWEROFF_POPUP_BUS_NAME POPUP_BUS_NAME".Poweroff"
#else
#define POWEROFF_POPUP_BUS_NAME POPUP_BUS_NAME
#endif

static int poweroff_launch(void *data)
{
	return dbus_method_async(POWEROFF_POPUP_BUS_NAME,
			POPUP_PATH_POWEROFF,
			POPUP_INTERFACE_POWEROFF,
			POPUP_METHOD_LAUNCH, NULL, NULL);
}


static const struct apps_ops poweroff_ops = {
	.name	= "poweroff-syspopup",
	.launch	= poweroff_launch,
};

APPS_OPS_REGISTER(&poweroff_ops)
