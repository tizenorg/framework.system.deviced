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


/**
 * @file	display-dbus.c
 * @brief	dbus interface
 *
 */

#include <error.h>
#include <stdbool.h>
#include <Ecore.h>
#include <device-node.h>
#include <sensor.h>

#include "core/log.h"
#include "util.h"
#include "core.h"
#include "weaks.h"
#include "core/common.h"
#include "core/devices.h"
#include "dd-display.h"
#include "display-actor.h"

#define TELEPHONY_PATH			"/org/tizen/telephony"
#define TELEPHONY_INTERFACE_SIM		"org.tizen.telephony.Manager"
#define SIGNAL_SIM_STATUS		"SimInserted"
#define SIM_CARD_NOT_PRESENT		(0x0)

#define VCONFKEY_LCD_BRIGHTNESS_INIT "db/private/deviced/lcd_brightness_init"

#define SIGNAL_HOMESCREEN		"HomeScreen"
#define SIGNAL_EXTREME			"Extreme"
#define SIGNAL_NOTEXTREME		"NotExtreme"

#define BLIND_MASK(val)			((val) & 0xFFFF)
#define BLIND_RED(val)			BLIND_MASK((val) >> 32)
#define BLIND_GREEN(val)		BLIND_MASK((val) >> 16)
#define BLIND_BLUE(val)			BLIND_MASK((val))

#define DISPLAY_DIM_BRIGHTNESS  0
#define DUMP_MODE_WATING_TIME   600000

static DBusMessage *edbus_start(E_DBus_Object *obj, DBusMessage *msg)
{
	static const struct device_ops *display_device_ops;

	if (!display_device_ops)
		display_device_ops = find_device("display");
	if (NOT_SUPPORT_OPS(display_device_ops))
		return dbus_message_new_method_return(msg);

	display_device_ops->start(CORE_LOGIC_MODE);
	return dbus_message_new_method_return(msg);
}

static DBusMessage *edbus_stop(E_DBus_Object *obj, DBusMessage *msg)
{
	static const struct device_ops *display_device_ops;

	if (!display_device_ops)
		display_device_ops = find_device("display");
	if (NOT_SUPPORT_OPS(display_device_ops))
		return dbus_message_new_method_return(msg);

	display_device_ops->stop(CORE_LOGIC_MODE);
	return dbus_message_new_method_return(msg);
}

