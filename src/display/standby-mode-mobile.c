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
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "core/common.h"
#include "core/list.h"

static int standby_mode;
static int standby_state;
static Eina_List *standby_mode_list;
static int (*basic_action) (int);

int get_standby_state(void)
{
	return standby_state;
}

void set_standby_state(bool state)
{
	if (standby_state != state)
		standby_state = state;
}

static int standby_action(int timeout)
{
	const struct device_ops *ops = NULL;

	if (backlight_ops.standby(false) < 0) {
		_E("Fail to start standby mode!");
		return -EIO;
	}
	if (CHECK_OPS(keyfilter_ops, backlight_enable))
		keyfilter_ops->backlight_enable(false);

	ops = find_device("touchkey");
	if (!check_default(ops))
		ops->stop(NORMAL_MODE);

	ops = find_device("touchscreen");
	if (!check_default(ops))
		ops->stop(NORMAL_MODE);

	set_standby_state(true);

	_I("standby mode (only LCD OFF, But phone is working normal)");
	reset_timeout(timeout);

	return 0;
}

void set_standby_mode(pid_t pid, int enable)
{
	Eina_List *l = NULL;
	Eina_List *l_next = NULL;
	void *data = 0;

	if (enable) {
		EINA_LIST_FOREACH(standby_mode_list, l, data)
			if (pid == (pid_t)((intptr_t)data)) {
				_E("%d already acquired standby mode", pid);
				return;
			}
		EINA_LIST_APPEND(standby_mode_list, (void *)((intptr_t)pid));
		_I("%d acquire standby mode", pid);
		if (standby_mode)
			return;
		standby_mode = true;
		basic_action = states[S_LCDOFF].action;
		states[S_LCDOFF].action = standby_action;
		change_trans_table(S_LCDOFF, S_LCDOFF);
		_I("Standby mode is enabled!");
	} else {
		if (!standby_mode)
			return;
		EINA_LIST_FOREACH_SAFE(standby_mode_list, l, l_next, data)
			if (pid == (pid_t)((intptr_t)data)) {
				standby_mode_list = eina_list_remove_list(
						    standby_mode_list, l);
				_I("%d release standby mode", pid);
			}
		if (standby_mode_list != NULL)
			return;
		set_standby_state(false);
		standby_mode = false;
		if (basic_action != NULL)
			states[S_LCDOFF].action = basic_action;

		change_trans_table(S_LCDOFF, S_SLEEP);

		pm_change_internal(INTERNAL_LOCK_STANDBY_MODE, LCD_NORMAL);
		_I("Standby mode is disabled!");
	}
}

void print_standby_mode(FILE *fp)
{
	char pname[PATH_MAX];
	Eina_List *l = NULL;
	void *data = 0;

	LOG_DUMP(fp, "\n\nstandby mode is on\n");

	EINA_LIST_FOREACH(standby_mode_list, l, data) {
		get_pname((pid_t)((intptr_t)data), pname);
		LOG_DUMP(fp, "  standby mode acquired by pid %d - process %s\n",
		    data, pname);
	}
}

