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
#include <errno.h>
#include <assert.h>
#include <device-node.h>
#include <Ecore.h>
#include <vconf.h>

#include "deviced/dd-led.h"
#include "core/log.h"
#include "core/common.h"
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/device-handler.h"
#include "display/core.h"
#include "conf.h"

#define BOOT_ANIMATION_FINISHED		1

#define LED_VALUE(high, low)		(((high)<<16)|((low)&0xFFFF))

#define BG_COLOR		0x00000000
#define GET_ALPHA(x)	(((x)>>24) & 0xFF)
#define GET_RED(x)		(((x)>>16) & 0xFF)
#define GET_GREEN(x)	(((x)>> 8) & 0xFF)
#define GET_BLUE(x)		((x) & 0xFF)
#define COMBINE_RGB(r,g,b)	((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

#define SET_WAVE_BIT	(0x6 << 24)
#define DUMPMODE_WAITING_TIME    600000

enum {
	LED_CUSTOM_DUTY_ON = 1 << 0,	/* this enum doesn't blink on led */
	LED_CUSTOM_DEFAULT = (LED_CUSTOM_DUTY_ON),
};

static int charging_key;
static int lowbat_key;
static bool charging_state;
static bool lowbat_state;
static bool full_state;
static bool badbat_state;
static bool ovp_state;
static int rgb_blocked = false;
static bool rgb_dumpmode = false;
static Ecore_Timer *dumpmode_timer = NULL;
static enum state_t lcd_state = S_NORMAL;
static Ecore_Timer *reset_timer;

static int rgb_start(enum device_flags flags)
{
	_I("rgbled device will be started");
	rgb_blocked = false;
	return 0;
}

static int rgb_stop(enum device_flags flags)
{
	_I("rgbled device will be stopped");
	rgb_blocked = true;
	return 0;
}

static int led_prop(int mode, int on, int off, unsigned int color)
{
	struct led_mode *led;

	if (mode < 0 || mode > LED_MODE_MAX)
		return -EINVAL;

	led = find_led_data(mode);
	if (!led) {
		_E("no find data(%d)", mode);
		return -EINVAL;
	}

	switch (mode) {
	case LED_MISSED_NOTI:
	case LED_VOICE_RECORDING:
	case LED_CUSTOM:
		if (on >= 0)
			led->data.on = on;
		if (off >= 0)
			led->data.off = off;
		if (color)
			led->data.color = color;
		break;
	default:
		/* the others couldn't change any property */
		break;
	}

	_D("changed mode(%d) : state(%d), on(%d), off(%d), color(%x)",
			mode, led->state, led->data.on, led->data.off, led->data.color);
	return 0;
}

static int led_mode(int mode, bool enable)
{
	struct led_mode *led;

	if (mode < 0 || mode > LED_MODE_MAX)
		return -EINVAL;

	led = find_led_data(mode);
	if (!led) {
		_E("no find data(%d)", mode);
		return -EINVAL;
	}

	led->state = enable;
	return 0;
}

static int get_led_mode_state(int mode, bool *state)
{
	struct led_mode *led;

	if (mode < 0 || mode > LED_MODE_MAX)
		return -EINVAL;

	led = find_led_data(mode);
	if (!led) {
		_E("no find data(%d)", mode);
		return -EINVAL;
	}

	if (led->state)
		*state = true;
	else
		*state = false;
	return 0;
}

static unsigned int led_blend(unsigned int before)
{
	unsigned int alpha, alpha_inv;
	unsigned char red, grn, blu;

	alpha = GET_ALPHA(before) + 1;
	alpha_inv = 256 - alpha;

	red = ((alpha * GET_RED(before) + alpha_inv * GET_RED(BG_COLOR)) >> 8);
	grn = ((alpha * GET_GREEN(before) + alpha_inv * GET_GREEN(BG_COLOR)) >> 8);
	blu = ((alpha * GET_BLUE(before) + alpha_inv * GET_BLUE(BG_COLOR)) >> 8);
	return COMBINE_RGB(red, grn, blu);
}

static Eina_Bool timer_reset_cb(void *data);
static int led_mode_to_device(struct led_mode *led)
{
	int val, color;
	double time;

	if (led == NULL)
		return 0;

	if (rgb_dumpmode)
		return 0;

	if (rgb_blocked && led->mode != LED_POWER_OFF && led->mode != LED_OFF)
		return 0;

	if (led->data.duration > 0) {
		time = ((led->data.on + led->data.off) * led->data.duration) / 1000.f;
		if (reset_timer)
			ecore_timer_del(reset_timer);
		reset_timer = ecore_timer_add(time, timer_reset_cb, led);
		if (!reset_timer)
			_E("failed to add reset timer");
		_D("add reset timer (mode:%d, %lfs)", led->mode, time);
	}

	val = LED_VALUE(led->data.on, led->data.off);
	color = led_blend(led->data.color);

	/* if wave status is ON, color should be combined with WAVE_BIT */
	if (led->data.wave)
		color |= SET_WAVE_BIT;

	device_set_property(DEVICE_TYPE_LED, PROP_LED_COLOR, color);
	device_set_property(DEVICE_TYPE_LED, PROP_LED_BLINK, val);
	return 0;
}

