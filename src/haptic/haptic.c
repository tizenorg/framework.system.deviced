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
#include <stdbool.h>
#include <dlfcn.h>
#include <vconf.h>

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"
#include "haptic.h"

#ifndef DATADIR
#define DATADIR		"/usr/share/deviced"
#endif

/* hardkey vibration variable */
#define HARDKEY_VIB_RESOURCE		DATADIR"/HW_touch_30ms_sharp.ivt"
#define HARDKEY_VIB_ITERATION		1
#define HARDKEY_VIB_FEEDBACK		3
#define HARDKEY_VIB_PRIORITY		2
#define HAPTIC_FEEDBACK_STEP		20

/* power on, power off vibration variable */
#define POWER_ON_VIB_DURATION			300
#define POWER_OFF_VIB_DURATION			300
#define POWER_VIB_FEEDBACK			100

#define MAX_EFFECT_BUFFER			(64*1024)

#define RETRY_CNT					3

#define CHECK_VALID_OPS(ops, r)		((ops) ? true : !(r = -ENODEV))

/* for playing */
static int g_handle;

/* haptic operation variable */
static dd_list *h_head;
static const struct haptic_plugin_ops *h_ops;
static bool haptic_disabled;

static int haptic_start(void);
static int haptic_stop(void);
static int haptic_internal_init(void);

void add_haptic(const struct haptic_ops *ops)
{
	DD_LIST_APPEND(h_head, (void*)ops);
}

void remove_haptic(const struct haptic_ops *ops)
{
	DD_LIST_REMOVE(h_head, (void*)ops);
}

static int haptic_module_load(void)
{
	struct haptic_ops *ops;
	dd_list *elem;
	int r;

	/* find valid plugin */
	DD_LIST_FOREACH(h_head, elem, ops) {
		if (ops->is_valid && ops->is_valid()) {
			if (ops->load)
				h_ops = ops->load();
			break;
		}
	}

	if (!CHECK_VALID_OPS(h_ops, r)) {
		_E("Can't find the valid haptic device");
		return r;
	}

	/* solution bug
	   we do not use internal vibration except power off
	   but module does not stop vibrating, although called terminate function */
	haptic_internal_init();

	return 0;
}

static DBusMessage *edbus_get_count(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret, val;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	ret = h_ops->get_device_count(&val);
	if (ret >= 0)
		ret = val;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_open_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int index, handle, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &index, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->open_device(index, &handle);
	if (ret >= 0)
		ret = handle;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_close_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	int ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->close_device(handle);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_vibrate_monotone(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	int duration, level, priority, e_handle, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled) {
		ret = -EACCES;
		goto exit;
	}

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_INT32, &duration,
				DBUS_TYPE_INT32, &level,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->vibrate_monotone(handle, duration, level, priority, &e_handle);
	if (ret >= 0)
		ret = e_handle;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;

}

