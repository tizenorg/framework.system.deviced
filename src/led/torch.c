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
#include "torch.h"

static int camera_status;

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

	/* TODO: Now we passed led file path from other module.
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
	ret = register_edbus_method(DEVICED_PATH_LED, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	/* Get camera status */
	if (vconf_get_int(VCONFKEY_CAMERA_STATE, &camera_status) < 0)
		_E("Failed to get VCONFKEY_CAMERA_STATE value");

	/* add watch for status value */
	vconf_notify_key_changed(VCONFKEY_CAMERA_STATE, camera_status_cb, NULL);
}

static void torch_exit(void *data)
{
	/* Release vconf callback */
	vconf_ignore_key_changed(VCONFKEY_CAMERA_STATE, camera_status_cb);
}

static const struct device_ops torchled_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "torchled",
	.init     = torch_init,
	.exit     = torch_exit,
};

DEVICE_OPS_REGISTER(&torchled_device_ops)
