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

#include "log.h"
#include "dbus.h"
#include "common.h"
#include "dd-display.h"

#define DISPLAY_MAX_BRIGHTNESS  100
#define DISPLAY_MIN_BRIGHTNESS  1
#define DISPLAY_DIM_BRIGHTNESS  0

#define HOLDKEY_BLOCK_BIT		0x1
#define STANDBY_MODE_BIT		0x2

#define BLIND_MASK(val)			((val) & 0xFFFF)
#define BLIND_COLOR(a,b,c)		((BLIND_MASK((unsigned long long)(a)) << 32) |	 \
								(BLIND_MASK(b) << 16) | BLIND_MASK(c))

#define METHOD_GET_ENHANCE_SUPPORTED	"GetEnhanceSupported"
#define METHOD_GET_IMAGE_ENHANCE	"GetImageEnhance"
#define METHOD_SET_IMAGE_ENHANCE	"SetImageEnhance"
#define METHOD_SET_REFRESH_RATE		"SetRefreshRate"
#define METHOD_GET_COLOR_BLIND		"GetColorBlind"
#define METHOD_SET_COLOR_BLIND		"SetColorBlind"
#define METHOD_LOCK_STATE		"lockstate"
#define METHOD_UNLOCK_STATE		"unlockstate"
#define METHOD_CHANGE_STATE		"changestate"
#define METHOD_GET_DISPLAY_COUNT	"GetDisplayCount"
#define METHOD_GET_MAX_BRIGHTNESS	"GetMaxBrightness"
#define METHOD_GET_BRIGHTNESS	"GetBrightness"
#define METHOD_SET_BRIGHTNESS	"SetBrightness"
#define METHOD_HOLD_BRIGHTNESS	"HoldBrightness"
#define METHOD_RELEASE_BRIGHTNESS	"ReleaseBrightness"
#define METHOD_GET_ACL_STATUS	"GetAclStatus"
#define METHOD_SET_ACL_STATUS	"SetAclStatus"
#define METHOD_GET_AUTO_TONE	"GetAutoTone"
#define METHOD_SET_AUTO_TONE	"SetAutoTone"
#define METHOD_GET_ENHANCED_TOUCH	"GetEnhancedTouch"
#define METHOD_SET_ENHANCED_TOUCH	"SetEnhancedTouch"
#define METHOD_GET_HBM			"GetHBM"
#define METHOD_SET_HBM			"SetHBM"
#define METHOD_SET_HBM_TIMEOUT		"SetHBMTimeout"

#define STR_LCD_OFF   "lcdoff"
#define STR_LCD_DIM   "lcddim"
#define STR_LCD_ON    "lcdon"
#define STR_SUSPEND   "suspend"

#define STR_STAYCURSTATE "staycurstate"
#define STR_GOTOSTATENOW "gotostatenow"

#define STR_HOLDKEYBLOCK "holdkeyblock"
#define STR_STANDBYMODE  "standbymode"
#define STR_NULL         "NULL"

#define STR_SLEEP_MARGIN "sleepmargin"
#define STR_RESET_TIMER  "resettimer"
#define STR_KEEP_TIMER   "keeptimer"

API int display_get_count(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_DISPLAY_COUNT, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_get_max_brightness(void)
{
	int ret;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_MAX_BRIGHTNESS, NULL, NULL);
	if (ret < 0)
		return DISPLAY_MAX_BRIGHTNESS;

	_D("get max brightness : %d", ret);
	return ret;
}

API int display_get_min_brightness(void)
{
	return DISPLAY_MIN_BRIGHTNESS;
}

