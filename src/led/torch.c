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
#include <vconf.h>
#include <device-node.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "apps/apps.h"
#include "dd-display.h"
#include "torch.h"

#define CRITICAL_TURNOFF_LED_ACT		"critical_turnoff_led_act"

#define LED_TURNOFF_INTERVAL	5

#define SIGNAL_FLASH_STATE "ChangeFlashState"

static int camera_status;
static Ecore_Timer *turnoff_led_timer;

static int torch_lowbat_popup(char *option)
{
	char *value;

	if (!option)
		return -1;

	if (strcmp(option, CRITICAL_TURNOFF_LED_ACT))
		return -1;

	value = "turnoffled";

	return launch_system_app(APP_DEFAULT,
			2, APP_KEY_TYPE, value);
}

static void flash_state_broadcast(int val)
{
	char *arr[1];
	char str_state[32];

	snprintf(str_state, sizeof(str_state), "%d", val);
	arr[0] = str_state;

	broadcast_edbus_signal(DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			SIGNAL_FLASH_STATE, "i", arr);
}

static Eina_Bool torch_turn_off_led_cb(void *data EINA_UNUSED)
{
	int status;

	if (turnoff_led_timer) {
		ecore_timer_del(turnoff_led_timer);
		turnoff_led_timer = NULL;
	}

	if (vconf_get_bool(VCONFKEY_SETAPPL_ACCESSIBILITY_TORCH_LIGHT, &status) != 0)
		return ECORE_CALLBACK_RENEW;

	if (!status)
		return ECORE_CALLBACK_RENEW;

	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &status) != 0)
		return ECORE_CALLBACK_RENEW;

	if (status == VCONFKEY_SYSMAN_CHARGER_CONNECTED)
		return ECORE_CALLBACK_RENEW;

	status = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, 0);
	if (status < 0)
		_E("turn_off_led_cb failed to set the property");
	else {
		vconf_set_bool(VCONFKEY_SETAPPL_ACCESSIBILITY_TORCH_LIGHT, 0);
		/* flash status broadcast */
		flash_state_broadcast(0);
		ongoing_clear();
	}

	return ECORE_CALLBACK_RENEW;
}

static void torch_check_battery_status(bool launch_popup)
{
	int status;

	/* Turn OFF LED if it is ON and battery level is critically low  */
	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &status) != 0)
		return;

	if (status == VCONFKEY_SYSMAN_CHARGER_CONNECTED)
		return;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &status) != 0)
		return;

	if (status == VCONFKEY_SYSMAN_BAT_CRITICAL_LOW ||
	    status == VCONFKEY_SYSMAN_BAT_POWER_OFF ||
	    status == VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF) {
		if (launch_popup)
			torch_lowbat_popup(CRITICAL_TURNOFF_LED_ACT);
		turnoff_led_timer = ecore_timer_add(LED_TURNOFF_INTERVAL, torch_turn_off_led_cb, NULL);
		if (!turnoff_led_timer)
			_E("ecore_timer_add failed");
	}
}

static void torch_battery_status_cb(keynode_t *key, void *data)
{
	/* Turn OFF LED if it is ON and battery level is critically low  */
	torch_check_battery_status(false);
}

static void torch_led_status_cb(keynode_t *key, void *data)
{
	int enabled;

	/* If LED is ON and charger is connected, register for charger & battery status change */
	if (vconf_get_bool(VCONFKEY_SETAPPL_ACCESSIBILITY_TORCH_LIGHT, &enabled) != 0)
		return;

	if (enabled) {
		vconf_notify_key_changed(VCONFKEY_SYSMAN_CHARGER_STATUS, torch_battery_status_cb, NULL);
		vconf_notify_key_changed(VCONFKEY_SYSMAN_BATTERY_LEVEL_STATUS, torch_battery_status_cb, NULL);

		/* Check if battery is in critical level */
		torch_check_battery_status(true);
		return;
	}

	vconf_ignore_key_changed(VCONFKEY_SYSMAN_CHARGER_STATUS, torch_battery_status_cb);
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_BATTERY_LEVEL_STATUS, torch_battery_status_cb);
}