static Eina_Bool timer_reset_cb(void *data)
{
	struct led_mode *led = (struct led_mode*)data;

	if (!led)
		return ECORE_CALLBACK_CANCEL;

	led->state = false;
	_D("reset timer (mode:%d)", led->mode);

	/* turn off previous setting */
	led = find_led_data(LED_OFF);
	led_mode_to_device(led);

	led = get_valid_led_data(lcd_state);
	/* preventing duplicated requests */
	if (led && led->mode != LED_OFF)
		led_mode_to_device(led);

	return ECORE_CALLBACK_CANCEL;
}

static int led_display_changed_cb(void *data)
{
	struct led_mode *led = NULL;
	int state;

	/* store last display condition */
	lcd_state = (enum state_t)data;

	/* charging error state */
	if (badbat_state)
		return 0;

	if (lcd_state == S_LCDOFF)
		state = DURING_LCD_OFF;
	else if (lcd_state == S_NORMAL || lcd_state == S_LCDDIM)
		state = DURING_LCD_ON;
	else
		return 0;

	/* turn off previous setting */
	led = find_led_data(LED_OFF);
	led_mode_to_device(led);

	led = get_valid_led_data(state);
	/* preventing duplicated requests */
	if (led && led->mode != LED_OFF)
		led_mode_to_device(led);

	return 0;
}

static int led_charging_changed_cb(void *data)
{
	int v = (int)data;

	if (v == CHARGER_CHARGING)
		charging_state = true;
	else
		charging_state = false;

	if (charging_key)
		led_mode(LED_CHARGING, charging_state);

	return 0;
}

static int led_lowbat_changed_cb(void *data)
{
	lowbat_state = (bool)data;

	if (lowbat_key)
		led_mode(LED_LOW_BATTERY, lowbat_state);

	return 0;
}

static int led_fullbat_changed_cb(void *data)
{
	full_state = (bool)data;

	if (charging_key)
		led_mode(LED_FULLY_CHARGED, full_state);

	return 0;
}

static int led_battery_health_changed_cb(void *data)
{
	struct led_mode *led;
	bool cur;

	cur = ((int)data == HEALTH_BAD) ? true : false;

	/* do not enter below scenario in case of same state */
	if (cur == badbat_state)
		return 0;

	badbat_state = cur;

	/* set charging error mode */
	led_mode(LED_CHARGING_ERROR, badbat_state);

	/* update the led state */
	if (badbat_state) {	/* charging error */
		led = find_led_data(LED_CHARGING_ERROR);
		led_mode_to_device(led);
	} else {
		led = find_led_data(LED_OFF);
		led_mode_to_device(led);
		if (lcd_state == S_LCDOFF)
			led_display_changed_cb((void*)S_LCDOFF);
	}

	return 0;
}

static int led_battery_ovp_changed_cb(void *data)
{
	int cur = (int)data;

	/* do not enter below flow in case of same state */
	if (cur == ovp_state)
		return 0;

	ovp_state = cur;

	if (ovp_state == OVP_NORMAL && lcd_state == S_LCDOFF)
		led_display_changed_cb((void*)S_LCDOFF);

	return 0;
}

static void led_poweron_changed_cb(keynode_t *key, void *data)
{
	int state;
	struct led_mode *led;

	state = vconf_keynode_get_int(key);

	if (state != BOOT_ANIMATION_FINISHED)
		return;

	led = find_led_data(LED_OFF);
	led_mode_to_device(led);

	vconf_ignore_key_changed(VCONFKEY_BOOT_ANIMATION_FINISHED,
			led_poweron_changed_cb);
}

static void led_poweroff_changed_cb(keynode_t *key, void *data)
{
	int state;
	struct led_mode *led;

	state = vconf_keynode_get_int(key);

	if (state == VCONFKEY_SYSMAN_POWER_OFF_NONE ||
		state == VCONFKEY_SYSMAN_POWER_OFF_POPUP)
		return;

	led = find_led_data(LED_POWER_OFF);
	led_mode_to_device(led);

	vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS,
			led_poweroff_changed_cb);
}

static void led_vconf_charging_cb(keynode_t *key, void *data)
{
	charging_key = vconf_keynode_get_bool(key);

	if (charging_key) {
		led_mode(LED_CHARGING, charging_state);
		led_mode(LED_FULLY_CHARGED, full_state);
	} else {
		led_mode(LED_CHARGING, false);
		led_mode(LED_FULLY_CHARGED, false);
	}
}

