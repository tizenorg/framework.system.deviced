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
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"

#define ON		"on"
#define OFF		"off"

#define SIGNAL_HBM_OFF	"HBMOff"

#define DEFAULT_BRIGHTNESS_LEVEL	80

static Ecore_Timer *timer = NULL;
static struct timespec offtime;
static char *hbm_path;

static void broadcast_hbm_off(void)
{
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_HBM_OFF, NULL, NULL);
}

static void hbm_set_offtime(int timeout)
{
	struct timespec now;

	if (timeout <= 0) {
		offtime.tv_sec = 0;
		return;
	}

	clock_gettime(CLOCK_REALTIME, &now);
	offtime.tv_sec = now.tv_sec + timeout;
}

static Eina_Bool hbm_off_cb(void *data)
{
	int ret;

	timer = NULL;

	if (pm_cur_state != S_NORMAL) {
		_D("hbm timeout! but it's not lcd normal");
		return EINA_FALSE;
	}
	hbm_set_offtime(0);

	ret = sys_set_str(hbm_path, OFF);
	if (ret < 0)
		_E("Failed to off hbm");

	vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS,
	    DEFAULT_BRIGHTNESS_LEVEL);
	backlight_ops.set_default_brt(DEFAULT_BRIGHTNESS_LEVEL);
	backlight_ops.update();
	broadcast_hbm_off();

	return EINA_FALSE;
}

static void hbm_start_timer(int timeout)
{
	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}
	if (!timer) {
		timer = ecore_timer_add(timeout,
		    hbm_off_cb, NULL);
	}
}

static void hbm_end_timer(void)
{
	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}
}

int hbm_get_state(void)
{
	char state[4];
	int ret, hbm;

	if (!hbm_path)
		return -ENODEV;

	ret = sys_get_str(hbm_path, state);
	if (ret < 0)
		return ret;

	if (!strncmp(state, ON, strlen(ON)))
		hbm = true;
	else if (!strncmp(state, OFF, strlen(OFF)))
		hbm = false;
	else
		hbm = -EINVAL;

	return hbm;
}

int hbm_set_state(int hbm)
{
	if (!hbm_path)
		return -ENODEV;

	return sys_set_str(hbm_path, (hbm ? ON : OFF));
}

int hbm_set_state_with_timeout(int hbm, int timeout)
{
	int ret;

	if (hbm && (timeout <= 0))
		return -EINVAL;

	ret = hbm_set_state(hbm);
	if (ret < 0)
		return ret;

	_D("timeout is %d", timeout);

	if (hbm) {
		/*
		 * hbm is turned off after timeout.
		 */
		hbm_set_offtime(timeout);
		hbm_start_timer(timeout);
	} else {
		hbm_set_offtime(0);
		hbm_end_timer();
		broadcast_hbm_off();
	}

	return 0;
}

void hbm_check_timeout(void)
{
	struct timespec now;
	int ret;

	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}

	if (offtime.tv_sec == 0) {
		if (hbm_get_state() == true) {
			_E("It's invalid state. hbm is off");
			hbm_set_state(false);
		}
		return;
	}

	clock_gettime(CLOCK_REALTIME, &now);
	_D("now %d, offtime %d", now.tv_sec, offtime.tv_sec);

	/* check it's timeout */
	if (now.tv_sec >= offtime.tv_sec) {
		hbm_set_offtime(0);

		ret = sys_set_str(hbm_path, OFF);
		if (ret < 0)
			_E("Failed to off hbm");
		vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS,
		    DEFAULT_BRIGHTNESS_LEVEL);
		backlight_ops.set_default_brt(DEFAULT_BRIGHTNESS_LEVEL);
		backlight_ops.update();
		broadcast_hbm_off();
	} else {
		_D("hbm state is restored!");
		hbm_set_state(true);
		hbm_start_timer(offtime.tv_sec - now.tv_sec);
	}
}

static int lcd_state_changed(void *data)
{
	int state = (int)data;
	int ret;

	switch (state) {
	case S_NORMAL:
		/* restore hbm when old state is dim */
		if (pm_old_state == S_LCDDIM)
			hbm_check_timeout();
		break;
	case S_LCDDIM:
		if (hbm_get_state() == true) {
			ret = hbm_set_state(false);
			if (ret < 0)
				_E("Failed to off hbm!");
		}
		/* fall through */
	case S_LCDOFF:
	case S_SLEEP:
		hbm_end_timer();
		break;
	}

	return 0;
}

static void hbm_init(void *data)
{
	int fd, ret;

	hbm_path = getenv("HBM_NODE");

	/* Check HBM node is valid */
	fd = open(hbm_path, O_RDONLY);
	if (fd < 0) {
		hbm_path = NULL;
		return;
	}
	close(fd);

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_LCD, lcd_state_changed);
}

static void hbm_exit(void *data)
{
	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_LCD, lcd_state_changed);
}

static const struct display_ops display_hbm_ops = {
	.name     = "hbm",
	.init     = hbm_init,
	.exit     = hbm_exit,
};

DISPLAY_OPS_REGISTER(&display_hbm_ops)