static DBusMessage *edbus_lockstate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state_str;
	char *option1_str;
	char *option2_str;
	int timeout;
	pid_t pid;
	int state;
	int flag;
	int ret;
	unsigned int caps;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &state_str,
		    DBUS_TYPE_STRING, &option1_str,
		    DBUS_TYPE_STRING, &option2_str,
		    DBUS_TYPE_INT32, &timeout, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (!state_str || timeout < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else {
		_E("%s state is invalid, dbus ignored!", state_str);
		ret = -EINVAL;
		goto out;
	}

	if (!strcmp(option1_str, STAYCURSTATE_STR))
		flag = STAY_CUR_STATE;
	else if (!strcmp(option1_str, GOTOSTATENOW_STR))
		flag = GOTO_STATE_NOW;
	else {
		_E("%s option is invalid. set default option!", option1_str);
		flag = STAY_CUR_STATE;
	}

	if (!strcmp(option2_str, HOLDKEYBLOCK_STR))
		flag |= HOLD_KEY_BLOCK;
	else if (!strcmp(option2_str, STANDBYMODE_STR))
		flag |= STANDBY_MODE;

	if (flag & GOTO_STATE_NOW) {
		caps = display_get_caps(DISPLAY_ACTOR_API);

		if (!display_has_caps(caps, DISPLAY_CAPA_LCDON) &&
		    state == LCD_NORMAL) {
			_D("No lcdon capability!");
			ret = -EPERM;
			goto out;
		}
		if (!display_has_caps(caps, DISPLAY_CAPA_LCDOFF) &&
		    state == LCD_OFF) {
			_D("No lcdoff capability!");
			ret = -EPERM;
			goto out;
		}
	}

	if (check_dimstay(state, flag) == true) {
		_E("LCD state can not be changed to OFF state now!");
		flag &= ~GOTO_STATE_NOW;
		flag |= STAY_CUR_STATE;
	}

	ret = pm_lock_internal(pid, state, flag, timeout);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_unlockstate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state_str;
	char *option_str;
	pid_t pid;
	int state;
	int flag;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &state_str,
		    DBUS_TYPE_STRING, &option_str, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (!state_str) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else {
		_E("%s state is invalid, dbus ignored!", state_str);
		ret = -EINVAL;
		goto out;
	}

	if (!strcmp(option_str, SLEEP_MARGIN_STR))
		flag = PM_SLEEP_MARGIN;
	else if (!strcmp(option_str, RESET_TIMER_STR))
		flag = PM_RESET_TIMER;
	else if (!strcmp(option_str, KEEP_TIMER_STR))
		flag = PM_KEEP_TIMER;
	else {
		_E("%s option is invalid. set default option!", option_str);
		flag = PM_RESET_TIMER;
	}

	ret = pm_unlock_internal(pid, state, flag);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_changestate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state_str;
	pid_t pid;
	int state;
	int ret;
	unsigned int caps;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &state_str, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (!state_str) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else if (!strcmp(state_str, PM_SUSPEND_STR))
		state = SUSPEND;
	else {
		_E("%s state is invalid, dbus ignored!", state_str);
		ret = -EINVAL;
		goto out;
	}

	caps = display_get_caps(DISPLAY_ACTOR_API);

	if (!display_has_caps(caps, DISPLAY_CAPA_LCDON) &&
	    state == LCD_NORMAL) {
		_D("No lcdon capability!");
		ret = -EPERM;
		goto out;
	}
	if (!display_has_caps(caps, DISPLAY_CAPA_LCDOFF) &&
	    state == LCD_OFF) {
		_D("No lcdoff capability!");
		ret = -EPERM;
		goto out;
	}

	if (check_dimstay(state, GOTO_STATE_NOW) == true) {
		_E("LCD state can not be changed to OFF state!");
		ret = -EBUSY;
		goto out;
	}

	ret = pm_change_internal(pid, state);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_getdisplaycount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, cnt, ret;

	cmd = DISP_CMD(PROP_DISPLAY_DISPLAY_COUNT, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &cnt);
	if (ret >= 0)
		ret = cnt;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getmaxbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, ret;

	cmd = DISP_CMD(PROP_DISPLAY_MAX_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brt);
	if (ret >= 0)
		ret = brt;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setmaxbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brt);

	cmd = DISP_CMD(PROP_DISPLAY_MAX_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brt);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, ret;

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brt);
	if (ret >= 0)
		ret = brt;

	_I("get brightness %d, %d", brt, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &brt);
	return reply;
}