static void led_vconf_lowbat_cb(keynode_t *key, void *data)
{
	lowbat_key = vconf_keynode_get_bool(key);

	if (lowbat_key)
		led_mode(LED_LOW_BATTERY, lowbat_state);
	else
		led_mode(LED_LOW_BATTERY, false);
}

static void led_vconf_blocking_cb(keynode_t *key, void *data)
{
	rgb_blocked = vconf_keynode_get_bool(key);
	_I("rgbled blocking mode %s", (rgb_blocked ? "started" : "stopped"));
}

static void led_vconf_psmode_cb(keynode_t *key, void *data)
{
	int psmode;

	psmode = vconf_keynode_get_int(key);
	if (psmode == SETTING_PSMODE_EMERGENCY)
		rgb_stop(NORMAL_MODE);
	else
		rgb_start(NORMAL_MODE);
}

static DBusMessage *edbus_playcustom(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, val, ret;
	int on, off;
	unsigned int color, flags;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &on,
			DBUS_TYPE_INT32, &off,
			DBUS_TYPE_UINT32, &color,
			DBUS_TYPE_UINT32, &flags, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	/* not to play blink, on and off value should be zero */
	if (!(flags & LED_CUSTOM_DUTY_ON))
		on = off = 0;

	/* set new value in case of LED_CUSTOM value */
	ret = led_mode(LED_CUSTOM, true);
	ret = led_prop(LED_CUSTOM, on, off, color);

	_D("play custom %d, %d, %x, %d", on, off, color, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_stopcustom(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;
	bool state;

	ret = get_led_mode_state(LED_CUSTOM, &state);
	if (ret < 0) {
		_E("failed to get led mode state : %d", ret);
		ret = -EPERM;
		goto error;
	}

	if (!state) {
		_E("not play custom led");
		ret = -EPERM;
		goto error;
	}

	/* reset default value */
	ret = led_mode(LED_CUSTOM, false);
	ret = led_prop(LED_CUSTOM, 500, 5000, 255);

	_D("stop custom %d", ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_set_mode(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int color;
	int mode, val, on, off, ret;
	struct led_mode *led;
	bool mode_enabled = false;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &mode,
			DBUS_TYPE_INT32, &val,
			DBUS_TYPE_INT32, &on,
			DBUS_TYPE_INT32, &off,
			DBUS_TYPE_UINT32, &color, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = get_led_mode_state(mode, &mode_enabled);
	if (ret) {
		_I("failed to get led mode state : %d", ret);
		goto error;
	}

	ret = led_mode(mode, val);
	ret = led_prop(mode, on, off, color);

	/* If lcd prop has a during_lcd_on bit,
	   it should turn on/off during lcd on state */
	led = find_led_data(mode);
	if (led && (led->data.lcd & DURING_LCD_ON) &&
	    (lcd_state == S_NORMAL || lcd_state == S_LCDDIM)) {
		led = get_valid_led_data(DURING_LCD_ON);
		led_mode_to_device(led);
	}

	/* Exception case :
	   in case of display off condition,
	   who requests missed noti event, it should change the device value */
	if (mode == LED_MISSED_NOTI && lcd_state == S_LCDOFF) {
		if (!mode_enabled || on == 0) {
			/* turn off previous setting */
			led = find_led_data(LED_OFF);
			led_mode_to_device(led);
			/* update the led state */
			led_display_changed_cb((void*)S_LCDOFF);
		}
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static void set_dumpmode(bool on)
{
	struct led_mode *led;
	int val, color;

	if (on) {
		val = LED_VALUE(200, 100);
		color = led_blend(0xFFFF0000);
		device_set_property(DEVICE_TYPE_LED, PROP_LED_COLOR, color);
		device_set_property(DEVICE_TYPE_LED, PROP_LED_BLINK, val);
		rgb_dumpmode = true;
		_I("dump_mode on");
	} else {
		if (dumpmode_timer) {
			ecore_timer_del(dumpmode_timer);
			dumpmode_timer = NULL;
		}
		if (rgb_dumpmode)
			rgb_dumpmode = false;
		else
			return;
		led = find_led_data(LED_OFF);
		led_mode_to_device(led);
		_I("dump_mode off");
	}
}

static void dumpmode_timer_cb(void *data)
{
	set_dumpmode(false);
}

static DBusMessage *edbus_dump_mode(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	char *on;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &on,
			DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get dumpmode state %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (!strcmp(on, "on")) {
		set_dumpmode(true);
		dumpmode_timer = ecore_timer_add(DUMPMODE_WAITING_TIME,
				(Ecore_Task_Cb)dumpmode_timer_cb, NULL);
	} else if (!strcmp(on, "off")) {
		set_dumpmode(false);
	} else
		ret = -EINVAL;

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_print_mode(E_DBus_Object *obj, DBusMessage *msg)
{
	print_all_data();
	return dbus_message_new_method_return(msg);
}

static const struct edbus_method edbus_methods[] = {
	{ "playcustom",     "iiuu",  "i",  edbus_playcustom },
	{ "stopcustom",     NULL,    "i",  edbus_stopcustom },
	{ "SetMode",        "iiiiu", "i",  edbus_set_mode },
	{ "PrintMode",      NULL,    NULL, edbus_print_mode },
	{ "Dumpmode",       "s",     "i",  edbus_dump_mode },
	/* Add methods here */
};

static void rgb_init(void *data)
{
	int ta_connected, boot, psmode;
	int ret;
	struct led_mode *led;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_LED, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	/* get led mode data from xml */
	get_led_data();

	/* LED_OFF state must be always true */
	led_mode(LED_OFF, true);

	/* verify booting or restart */
	ret = vconf_get_int(VCONFKEY_BOOT_ANIMATION_FINISHED, &boot);
	if (ret != 0)
		boot = 0;	/* set booting state */

	/* in case of restart */
	if (boot)
		goto next;

	/* when booting, led indicator will be work */
	led = find_led_data(LED_POWER_OFF);
	led_mode_to_device(led);
	vconf_notify_key_changed(VCONFKEY_BOOT_ANIMATION_FINISHED,
			led_poweron_changed_cb, NULL);

next:
	/* register power off callback */
	vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS,
			led_poweroff_changed_cb, NULL);

	/* register notifier for each event */
	register_notifier(DEVICE_NOTIFIER_LCD, led_display_changed_cb);
	register_notifier(DEVICE_NOTIFIER_BATTERY_CHARGING, led_charging_changed_cb);
	register_notifier(DEVICE_NOTIFIER_LOWBAT, led_lowbat_changed_cb);
	register_notifier(DEVICE_NOTIFIER_FULLBAT, led_fullbat_changed_cb);
	register_notifier(DEVICE_NOTIFIER_BATTERY_HEALTH, led_battery_health_changed_cb);
	register_notifier(DEVICE_NOTIFIER_BATTERY_OVP, led_battery_ovp_changed_cb);

	/* initialize vconf value */
	vconf_get_bool(VCONFKEY_SETAPPL_LED_INDICATOR_CHARGING, &charging_key);
	vconf_get_bool(VCONFKEY_SETAPPL_LED_INDICATOR_LOW_BATT, &lowbat_key);

	/* initialize led indicator blocking value */
	vconf_get_bool(VCONFKEY_SETAPPL_BLOCKINGMODE_LED_INDICATOR, &rgb_blocked);

	/* register vconf callback */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_LED_INDICATOR_CHARGING,
			led_vconf_charging_cb, NULL);
	vconf_notify_key_changed(VCONFKEY_SETAPPL_LED_INDICATOR_LOW_BATT,
			led_vconf_lowbat_cb, NULL);

	/* register led indicator blocking vconf callback */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_BLOCKINGMODE_LED_INDICATOR,
			led_vconf_blocking_cb, NULL);

	/* check power saving state */
	vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &psmode);
	if (psmode == SETTING_PSMODE_EMERGENCY)
		rgb_stop(NORMAL_MODE);

	/* register power saving callback */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE, led_vconf_psmode_cb, NULL);

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_NOW, &ta_connected);
	if (!ret && ta_connected)
		led_charging_changed_cb((void*)ta_connected);
}

static void rgb_exit(void *data)
{
	struct led_mode *led;

	/* unregister vconf callback */
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_LED_INDICATOR_CHARGING,
			led_vconf_charging_cb);
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_LED_INDICATOR_LOW_BATT,
			led_vconf_lowbat_cb);

	/* unregister led indicatore blocking vconf callback */
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_BLOCKINGMODE_LED_INDICATOR,
			led_vconf_blocking_cb);

	/* unregister power saving callback */
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE, led_vconf_psmode_cb);

	/* unregister notifier for each event */
	unregister_notifier(DEVICE_NOTIFIER_LCD, led_display_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_BATTERY_CHARGING, led_charging_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_LOWBAT, led_lowbat_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_FULLBAT, led_fullbat_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_BATTERY_HEALTH, led_battery_health_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_BATTERY_OVP, led_battery_ovp_changed_cb);

	set_dumpmode(false);
	/* turn off led */
	led = find_led_data(LED_OFF);
	led_mode_to_device(led);

	/* release led mode data */
	release_led_data();
}

static const struct device_ops rgbled_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "rgbled",
	.init     = rgb_init,
	.exit     = rgb_exit,
	.start    = rgb_start,
	.stop     = rgb_stop,
};

DEVICE_OPS_REGISTER(&rgbled_device_ops)
