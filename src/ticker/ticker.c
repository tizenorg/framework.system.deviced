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

#include <vconf.h>
#include "core/log.h"
#include "ticker.h"
#include "core/devices.h"
#include "core/edbus-handler.h"

void show_ticker_noti(char *name)
{
	int ret, ret_val;
	char *param[1];

	if (!name)
		return ;

	param[0] = name;

	ret_val = dbus_method_async(POPUP_BUS_NAME,
			POPUP_PATH_TICKER,
			POPUP_INTERFACE_TICKER,
			"TickerNotiOn", "s", param);
	return;
}

void insert_ticker_queue(char *name)
{
	/* TODO: Implement queue for ticker noti */
}

static void ticker_init(void *data)
{
	const struct ticker_ops *dev;
	struct ticker_data *input;
	static int initialized = 0;

	if (!initialized) {
		initialized = 1;
		return;
	}

	input = (struct ticker_data *)data;
	if (input == NULL || input->name == NULL)
		return;

	if (input->type == WITHOUT_QUEUE) {
		_E("without queue");
		show_ticker_noti(input->name);
	} else {
		_E("with queue");
		insert_ticker_queue(input->name);
	}
}

static const struct device_ops ticker_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "ticker",
	.init     = ticker_init,
};

DEVICE_OPS_REGISTER(&ticker_device_ops)