static DBusMessage *edbus_get_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, &val);
	if (ret >= 0)
		ret = val;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_max_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_LED, PROP_LED_MAX_BRIGHTNESS, &val);
	if (ret >= 0)
		ret = val;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_set_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, enable, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &val,
			DBUS_TYPE_INT32, &enable, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, val);
	if (ret < 0)
		goto error;

	/* flash status broadcast */
	flash_state_broadcast(val);

	/* if enable is ON, noti will be show or hide */
	if (enable) {
		if (val)
			ongoing_show();
		else
			ongoing_clear();
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_camera_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_LED, PROP_LED_FLASH_BRIGHTNESS, &val);
	if (ret >= 0)
		ret = val;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_set_camera_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, enable, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = device_set_property(DEVICE_TYPE_LED, PROP_LED_FLASH_BRIGHTNESS, val);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_play_pattern(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *path;
	int cnt, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INT32, &cnt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto exit;
	}

	/* check camera status
	   Do not play led notification during the video recording */
	if (camera_status == VCONFKEY_CAMERA_STATE_RECORDING) {
		_D("Camera is recording (status : %d)", camera_status);
		ret = 0;
		goto exit;
	}

	/* TODO:
	   Now we passed led file path from other module.
	   But led file is not a common file structure,
	   so we should change to be passed vibration pattern path
	   then it converts to led data internally */
	ret = play_pattern(path, cnt);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_stop_pattern(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = stop_pattern();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static void camera_status_cb(keynode_t *key, void *data)
{
	camera_status = vconf_keynode_get_int(key);

	_D("recording status changed!! new status => %d", camera_status);

	if (camera_status != VCONFKEY_CAMERA_STATE_RECORDING)
		return;

	/* If camera state is recording, stop to play ledplayer */
	stop_pattern();
}

static const struct edbus_method edbus_methods[] = {
	/**
	 * GetBrightnessForCamera is for camera library.
	 * Currently they do not have a daemon for camera,
	 * but they need to get camera brightness value without led priv.
	 * It's a temporary solution on Tizen 2.4 and will be removed asap.
	 */
	{ "GetBrightnessForCamera", NULL,   "i", edbus_get_brightness },
	{ "GetBrightness",    NULL,   "i", edbus_get_brightness },
	{ "GetMaxBrightness", NULL,   "i", edbus_get_max_brightness },
	{ "SetBrightness",    "ii",   "i", edbus_set_brightness },
	{ "GetCameraBrightness",    NULL,   "i", edbus_get_camera_brightness },
	{ "SetCameraBrightness",     "i",   "i", edbus_set_camera_brightness },
	{ "PlayPattern",      "si",   "i", edbus_play_pattern },
	{ "StopPattern",      NULL,   "i", edbus_stop_pattern },
	/* Add methods here */
};

static void torch_init(void *data)
{
	int ret;

	/* init dbus interface */
	ret = register_edbus_interface_and_method(DEVICED_PATH_LED,
			DEVICED_INTERFACE_LED,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	/* Get camera status */
	if (vconf_get_int(VCONFKEY_CAMERA_STATE, &camera_status) < 0)
		_E("Failed to get VCONFKEY_CAMERA_STATE value");

	/* add watch for status value */
	vconf_notify_key_changed(VCONFKEY_CAMERA_STATE, camera_status_cb, NULL);

	/* add watch for assistive light */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TORCH_LIGHT, torch_led_status_cb, NULL);
}

static void torch_exit(void *data)
{
	/* Release vconf callback */
	vconf_ignore_key_changed(VCONFKEY_CAMERA_STATE, camera_status_cb);
}

static const struct device_ops torchled_device_ops = {
	.name     = "torchled",
	.init     = torch_init,
	.exit     = torch_exit,
};

DEVICE_OPS_REGISTER(&torchled_device_ops)
