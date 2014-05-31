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

#define ON		"on"
#define OFF		"off"

#define SIGNAL_ALPM_ON			"ALPMOn"
#define SIGNAL_ALPM_OFF			"ALPMOff"
#define CLOCK_START			"clockstart"
#define CLOCK_END			"clockend"
#define ALPM_MAX_TIME			30	/* minutes */

static char *alpm_path;
static int update_count;
static int alpm_state;

void broadcast_alpm_state(int state)
{
	char *signal;

	signal = (state == true ? SIGNAL_ALPM_ON : SIGNAL_ALPM_OFF);

	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    signal, NULL, NULL);
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
	if (!alpm_path)
		return -ENODEV;

	broadcast_alpm_state(on);

	update_count = 0;

	_D("ALPM is %s", (on ? ON : OFF));

	alpm_state = on;

	return sys_set_str(alpm_path, (on ? ON : OFF));
}

static void start_clock(void)
{
	struct timespec now_time;
	static struct timespec start_time;
	int diff_time;

	if (pm_cur_state == S_NORMAL ||
	    pm_cur_state == S_LCDDIM ||
	    alpm_state == false)
		return;

	_D("lcd on");

	if (update_count == 0)
		clock_gettime(CLOCK_REALTIME, &start_time);

	update_count++;

	_D("clock update count : %d", update_count);

	clock_gettime(CLOCK_REALTIME, &now_time);
	diff_time = (now_time.tv_sec - start_time.tv_sec) / 60;

	if (diff_time >= ALPM_MAX_TIME) {
		_D("Exit ALPM state, LCD off");
		alpm_set_state(false);
		backlight_ops.on();
		backlight_ops.off();
		return;
	}

	/* change pmstate in order that apps can know the state */
	set_setting_pmstate(S_NORMAL);
	backlight_ops.on();

	sleep(1);

	_D("lcd off");
	set_setting_pmstate(S_LCDOFF);
	backlight_ops.off();

	_D("finished!");
}

static void end_clock(void)
{
	if (pm_cur_state == S_NORMAL ||
	    pm_cur_state == S_LCDDIM ||
	    alpm_state == false)
		return;

	_D("end clock : lcd off");
}

int set_alpm_screen(char *screen)
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
		end_clock();
	}

	return 0;
}

static void alpm_init(void *data)
{
	int fd;

	alpm_path = getenv("ALPM_NODE");

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

