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


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include <vconf.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "poll.h"
#include "weaks.h"
#include "brightness.h"
#include "device-node.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "power/power-handler.h"

#include <linux/input.h>

#define POWEROFF_ACT			"poweroff"

#define KEY_RELEASED		0
#define KEY_PRESSED		1

#define SIGNAL_LCDON_BY_POWERKEY "LCDOnByPowerkey"

static Ecore_Timer *longkey_timeout_id;
static bool ignore_powerkey;

static inline int current_state_in_on(void)
{
	return (pm_cur_state == S_LCDDIM || pm_cur_state == S_NORMAL);
}

static int power_execute(void *data)
{
	static const struct device_ops *ops;

	FIND_DEVICE_INT(ops, POWER_OPS_NAME);

	return ops->execute(data);
}

static void longkey_pressed()
{
	int val, ret;

	_I("Power key long pressed!");

	/*
	 * Poweroff-popup has been launched by app.
	 * deviced only turns off the phone by power key
	 * when power-off popup is disabled for testmode.
	 */
	_I("power off action!");

	power_execute(POWEROFF_ACT);
}

static Eina_Bool longkey_pressed_cb(void *data)
{
	longkey_pressed();
	longkey_timeout_id = NULL;

	return EINA_FALSE;
}

static inline void check_key_pair(int code, int new, int *old)
{
	if (new == *old)
		_E("key pair is not matched! (%d, %d)", code, new);
	else
		*old = new;
}

static inline void broadcast_lcdon_by_powerkey(void)
{
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_LCDON_BY_POWERKEY, NULL, NULL);
}

static inline bool switch_on_lcd(enum device_flags flags)
{
	if (current_state_in_on())
		return false;

	if (backlight_ops.get_lcd_power() == PM_LCD_POWER_ON)
		if (alpm_get_state == NULL ||
		    alpm_get_state() == false)
			return false;

	broadcast_lcdon_by_powerkey();

	lcd_on_direct(flags);

	return true;
}

static inline void switch_off_lcd(void)
{
	if (!current_state_in_on())
		return;

	if (backlight_ops.get_lcd_power() == PM_LCD_POWER_OFF)
		return;

	lcd_off_procedure(LCD_OFF_BY_POWER_KEY);
}

static int process_power_key(struct input_event *pinput)
{
	int ignore = true;
	static int value = KEY_RELEASED;

	switch (pinput->value) {
	case KEY_RELEASED:
		check_key_pair(pinput->code, pinput->value, &value);
		ignore = false;

		if (longkey_timeout_id > 0) {
			ecore_timer_del(longkey_timeout_id);
			longkey_timeout_id = NULL;
		}
		break;
	case KEY_PRESSED:
		switch_on_lcd(LCD_ON_BY_POWER_KEY);
		check_key_pair(pinput->code, pinput->value, &value);
		_I("power key pressed");

		/* add long key timer */
		longkey_timeout_id = ecore_timer_add(
			    display_conf.longpress_interval,
			    longkey_pressed_cb, NULL);
		ignore = false;
		break;
	}
	return ignore;
}

static int check_key(struct input_event *pinput, int fd)
{
	int ignore = true;

	switch (pinput->code) {
	case KEY_POWER:
		ignore = process_power_key(pinput);
		break;
	case KEY_PHONE:	/* Flick up key */
	case KEY_BACK: /* Flick down key */
	case KEY_VOLUMEUP:
	case KEY_VOLUMEDOWN:
	case KEY_CAMERA:
	case KEY_EXIT:
	case KEY_CONFIG:
	case KEY_MEDIA:
	case KEY_MUTE:
	case KEY_PLAYPAUSE:
	case KEY_PLAYCD:
	case KEY_PAUSECD:
	case KEY_STOPCD:
	case KEY_NEXTSONG:
	case KEY_PREVIOUSSONG:
	case KEY_REWIND:
	case KEY_FASTFORWARD:
		/* These case do not turn on the lcd */
		if (current_state_in_on())
			ignore = false;
		break;
	default:
		ignore = false;
	}

	if (pinput->value == KEY_PRESSED)
		pm_history_save(PM_LOG_KEY_PRESS, pinput->code);
	else if (pinput->value == KEY_RELEASED)
		pm_history_save(PM_LOG_KEY_RELEASE, pinput->code);
	return ignore;
}

static int check_key_filter(void *data, int fd)
{
	struct input_event *pinput = (struct input_event *)data;
	int ignore = true;
	static int old_fd, code, value;

	switch (pinput->type) {
	case EV_KEY:
		if (pinput->code == code && pinput->value == value) {
			_E("Same key(%d, %d) is polled [%d,%d]",
			    code, value, old_fd, fd);
		}
		old_fd = fd;
		code = pinput->code;
		value = pinput->value;

		ignore = check_key(pinput, fd);
		break;
	case EV_REL:
		ignore = false;
		break;
	case EV_ABS:
		if (current_state_in_on())
			ignore = false;
		if (get_ambient_mode != NULL &&
		    get_ambient_mode() == true) {
			switch_on_lcd(LCD_ON_BY_TOUCH);
			ignore = false;
		}
		if (pm_cur_state == S_LCDDIM &&
		    backlight_ops.get_custom_status())
			backlight_ops.custom_update();
		break;
	}

	if (ignore == true)
		return 1;

	return 0;
}

static void set_powerkey_ignore(int on)
{
	_D("ignore powerkey %d", on);
	ignore_powerkey = on;
}

static int powerkey_lcdoff(void)
{
	/* Remove non est processes */
	check_processes(S_LCDOFF);
	check_processes(S_LCDDIM);

	/* check holdkey block flag in lock node */
	if (check_holdkey_block(S_LCDOFF) ||
	    check_holdkey_block(S_LCDDIM)) {
		return -ECANCELED;
	}

	_I("power key lcdoff");

	if (display_info.update_auto_brightness)
		display_info.update_auto_brightness(false);
	switch_off_lcd();
	delete_condition(S_LCDOFF);
	delete_condition(S_LCDDIM);
	recv_data.pid = getpid();
	recv_data.cond = 0x400;
	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

static const struct display_keyfilter_ops micro_keyfilter_ops = {
	.init			= NULL,
	.exit			= NULL,
	.check			= check_key_filter,
	.set_powerkey_ignore	= set_powerkey_ignore,
	.powerkey_lcdoff	= powerkey_lcdoff,
	.backlight_enable	= NULL,
};
const struct display_keyfilter_ops *keyfilter_ops = &micro_keyfilter_ops;