API int display_get_brightness(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_BRIGHTNESS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_set_brightness_with_setting(int val)
{
	char str_val[32];
	char *arr[1];
	int ret;

	if (val < 0 || val > 100)
		return -EINVAL;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	ret = dbus_method_async(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_BRIGHTNESS, "i", arr);
	if (ret < 0)
		_E("no message : failed to setting");

	return ret;
}

API int display_set_brightness(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_HOLD_BRIGHTNESS, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_release_brightness(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_RELEASE_BRIGHTNESS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_get_acl_status(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_ACL_STATUS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_set_acl_status(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_ACL_STATUS, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_get_auto_screen_tone(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_AUTO_TONE, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_set_auto_screen_tone(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_AUTO_TONE, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_get_image_enhance_info(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_ENHANCE_SUPPORTED, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_get_image_enhance(int type)
{
	DBusError err;
	DBusMessage *msg;
	char str_type[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_type, sizeof(str_type), "%d", type);
	arr[0] = str_type;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_IMAGE_ENHANCE, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_set_image_enhance(int type, int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_type[32];
	char str_val[32];
	char *arr[2];
	int ret, ret_val;

	snprintf(str_type, sizeof(str_type), "%d", type);
	arr[0] = str_type;
	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[1] = str_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_IMAGE_ENHANCE, "ii", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_set_frame_rate(int val)
{
	return display_set_refresh_rate(REFRESH_SETTING, val);
}

API int display_set_refresh_rate(enum refresh_app app, int val)
{
	char str_app[32];
	char str_val[32];
	char *arr[2];

	snprintf(str_app, sizeof(str_app), "%d", app);
	arr[0] = str_app;
	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[1] = str_val;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_REFRESH_RATE, "ii", arr);
}

API int display_get_color_blind(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_COLOR_BLIND, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int display_set_color_blind(int enable, struct blind_color_info *info)
{
	DBusError err;
	DBusMessage *msg;
	char str_enable[32];
	char str_red[32];
	char str_grn[32];
	char str_blu[32];
	char *arr[4];
	int ret, ret_val;

	snprintf(str_enable, sizeof(str_enable), "%d", enable);
	arr[0] = str_enable;
	snprintf(str_red, sizeof(str_red), "%llu", BLIND_COLOR(info->RrCr, info->RgCg, info->RbCb));
	arr[1] = str_red;
	snprintf(str_grn, sizeof(str_grn), "%llu", BLIND_COLOR(info->GrMr, info->GgMg, info->GbMb));
	arr[2] = str_grn;
	snprintf(str_blu, sizeof(str_blu), "%llu", BLIND_COLOR(info->BrYr, info->BgYg, info->BbYb));
	arr[3] = str_blu;

	_D("red : %s, grn : %s, blu : %s", str_red, str_grn, str_blu);

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_COLOR_BLIND, "uttt", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

static inline char *get_lcd_str(unsigned int val)
{
	switch (val) {
	case LCD_NORMAL:
		return STR_LCD_ON;
	case LCD_DIM:
		return STR_LCD_DIM;
	case LCD_OFF:
		return STR_LCD_OFF;
	case SUSPEND:
		return STR_SUSPEND;
	default:
		return NULL;
	}
}

static void display_change_cb(void *data, DBusMessage *msg, DBusError *unused)
{
	DBusError err;
	int ret, val;

	if (!msg)
		return;

	dbus_error_init(&err);
	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}
        _D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_CHANGE_STATE, val);
}

API int display_change_state(unsigned int s_bits)
{
	char *p, *pa[1];
	int ret;

	p = get_lcd_str(s_bits);
	if (!p)
		return -EINVAL;
	pa[0] = p;

	ret = dbus_method_async_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_CHANGE_STATE, "s", pa, display_change_cb, -1, NULL);
	if (ret < 0)
		_E("no message : failed to change state");

	_D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_CHANGE_STATE, ret);

	return ret;
}

static void display_lock_cb(void *data, DBusMessage *msg, DBusError *unused)
{
	DBusError err;
	int ret, val;

	if (!msg)
		return;

	dbus_error_init(&err);
	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}
        _D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_LOCK_STATE, val);
}

API int display_lock_state(unsigned int s_bits, unsigned int flag,
		      unsigned int timeout)
{
	char *p, *pa[4];
	char str_timeout[32];
	int ret;

	p = get_lcd_str(s_bits);
	if (!p)
		return -EINVAL;
	pa[0] = p;

	if (flag & GOTO_STATE_NOW)
		/* if the flag is true, go to the locking state directly */
		p = STR_GOTOSTATENOW;
	else
		p = STR_STAYCURSTATE;
	pa[1] = p;

	if (flag & HOLD_KEY_BLOCK)
		p = STR_HOLDKEYBLOCK;
	else if (flag & STANDBY_MODE)
		p = STR_STANDBYMODE;
	else
		p = STR_NULL;
	pa[2] = p;

	if (timeout < 0)
		return -EINVAL;

	snprintf(str_timeout, sizeof(str_timeout), "%d", timeout);
	pa[3] = str_timeout;

	ret = dbus_method_async_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_LOCK_STATE, "sssi", pa, display_lock_cb, -1, NULL);
	if (ret < 0)
		_E("no message : failed to lock state");

	_D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_LOCK_STATE, ret);

	return ret;
}

static void display_unlock_cb(void *data, DBusMessage *msg, DBusError *unused)
{
	DBusError err;
	int ret, val;

	if (!msg)
		return;

	dbus_error_init(&err);
	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}
        _D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_UNLOCK_STATE, val);
}

API int display_unlock_state(unsigned int s_bits, unsigned int flag)
{
	char *p, *pa[2];
	int ret;

	p = get_lcd_str(s_bits);
	if (!p)
		return -EINVAL;
	pa[0] = p;

	switch (flag) {
	case PM_SLEEP_MARGIN:
		p = STR_SLEEP_MARGIN;
		break;
	case PM_RESET_TIMER:
		p = STR_RESET_TIMER;
		break;
	case PM_KEEP_TIMER:
		p = STR_KEEP_TIMER;
		break;
	default:
		return -EINVAL;
	}
	pa[1] = p;

	ret = dbus_method_async_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_UNLOCK_STATE, "ss", pa, display_unlock_cb, -1, NULL);
	if (ret < 0)
		_E("no message : failed to unlock state");

	_D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_UNLOCK_STATE, ret);

	return ret;
}

API int display_get_enhanced_touch(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_ENHANCED_TOUCH, NULL, NULL);
}

API int display_set_enhanced_touch(int enable)
{
	char *arr[1];
	char str_enable[32];

	snprintf(str_enable, sizeof(str_enable), "%d", enable);
	arr[0] = str_enable;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_ENHANCED_TOUCH, "i", arr);
}

API int display_get_hbm(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_HBM, NULL, NULL);
}

API int display_set_hbm(int enable)
{
	char *arr[1];
	char str_enable[2];

	if (enable != 0 && enable != 1)
		return -EINVAL;

	snprintf(str_enable, sizeof(str_enable), "%d", enable);
	arr[0] = str_enable;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_HBM, "i", arr);
}

API int display_enable_hbm(int enable, int timeout)
{
	char *arr[2];
	char str_enable[2];
	char str_timeout[32];

	if (enable != 0 && enable != 1)
		return -EINVAL;

	if (timeout < 0)
		return -EINVAL;

	snprintf(str_enable, sizeof(str_enable), "%d", enable);
	arr[0] = str_enable;

	snprintf(str_timeout, sizeof(str_timeout), "%d", timeout);
	arr[1] = str_timeout;

	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_HBM_TIMEOUT, "ii", arr);
}

