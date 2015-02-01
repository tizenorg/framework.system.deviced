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

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static int lowmem_launch(void *data)
{
	char *param[2];
	struct popup_data *params = (struct popup_data *)data;

	param[0] = params->key;
	param[1] = params->value;

	return dbus_method_async(POPUP_BUS_NAME,
			POPUP_PATH_LOWMEM,
			POPUP_INTERFACE_LOWMEM,
			POPUP_METHOD_LAUNCH, "ss", param);
}


static const struct apps_ops lowmem_ops = {
	.name	= "lowmem-syspopup",
	.launch	= lowmem_launch,
};

APPS_OPS_REGISTER(&lowmem_ops)
