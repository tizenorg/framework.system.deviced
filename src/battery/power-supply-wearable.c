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


#include <vconf.h>
#include <stdbool.h>

#include "core/log.h"
#include "core/common.h"
#include "display/poll.h"
#include "display/core.h"

#define LCD_DIM_TIME_IN_BATTERY_FULL	20000	/* ms */

int check_power_supply_noti(void)
{
	int block;

	if (vconf_get_bool("db/setting/blockmode_wearable", &block) != 0)
		return 1;
	if (block == 0)
		return 1;
	return 0;
}

int update_power_lcd_timeout(void)
{
	int state;

	state = pm_get_power_state();

	if (state == S_NORMAL || state == S_LCDDIM)
		return false;

	pm_lock_internal(INTERNAL_LOCK_BATTERY_FULL, LCD_DIM,
	    GOTO_STATE_NOW, LCD_DIM_TIME_IN_BATTERY_FULL);

	return true;
}