static DBusMessage *edbus_setbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, powersaver, autobrt, ret, caps;

	caps = display_get_caps(DISPLAY_ACTOR_API);

	if (!display_has_caps(caps, DISPLAY_CAPA_BRIGHTNESS)) {
		_D("No brightness changing capability!");
		ret = -EPERM;
		goto error;
	}

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brt);

	if (brt == DISPLAY_DIM_BRIGHTNESS) {
		_E("application can not set this value(DIM VALUE:%d)", brt);
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &powersaver) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_PSMODE value");
		powersaver = SETTING_PSMODE_NORMAL;
	}

	if (powersaver == SETTING_PSMODE_WEARABLE ||
	    powersaver == SETTING_PSMODE_EMERGENCY) {
		_D("Powersaver mode! brightness can not be changed!");
		ret = -EPERM;
		goto error;

	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &autobrt) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		autobrt = SETTING_BRIGHTNESS_AUTOMATIC_OFF;
	}

	if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		_E("auto_brightness state is ON, can not change the brightness value");
		ret = -EPERM;
		goto error;
	}

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brt);
	if (ret < 0)
		goto error;
	if (vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt) != 0)
		_E("Failed to set VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");

	backlight_ops.set_current_brightness(brt);

	_I("set brightness %d, %d", brt, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_holdbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, powersaver, autobrt, ret, caps;

	caps = display_get_caps(DISPLAY_ACTOR_API);

	if (!display_has_caps(caps, DISPLAY_CAPA_BRIGHTNESS)) {
		_D("No brightness changing capability!");
		ret = -EPERM;
		goto error;
	}

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brt);

	if (brt == DISPLAY_DIM_BRIGHTNESS) {
		_E("application can not set this value(DIM VALUE:%d)", brt);
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &powersaver) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_PSMODE value");
		powersaver = SETTING_PSMODE_NORMAL;
	}

	if (powersaver == SETTING_PSMODE_WEARABLE ||
	    powersaver == SETTING_PSMODE_EMERGENCY) {
		_D("Powersaver mode! brightness can not be changed!");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &autobrt) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		autobrt = SETTING_BRIGHTNESS_AUTOMATIC_OFF;
	}

	backlight_ops.set_custom_status(true);

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brt);
	if (ret < 0)
		goto error;

	if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		_D("Auto brightness will be paused");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_PAUSE);
	}

	backlight_ops.set_current_brightness(brt);

	_I("hold brightness %d, %d", brt, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;

}

