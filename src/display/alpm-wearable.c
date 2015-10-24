/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "weaks.h"
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "core/device-nodes.h"

#define ON		"on"
#define OFF		"off"

#define SIGNAL_ALPM_ON			"ALPMOn"
#define SIGNAL_ALPM_OFF			"ALPMOff"
#define CLOCK_START			"clockstart"
#define CLOCK_END			"clockend"
#define TIMEOUT_NONE			(-1)
#define ALPM_CLOCK_WAITING_TIME	5000 /* ms */

static char *alpm_path;
static int update_count;
static int alpm_state;
static pid_t alpm_pid;
static int ambient_mode;
static const struct device_ops *touchscreen_ops;

int get_ambient_mode(void)
{
	return ambient_mode;
}

void broadcast_alpm_state(int state)
{
	char *signal;

	signal = (state == true ? SIGNAL_ALPM_ON : SIGNAL_ALPM_OFF);

	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    signal, NULL, NULL);
}

bool check_suspend_direct(pid_t pid)
{
	struct state *st;

	if (pm_cur_state == S_NORMAL ||
	    pm_cur_state == S_LCDDIM)
		return false;

	if (pid != alpm_pid)
		return false;

	if (check_lock_state(S_SLEEP) == true)
		return false;

	if (backlight_ops.get_lcd_power() != PM_LCD_POWER_OFF)
		return false;

	_I("goto sleep state direct!");

	reset_timeout(TIMEOUT_NONE);
	pm_old_state = pm_cur_state;
	pm_cur_state = S_SLEEP;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(TIMEOUT_NONE);

	return true;
}

int alpm_get_state(void)
{
	char state[4];
	int ret, alpm;

	if (!alpm_path)
		return -ENODEV;

	ret = sys_get_str(alpm_path, state);
	if (ret < 0)
		return ret;

	if (!strncmp(state, ON, strlen(ON)))
		alpm = true;
	else if (!strncmp(state, OFF, strlen(OFF)))
		alpm = false;
	else
		alpm = -EINVAL;

	return alpm;
}

int alpm_set_state(int on)
{
	int mode;

	if (!ambient_mode)
		return 0;

	if (!alpm_path)
		return -ENODEV;

	broadcast_alpm_state(on);

	update_count = 0;

	if (!on)
		alpm_pid = 0;

	_D("ALPM is %s", (on ? ON : OFF));

	alpm_state = on;

	if (touchscreen_ops) {
		mode = (on ? AMBIENT_MODE : NORMAL_MODE);
		touchscreen_ops->execute((void *)mode);
	}

	return sys_set_str(alpm_path, (on ? ON : OFF));
}

void check_alpm_invalid_state(void)
{
	if (backlight_ops.get_lcd_power() == PM_LCD_POWER_OFF)
		return;

	if (alpm_get_state() == false)
		return;

	_E("Invalid state! alpm state is change to off!");

	/* If lcd_power is on and alpm state is true
	 * when pm state is changed to sleep
	 * deviced doesn't get the clock signal.
	 * deviced just turns off lcd in this case. */

	alpm_set_state(false);
	backlight_ops.off(NORMAL_MODE);
}

static void start_clock(void)
{
	if (pm_cur_state == S_NORMAL ||
	    pm_cur_state == S_LCDDIM ||
	    alpm_state == false)
		return;

	pm_lock_internal(INTERNAL_LOCK_ALPM,
	    LCD_OFF, STAY_CUR_STATE, ALPM_CLOCK_WAITING_TIME);
}

static void end_clock(pid_t pid)
{
	if (pm_cur_state == S_NORMAL ||
	    pm_cur_state == S_LCDDIM ||
	    alpm_state == false)
		return;

	if (update_count == 0) {
		_D("lcd off");
		backlight_ops.off(NORMAL_MODE);
		broadcast_lcd_off_late(LCD_OFF_LATE_MODE);
		pm_unlock_internal(INTERNAL_LOCK_ALPM,
		    LCD_OFF, PM_SLEEP_MARGIN);
	}

	update_count++;
	alpm_pid = pid;

	_I("enter real alpm state by %d (%d)",
	    alpm_pid, update_count);
}

int set_alpm_screen(char *screen, pid_t pid)
{
	if (!screen)
		return -EINVAL;

	if (!alpm_path)
		return -ENODEV;

	if (!strncmp(screen, CLOCK_START,
	    strlen(CLOCK_START))) {
		start_clock();
	} else if (!strncmp(screen, CLOCK_END,
	    strlen(CLOCK_END))) {
		end_clock(pid);
	}

	return 0;
}

static void set_ambient_mode(keynode_t *key_nodes, void *data)
{
	if (key_nodes == NULL) {
		_E("wrong parameter, key_nodes is null");
		return;
	}

	ambient_mode = vconf_keynode_get_bool(key_nodes);
	_I("ambient mode is %d", ambient_mode);

	if (!ambient_mode)
		pm_unlock_internal(INTERNAL_LOCK_ALPM, LCD_OFF,
		    PM_SLEEP_MARGIN);

	set_dim_state(ambient_mode ? false : true);
}

static void alpm_init(void *data)
{
	int fd;
	int ret;

	ret = vconf_get_bool(VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL,
	    &ambient_mode);
	if (ret < 0) {
		_E("Failed to get ambient mode! (%d)", ret);
		ambient_mode = false;
	}
	_I("ambient mode is %d", ambient_mode);

	vconf_notify_key_changed(VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL,
	    set_ambient_mode, NULL);

	alpm_path = find_device_node(DEVICE_ALPM);

	/* Check alpm node is valid */
	fd = open(alpm_path, O_RDONLY);
	if (fd < 0) {
		_E("alpm node is invalid");
		alpm_path = NULL;
		return;
	} else {
		_I("alpm path[%s] is initialized!", alpm_path);
	}
	close(fd);

	touchscreen_ops = find_device("touchscreen");
	if (check_default(touchscreen_ops))
		touchscreen_ops = NULL;
}

static void alpm_exit(void *data)
{
	alpm_set_state(false);
}

static const struct display_ops display_alpm_ops = {
	.name     = "alpm",
	.init     = alpm_init,
	.exit     = alpm_exit,
};

DISPLAY_OPS_REGISTER(&display_alpm_ops)