static DBusMessage *edbus_vibrate_buffer(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	unsigned char *data;
	int size, iteration, level, priority, e_handle, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled) {
		ret = -EACCES;
		goto exit;
	}

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_INT32, &iteration,
				DBUS_TYPE_INT32, &level,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->vibrate_buffer(handle, data, iteration, level, priority, &e_handle);
	if (ret >= 0)
		ret = e_handle;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_stop_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	int ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled) {
		ret = -EACCES;
		goto exit;
	}

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->stop_device(handle);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int index, state, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &index, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->get_device_state(index, &state);
	if (ret >= 0)
		ret = state;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_create_effect(E_DBus_Object *obj, DBusMessage *msg)
{
	static unsigned char data[MAX_EFFECT_BUFFER];
	static unsigned char *p = data;
	DBusMessageIter iter, arr;
	DBusMessage *reply;
	haptic_module_effect_element *elem_arr;
	int i, size, cnt, ret, bufsize = sizeof(data);

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &bufsize,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &elem_arr, &size,
				DBUS_TYPE_INT32, &cnt, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	if (bufsize > MAX_EFFECT_BUFFER) {
		ret = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < cnt; ++i)
		_D("[%2d] %d %d", i, elem_arr[i].haptic_duration, elem_arr[i].haptic_level);

	memset(data, 0, MAX_EFFECT_BUFFER);
	ret = h_ops->create_effect(data, bufsize, elem_arr, cnt);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arr);
	dbus_message_iter_append_fixed_array(&arr, DBUS_TYPE_BYTE, &p, bufsize);
	dbus_message_iter_close_container(&iter, &arr);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_duration(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	unsigned char *data;
	int size, duration, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->get_buffer_duration(handle, data, &duration);
	if (ret >= 0)
		ret = duration;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_save_binary(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned char *data;
	unsigned char *path;
	int size, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	_D("file path : %s", path);
	ret = h_ops->convert_binary(data, size, path);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static unsigned char* convert_file_to_buffer(const char *file_name, int *size)
{
	FILE *pf;
	long file_size;
	unsigned char *pdata = NULL;

	if (!file_name)
		return NULL;

	/* Get File Stream Pointer */
	pf = fopen(file_name, "rb");
	if (!pf) {
		_E("fopen failed : %s", strerror(errno));
		return NULL;
	}

	if (fseek(pf, 0, SEEK_END))
		goto error;

	file_size = ftell(pf);
	if (fseek(pf, 0, SEEK_SET))
		goto error;

	if (file_size < 0)
		goto error;

	pdata = (unsigned char*)malloc(file_size);
	if (!pdata)
		goto error;

	if (fread(pdata, 1, file_size, pf) != file_size)
		goto err_free;

	fclose(pf);
	*size = file_size;
	return pdata;

err_free:
	free(pdata);

error:
	fclose(pf);

	_E("failed to convert file to buffer (%s)", strerror(errno));
	return NULL;
}

static int haptic_internal_init(void)
{
	int r;
	if (!CHECK_VALID_OPS(h_ops, r))
		return r;
	return h_ops->open_device(HAPTIC_MODULE_DEVICE_ALL, &g_handle);
}

static int haptic_internal_exit(void)
{
	int r;
	if (!CHECK_VALID_OPS(h_ops, r))
		return r;
	return h_ops->close_device(g_handle);
}

static int haptic_booting_done_cb(void *data)
{
	return haptic_module_load();
}

static int haptic_hardkey_changed_cb(void *data)
{
	int size, level, status, e_handle, ret, cnt = RETRY_CNT;
	unsigned char *buf;

	while (!CHECK_VALID_OPS(h_ops, ret) && cnt--) {
		haptic_module_load();
		if (!cnt)
			return ret;
	}

	if (!g_handle)
		haptic_internal_init();

	if (vconf_get_bool(VCONFKEY_SETAPPL_HAPTIC_FEEDBACK_STATUS_BOOL, &status) < 0) {
		_E("fail to get VCONFKEY_SETAPPL_HAPTIC_FEEDBACK_STATUS_BOOL");
		status = 1;
	}

	/* when turn off haptic feedback option */
	if (!status)
		return 0;

	buf = convert_file_to_buffer(HARDKEY_VIB_RESOURCE, &size);
	if (!buf)
		return -1;

	ret = vconf_get_int(VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT, &level);
	if (ret < 0) {
		_E("fail to get VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT");
		level = HARDKEY_VIB_FEEDBACK;
	}

	ret = h_ops->vibrate_buffer(g_handle, buf, HARDKEY_VIB_ITERATION,
			level*HAPTIC_FEEDBACK_STEP, HARDKEY_VIB_PRIORITY, &e_handle);
	if (ret < 0)
		_E("fail to vibrate buffer : %d", ret);

	free(buf);
	return ret;
}

static int haptic_poweroff_cb(void *data)
{
	int e_handle, ret, cnt = RETRY_CNT;

	while (!CHECK_VALID_OPS(h_ops, ret) && cnt--) {
		haptic_module_load();
		if (!cnt)
			return ret;
	}

	if (!g_handle)
		haptic_internal_init();

	/* power off vibration */
	ret = h_ops->vibrate_monotone(g_handle, POWER_OFF_VIB_DURATION,
			POWER_VIB_FEEDBACK, HARDKEY_VIB_PRIORITY, &e_handle);
	if (ret < 0) {
		_E("fail to vibrate_monotone : %d", ret);
		return ret;
	}

	/* sleep for vibration */
	usleep(POWER_OFF_VIB_DURATION*1000);
	return 0;
}

static int haptic_powersaver_cb(void *data)
{
	int powersaver_on = (int)data;

	if (powersaver_on)
		haptic_stop();
	else
		haptic_start();

	return 0;
}

static const struct edbus_method edbus_methods[] = {
	{ "GetCount",          NULL,   "i", edbus_get_count },
	{ "OpenDevice",         "i",   "i", edbus_open_device },
	{ "CloseDevice",        "u",   "i", edbus_close_device },
	{ "StopDevice",         "u",   "i", edbus_stop_device },
	{ "VibrateMonotone", "uiii",   "i", edbus_vibrate_monotone },
	{ "VibrateBuffer", "uayiii",   "i", edbus_vibrate_buffer },
	{ "GetState",           "i",   "i", edbus_get_state },
	{ "GetDuration",      "uay",   "i", edbus_get_duration },
	{ "CreateEffect",    "iayi", "ayi", edbus_create_effect },
	{ "SaveBinary",       "ays",   "i", edbus_save_binary },
	/* Add methods here */
};

static void haptic_init(void *data)
{
	int r;

	/* init dbus interface */
	r = register_edbus_method(DEVICED_PATH_HAPTIC, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (r < 0)
		_E("fail to init edbus method(%d)", r);

	/* register notifier for below each event */
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, haptic_booting_done_cb);
	register_notifier(DEVICE_NOTIFIER_TOUCH_HARDKEY, haptic_hardkey_changed_cb);
	register_notifier(DEVICE_NOTIFIER_POWEROFF_HAPTIC, haptic_poweroff_cb);
	register_notifier(DEVICE_NOTIFIER_POWERSAVER, haptic_powersaver_cb);
}

static void haptic_exit(void *data)
{
	struct haptic_ops *ops;
	dd_list *elem;
	int r;

	/* unregister notifier for below each event */
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, haptic_booting_done_cb);
	unregister_notifier(DEVICE_NOTIFIER_TOUCH_HARDKEY, haptic_hardkey_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF_HAPTIC, haptic_poweroff_cb);
	unregister_notifier(DEVICE_NOTIFIER_POWERSAVER, haptic_powersaver_cb);

	if (!CHECK_VALID_OPS(h_ops, r))
		return;

	/* haptic exit for deviced */
	haptic_internal_exit();

	/* release plugin */
	DD_LIST_FOREACH(h_head, elem, ops) {
		if (ops->is_valid && ops->is_valid()) {
			if (ops->release)
				ops->release();
			h_ops = NULL;
			break;
		}
	}
}

static int haptic_start(void)
{
	_I("start");
	haptic_disabled = false;
	return 0;
}

static int haptic_stop(void)
{
	_I("stop");
	haptic_disabled = true;
	return 0;
}

static const struct device_ops haptic_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "haptic",
	.init     = haptic_init,
	.exit     = haptic_exit,
};

DEVICE_OPS_REGISTER(&haptic_device_ops)