static DBusMessage *edbus_releasebrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, bat, charger, changed, setting, brt, autobrt, powersaver, ret = 0;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat) != 0) {
		_E("Failed to get VCONFKEY_SYSMAN_BATTERY_STATUS_LOW value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &charger) != 0) {
		_E("Failed to get VCONFKEY_SYSMAN_CHARGER_STATUS value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &setting) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &autobrt) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		ret = -EPERM;
		goto error;
	}

	changed = pm_status_flag & BRTCH_FLAG;
	backlight_ops.set_custom_status(false);

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brt);
	if (ret < 0)
		brt = ret;

	/* check dim state */
	if (low_battery_state(bat) &&
	    charger == VCONFKEY_SYSMAN_CHARGER_DISCONNECTED && !changed) {
		_D("batt warning low : brightness is not changed!");
		if (brt != 0)
			device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, 0);
		goto error;
	}

	if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_OFF) {
		if (vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &powersaver) != 0) {
			_E("Failed to get VCONFKEY_SETAPPL_PSMODE value");
			powersaver = SETTING_PSMODE_NORMAL;
		}

		if (powersaver == SETTING_PSMODE_WEARABLE ||
		    powersaver == SETTING_PSMODE_EMERGENCY) {
			_D("brightness is released in powersaver mode!");
			ret = -EPERM;
			goto error;
		}

		if (brt != setting) {
			device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, setting);
			backlight_ops.set_current_brightness(setting);
		}
	} else if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		_D("Auto brightness will be enable");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_ON);
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getaclstatus(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, st, ret;

	cmd = DISP_CMD(PROP_DISPLAY_ACL_CONTROL, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &st);
	if (ret >= 0)
		ret = st;

	_I("get acl status %d, %d", st, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setaclstatus(E_DBus_Object *ob, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, st, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &st);

	cmd = DISP_CMD(PROP_DISPLAY_ACL_CONTROL, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, st);

	_I("set acl status %d, %d", st, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getautotone(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, tone, ret;

	cmd = DISP_CMD(PROP_DISPLAY_AUTO_SCREEN_TONE, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &tone);
	if (ret >= 0)
		ret = tone;

	_I("get auto screen tone %d, %d", tone, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setautotone(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, tone, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &tone);

	cmd = DISP_CMD(PROP_DISPLAY_AUTO_SCREEN_TONE, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, tone);

	_I("set auto screen tone %d, %d", tone, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setautotoneforce(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, tone, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &tone);

	cmd = DISP_CMD(PROP_DISPLAY_AUTO_SCREEN_TONE_FORCE, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, tone);

	_I("set auto screen tone force %d, %d", tone, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setrefreshrate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int app, val, cmd, ret, control;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &app,
			DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (app < 0 || app >= ARRAY_SIZE(display_conf.framerate_app) || val < 0) {
		ret = -EINVAL;
		goto error;
	}

	if (!display_conf.framerate_app[app]) {
		_I("This case(%d) is not support in this target", app);
		ret = -EPERM;
		goto error;
	}

	control = display_conf.control_display;

	if (control)
		backlight_ops.off(NORMAL_MODE);

	_D("app : %d, value : %d", app, val);
	cmd = DISP_CMD(PROP_DISPLAY_FRAME_RATE, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, val);

	if (control)
		backlight_ops.on(NORMAL_MODE);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getcolorblind(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, val, ret;

	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_COLOR_BLIND, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &val);
	if (ret >= 0)
		ret = val;

	_I("get color blind %d", val);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setcolorblind(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, val, ret;
	unsigned int power;
	uint64_t red, grn, blu;
	struct color_blind_info info;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &power,
			DBUS_TYPE_UINT64, &red, DBUS_TYPE_UINT64, &grn,
			DBUS_TYPE_UINT64, &blu, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	info.power = power;
	info.RrCr = BLIND_RED(red);
	info.RgCg = BLIND_GREEN(red);
	info.RbCb = BLIND_BLUE(red);
	info.GrMr = BLIND_RED(grn);
	info.GgMg = BLIND_GREEN(grn);
	info.GbMb = BLIND_BLUE(grn);
	info.BrYr = BLIND_RED(blu);
	info.BgYg = BLIND_GREEN(blu);
	info.BbYb = BLIND_BLUE(blu);

	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_COLOR_BLIND, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, (int)&info);

	_I("set color blind %d, %hu, %hu, %hu, %hu, %hu, %hu, %hu, %hu, %hu, %d",
			info.power, info.RrCr, info.RgCg, info.RbCb, info.GrMr, info.GgMg,
			info.GbMb, info.BrYr, info.BgYg, info.BbYb, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_gethbm(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int hbm;

	if (!msg) {
		_E("msg is empty!");
		return NULL;
	}

	/* check weak function symbol */
	if (!hbm_get_state) {
		hbm = -ENOSYS;
		goto error;
	}

	hbm = hbm_get_state();

	if (hbm < 0)
		_E("failed to get high brightness mode %d", hbm);
	else
		_D("get high brightness mode %d", hbm);
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &hbm);
	return reply;
}

static DBusMessage *edbus_sethbm(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int hbm, ret;

	if (!msg) {
		_E("msg is empty!");
		return NULL;
	}

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_INT32, &hbm,
		    DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	/* check weak function symbol */
	if (!hbm_set_state) {
		ret = -ENOSYS;
		goto error;
	}

	ret = hbm_set_state(hbm);

	if (ret < 0)
		_E("failed to set high brightness mode (%d,%d)", ret, hbm);
	else
		_I("set high brightness mode (%d,%d)", ret, hbm);
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_sethbm_timeout(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int hbm, timeout, ret;

	if (!msg) {
		_E("msg is empty!");
		return NULL;
	}

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_INT32, &hbm,
		    DBUS_TYPE_INT32, &timeout, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	/* check weak function symbol */
	if (!hbm_set_state_with_timeout) {
		ret = -ENOSYS;
		goto error;
	}

	ret = hbm_set_state_with_timeout(hbm, timeout);

	if (ret < 0)
		_E("failed to set high brightness mode (%d,%d,%d)",
		    ret, hbm, timeout);
	else
		_I("set high brightness mode (%d,%d,%d)",
		    ret, hbm, timeout);
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setautobrightnessmin(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;
	pid_t pid;
	const char *sender;

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender name!");
		ret = -EINVAL;
		goto error;
	}
	if (!display_info.set_autobrightness_min) {
		ret = -EIO;
		goto error;
	}
	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &val);

	pid = get_edbus_sender_pid(msg);
	ret = display_info.set_autobrightness_min(val, (char *)sender);
	if (ret) {
		_W("fail to set autobrightness min %d, %d by %d", val, ret, pid);
		goto error;
	}
	if (display_info.reset_autobrightness_min) {
		register_edbus_watch(sender,
		    display_info.reset_autobrightness_min, NULL);
		_I("set autobrightness min %d by %d", val, pid);
	}
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_setlcdtimeout(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int on, dim, holdkey_block, ret;
	pid_t pid;
	const char *sender;

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender name!");
		ret = -EINVAL;
		goto error;
	}

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &on,
		    DBUS_TYPE_INT32, &dim, DBUS_TYPE_INT32, &holdkey_block,
		    DBUS_TYPE_INVALID);

	pid = get_edbus_sender_pid(msg);
	ret = set_lcd_timeout(on, dim, holdkey_block, sender);
	if (ret) {
		_W("fail to set lcd timeout %d by %d", ret, pid);
	} else {
		register_edbus_watch(sender,
		    reset_lcd_timeout, NULL);
		_I("set lcd timeout on %d, dim %d, holdblock %d by %d",
		    on, dim, holdkey_block, pid);
	}
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_lockscreenbgon(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	char *on;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &on,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to update lcdscreen bg on state %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (!strcmp(on, "true"))
		update_pm_setting(SETTING_LOCK_SCREEN_BG, true);
	else if (!strcmp(on, "false"))
		update_pm_setting(SETTING_LOCK_SCREEN_BG, false);
	else
		ret = -EINVAL;

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_dumpmode(E_DBus_Object *obj, DBusMessage *msg)
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
		pm_lock_internal(INTERNAL_LOCK_DUMPMODE, LCD_OFF,
		    STAY_CUR_STATE, DUMP_MODE_WATING_TIME);
	} else if (!strcmp(on, "off")) {
		pm_unlock_internal(INTERNAL_LOCK_DUMPMODE, LCD_OFF,
		    PM_SLEEP_MARGIN);
		backlight_ops.blink(0);
	} else if (!strcmp(on, "blink")) {
		backlight_ops.blink(500);
	} else {
		ret = -EINVAL;
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_savelog(E_DBus_Object *obj, DBusMessage *msg)
{
	save_display_log(NULL);
	return dbus_message_new_method_return(msg);
}

static DBusMessage *edbus_powerkeyignore(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	int on;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &on,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get powerkey ignore %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (CHECK_OPS(keyfilter_ops, set_powerkey_ignore))
		keyfilter_ops->set_powerkey_ignore(on == 1 ? true : false);
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_powerkeylcdoff(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	if (CHECK_OPS(keyfilter_ops, powerkey_lcdoff))
		ret = keyfilter_ops->powerkey_lcdoff();
	else
		ret = -ENOSYS;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_customlcdon(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	int timeout;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &timeout,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get custom lcd timeout %d", ret);
		ret = -EINVAL;
		goto error;
	}

	ret = custom_lcdon(timeout);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_staytouchscreenoff(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	int val;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &val,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get stay touchscreen off state %d", ret);
		ret = -EINVAL;
		goto error;
	}

	set_stay_touchscreen_off(val);
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_lcdpaneloffmode(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	int val;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &val,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get lcd panel off mode %d", ret);
		ret = -EINVAL;
		goto error;
	}

	set_lcd_paneloff_mode(val);
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_actorcontrol(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0, val, actor;
	char *op;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &op,
		    DBUS_TYPE_INT32, &actor, DBUS_TYPE_INT32, &val,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to update actor control %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (!strcmp(op, "set"))
		ret = display_set_caps(actor, val);
	else if (!strcmp(op, "reset"))
		ret = display_reset_caps(actor, val);
	else
		ret = -EINVAL;

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_autobrightnesschanged(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	int level;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &level,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get level %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (display_info.set_brightness_level)
		display_info.set_brightness_level(level);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static void set_lcd_timeout_by_touch(const char *sender, void *data)
{
	if (!sender)
		return;

	_I("set capability!");

	display_set_caps(DISPLAY_ACTOR_TOUCH_KEY,
			DISPLAY_CAPA_UPDATE_LCD_TIMEOUT);

}

static DBusMessage *edbus_updatelcdtimeoutbytouch(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	const char *sender;
	int ret = 0;
	int state;

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender name");
		ret = -EINVAL;
		goto error;
	}

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &state,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to get state %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (state == true) {
		ret = display_set_caps(DISPLAY_ACTOR_TOUCH_KEY,
		    DISPLAY_CAPA_UPDATE_LCD_TIMEOUT);
	} else if (state == false) {
		ret = display_reset_caps(DISPLAY_ACTOR_TOUCH_KEY,
		    DISPLAY_CAPA_UPDATE_LCD_TIMEOUT);
		register_edbus_watch(sender, set_lcd_timeout_by_touch, NULL);
	} else {
		ret = -EINVAL;
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *edbus_getcustombrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int status = 0;

	status = backlight_ops.get_custom_status();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &status);

	return reply;
}

static DBusMessage *edbus_getcurrentbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int brightness = 0;

	brightness = backlight_ops.get_current_brightness();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &brightness);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "start",           NULL,  NULL, edbus_start },
	{ "stop",            NULL,  NULL, edbus_stop },
	{ "lockstate",     "sssi",   "i", edbus_lockstate },
	{ "unlockstate",     "ss",   "i", edbus_unlockstate },
	{ "changestate",      "s",   "i", edbus_changestate },
	{ "getbrightness",   NULL,   "i", edbus_getbrightness },	/* deprecated */
	{ "setbrightness",    "i",   "i", edbus_setbrightness },	/* deprecated */
	{ "getautotone",     NULL,   "i", edbus_getautotone },	/* deprecated */
	{ "setautotone",      "i",   "i", edbus_setautotone },	/* deprecated */
	{ "setframerate",    "ii",   "i", edbus_setrefreshrate },	/* deprecated */
	{ "getcolorblind",   NULL,   "i", edbus_getcolorblind },	/* deprecated */
	{ "setcolorblind", "uttt",   "i", edbus_setcolorblind },	/* deprecated */
	{ "setautobrightnessmin", "i", "i", edbus_setautobrightnessmin },
	{ "setlcdtimeout",  "iii",   "i", edbus_setlcdtimeout },
	{ "LockScreenBgOn",   "s",   "i", edbus_lockscreenbgon },
	{ "GetDisplayCount", NULL,   "i", edbus_getdisplaycount },
	{ "GetMaxBrightness", NULL,   "i", edbus_getmaxbrightness },
	{ "SetMaxBrightness", "i",   "i", edbus_setmaxbrightness },
	{ "GetBrightness",   NULL,   "i", edbus_getbrightness },
	{ "SetBrightness",    "i",   "i", edbus_setbrightness },
	{ "HoldBrightness",   "i",   "i", edbus_holdbrightness },
	{ "ReleaseBrightness", NULL, "i", edbus_releasebrightness },
	{ "GetAclStatus",    NULL,   "i", edbus_getaclstatus },
	{ "SetAclStatus",    NULL,   "i", edbus_setaclstatus },
	{ "GetAutoTone",     NULL,   "i", edbus_getautotone },
	{ "SetAutoTone",      "i",   "i", edbus_setautotone },
	{ "SetAutoToneForce", "i",   "i", edbus_setautotoneforce },
	{ "SetRefreshRate",  "ii",   "i", edbus_setrefreshrate },
	{ "GetColorBlind",   NULL,   "i", edbus_getcolorblind },
	{ "SetColorBlind", "uttt",   "i", edbus_setcolorblind },
	{ "GetHBM",          NULL,   "i", edbus_gethbm },
	{ "SetHBM",           "i",   "i", edbus_sethbm },
	{ "SetHBMTimeout",   "ii",   "i", edbus_sethbm_timeout },
	{ "Dumpmode",         "s",   "i", edbus_dumpmode },
	{ "SaveLog",         NULL,  NULL, edbus_savelog },
	{ "PowerKeyIgnore",   "i",  NULL, edbus_powerkeyignore },
	{ "PowerKeyLCDOff",  NULL,   "i", edbus_powerkeylcdoff },
	{ "CustomLCDOn",      "i",   "i", edbus_customlcdon },
	{ "StayTouchScreenOff", "i",  "i", edbus_staytouchscreenoff },
	{ "LCDPanelOffMode",  "i",   "i", edbus_lcdpaneloffmode },
	{ "ActorControl",   "sii",   "i", edbus_actorcontrol },
	{ "AutoBrightnessChanged",   "i",   "i", edbus_autobrightnesschanged },
	{ "UpdateLCDTimeoutByTouch", "i",   "i", edbus_updatelcdtimeoutbytouch },
	{ "CustomBrightness", NULL,   "i", edbus_getcustombrightness },
	{ "CurrentBrightness", NULL,  "i", edbus_getcurrentbrightness },
	/* Add methods here */
};

static void sim_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret, val;
	static int state;

	if (state)
		return;

	ret = dbus_message_is_signal(msg, TELEPHONY_INTERFACE_SIM,
	    SIGNAL_SIM_STATUS);
	if (!ret) {
		_E("there is no sim status signal");
		return;
	}

	ret = vconf_get_bool(VCONFKEY_LCD_BRIGHTNESS_INIT, &state);
	if (ret < 0 || state)
		return;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val,
	    DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
	}

	if (val != SIM_CARD_NOT_PRESENT) {
		state = true;
		vconf_set_bool(VCONFKEY_LCD_BRIGHTNESS_INIT, state);
		vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS,
		    display_conf.default_brightness);
		backlight_ops.set_default_brt(
		    display_conf.default_brightness);
		backlight_ops.update();
		_I("SIM card is inserted at first!");
	}
}

static void homescreen_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret;
	char *screen;
	pid_t pid;

	ret = dbus_message_is_signal(msg, DEVICED_INTERFACE_NAME,
	    SIGNAL_HOMESCREEN);
	if (!ret) {
		_E("there is no homescreen signal");
		return;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &screen,
	    DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
	}

	_D("screen : %s", screen);

	if (set_alpm_screen) {
		pid = get_edbus_sender_pid(msg);
		set_alpm_screen(screen, pid);
	} else {
		_E("alpm screen mode is not supported!");
	}
}

/*
 * Default capability
 * api      := LCDON | LCDOFF | BRIGHTNESS
 * gesture  := LCDON
 */
static struct display_actor_ops display_api_actor = {
	.id	= DISPLAY_ACTOR_API,
	.caps	= DISPLAY_CAPA_LCDON |
		  DISPLAY_CAPA_LCDOFF |
		  DISPLAY_CAPA_BRIGHTNESS,
};

static struct display_actor_ops display_gesture_actor = {
	.id	= DISPLAY_ACTOR_GESTURE,
	.caps	= DISPLAY_CAPA_LCDON,
};

int init_pm_dbus(void)
{
	int ret;

	display_add_actor(&display_api_actor);
	display_add_actor(&display_gesture_actor);

	ret = register_edbus_method(DEVICED_PATH_DISPLAY,
		    edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0) {
		_E("Failed to register edbus method! %d", ret);
		return ret;
	}

	ret = register_edbus_signal_handler(TELEPHONY_PATH,
		    TELEPHONY_INTERFACE_SIM, SIGNAL_SIM_STATUS,
		    sim_signal_handler);
	if (ret < 0 && ret != -EEXIST) {
		_E("Failed to register signal handler! %d", ret);
		return ret;
	}

	ret = register_edbus_signal_handler(DEVICED_OBJECT_PATH,
		    DEVICED_INTERFACE_NAME, SIGNAL_HOMESCREEN,
		    homescreen_signal_handler);
	if (ret < 0 && ret != -EEXIST) {
		_E("Failed to register signal handler! %d", ret);
		return ret;
	}

	return 0;
}

