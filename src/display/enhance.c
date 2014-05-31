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
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <device-node.h>
#include <sys/un.h>
#include <stdarg.h>
#include <vconf.h>
#include <vconf-keys.h>
#include "core.h"
#include "core/data.h"
#include "core/devices.h"
#include "core/queue.h"
#include "core/log.h"
#include "core/common.h"
#include "proc/proc-handler.h"
#include "device-interface.h"

#define VCONFKEY_ENHANCE_MODE		"db/private/sysman/enhance_mode"
#define VCONFKEY_ENHANCE_SCENARIO	"db/private/sysman/enhance_scenario"
#define VCONFKEY_ENHANCE_TONE		"db/private/sysman/enhance_tone"
#define VCONFKEY_ENHANCE_OUTDOOR	"db/private/sysman/enhance_outdoor"
#define VCONFKEY_ENHANCE_PID		"memory/private/sysman/enhance_pid"

#ifndef VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT
#define VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT	"db/setting/auto_display_adjustment"
#endif

#define SETTING_NAME	"setting"
#define CLUSTER_HOME	"cluster-home"
#define BROWSER_NAME	"browser"

enum lcd_enhance_type{
	ENHANCE_MODE = 0,
	ENHANCE_SCENARIO,
	ENHANCE_TONE,
	ENHANCE_OUTDOOR,
	ENHANCE_MAX,
};

struct enhance_entry_t{
	int pid;
	int type[ENHANCE_MAX];
};

static Eina_List *enhance_ctl_list;
static struct enhance_entry_t default_enhance;

int get_glove_state(void)
{
	int ret, val;

	ret = device_get_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_SCREEN_GLOVE_MODE, &val);
	if (ret < 0) {
		return ret;
	}
	return val;
}

void switch_glove_key(int val)
{
	int ret, exval;

	ret = device_get_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_KEY_GLOVE_MODE, &exval);
	if (ret < 0 || exval != val) {
		ret = device_set_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_KEY_GLOVE_MODE, val);
		if (ret < 0)
			_E("fail to set touch key glove mode");
		else
			_D("glove key mode is %s", (val ? "on" : "off"));
	}
}

static int check_default_process(int pid, char *default_name)
{
	char exe_name[PATH_MAX];
	if (get_cmdline_name(pid, exe_name, PATH_MAX) != 0) {
		_E("fail to check cmdline: %d (%s)", pid, default_name);
		return -EINVAL;
	}
	if (strncmp(exe_name, default_name, strlen(default_name)) != 0) {
		return -EINVAL;
	}
	return 0;
}

static void restore_enhance_status(struct enhance_entry_t *entry)
{
	int ret;
	int scenario;
	static int pid;

	if (pid == entry->pid)
		return;

	pid = entry->pid;

	ret = device_get_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO, &scenario);
	if (ret != 0) {
		_E("fail to get status");
		return;
	}
	if (entry->type[ENHANCE_SCENARIO] == scenario)
		goto restore_enhance;

	_D("clear tone and outdoor");
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_TONE, 0);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR, 0);

restore_enhance:
	_D("restore [%d:%d:%d:%d]",
		entry->type[ENHANCE_MODE], entry->type[ENHANCE_SCENARIO],
		entry->type[ENHANCE_TONE], entry->type[ENHANCE_OUTDOOR]);

	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_MODE,
		entry->type[ENHANCE_MODE]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO,
		entry->type[ENHANCE_SCENARIO]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_TONE,
		entry->type[ENHANCE_TONE]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR,
		entry->type[ENHANCE_OUTDOOR]);
}

static int find_entry_from_enhance_ctl_list(int pid)
{
	Eina_List *n;
	Eina_List *next;
	struct enhance_entry_t* entry;
	char exe_name[PATH_MAX];

	EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
		if (entry != NULL && entry->pid == pid && get_cmdline_name(entry->pid, exe_name, PATH_MAX) == 0) {
			_D("find enhance list");
			restore_enhance_status(entry);
			return 1;
		}
	}
	return 0;
}

static void remove_entry_from_enhance_ctl_list(int pid)
{
	Eina_List *n;
	Eina_List *next;
	struct enhance_entry_t *entry;
	char exe_name[PATH_MAX];

	if (pid <= 0)
		return;

	EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
		if (entry != NULL &&
		    (get_cmdline_name(entry->pid, exe_name, PATH_MAX) != 0 || (entry->pid == pid))) {
			_D("remove enhance list");
			enhance_ctl_list = eina_list_remove(enhance_ctl_list, entry);
			free(entry);
		}
	}
}

static int check_entry_to_enhance_ctl_list(int pid)
{
	int oom_score_adj;
	if (pid <= 0)
		return -EINVAL;

	if (get_oom_score_adj(pid, &oom_score_adj) < 0) {
		_E("fail to get adj value of pid: %d (%d)", pid);
		return -EINVAL;
	}

	switch (oom_score_adj) {
	case OOMADJ_FOREGRD_LOCKED:
	case OOMADJ_FOREGRD_UNLOCKED:
		if (!find_entry_from_enhance_ctl_list(pid))
			return -EINVAL;
		return 0;
	case OOMADJ_BACKGRD_LOCKED:
	case OOMADJ_BACKGRD_UNLOCKED:
		remove_entry_from_enhance_ctl_list(pid);
		return -EINVAL;
	case OOMADJ_SU:
		if (check_default_process(pid, CLUSTER_HOME) != 0)
			return 0;
		return -EINVAL;
	default:
		return 0;
	}
}

static void update_enhance_status(struct enhance_entry_t *entry, int index)
{
	char exe_name[PATH_MAX];
	int type;

	if (index == ENHANCE_MODE)
		type = PROP_DISPLAY_IMAGE_ENHANCE_MODE;
	else if (index == ENHANCE_SCENARIO)
		type = PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO;
	else if (index == ENHANCE_TONE)
		type = PROP_DISPLAY_IMAGE_ENHANCE_TONE;
	else if (index == ENHANCE_OUTDOOR)
		type = PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR;
	else {
		_E("abnormal type is inserted(%d)", index);
		return;
	}

	_I("[ENHANCE(%d)] %d", index, entry->type[index]);
	device_set_property(DEVICE_TYPE_DISPLAY, type, entry->type[index]);
}

static int change_default_enhance_status(int pid, int index, int val)
{
	Eina_List *n, *next;
	struct enhance_entry_t *entry;
	char exe_name[PATH_MAX];

	if (check_default_process(pid, SETTING_NAME) != 0)
		return -EINVAL;
	default_enhance.pid = pid;
	default_enhance.type[index] = val;
	switch (index) {
	case ENHANCE_MODE:
		vconf_set_int(VCONFKEY_ENHANCE_MODE, val);
		EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
			if (!entry)
				continue;
			entry->type[index] = default_enhance.type[index];
		}
		break;
	case ENHANCE_SCENARIO:
		vconf_set_int(VCONFKEY_ENHANCE_SCENARIO, val);
		break;
	case ENHANCE_TONE:
		vconf_set_int(VCONFKEY_ENHANCE_TONE, val);
		break;
	case ENHANCE_OUTDOOR:
		vconf_set_int(VCONFKEY_ENHANCE_OUTDOOR, val);
		break;
	default:
		_E("input index is abnormal value");
		return -EINVAL;
	}
	update_enhance_status(&default_enhance, index);
	return 0;
}

static int add_entry_to_enhance_ctl_list(int pid, int index, int val)
{
	int find_node = 0;
	Eina_List *n, *next;
	struct enhance_entry_t *entry;
	struct enhance_entry_t *entry_buf;
	int oom_score_adj;

	if (pid <= 0)
		return -EINVAL;

	EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
		if (entry != NULL && entry->pid == pid) {
			find_node = 1;
			break;
		}
	}

	entry_buf = malloc(sizeof(struct enhance_entry_t));
	if (!entry_buf) {
		_E("Malloc failed");
		return -EINVAL;
	}

	if (find_node) {
		memcpy(entry_buf, entry, sizeof(struct enhance_entry_t));
		entry_buf->type[index] = val;
		remove_entry_from_enhance_ctl_list(pid);
	} else {
		memset(entry_buf, 0, sizeof(struct enhance_entry_t));
		entry_buf->type[ENHANCE_MODE] = default_enhance.type[ENHANCE_MODE];
		entry_buf->pid = pid;
		entry_buf->type[index] = val;
	}

	enhance_ctl_list = eina_list_prepend(enhance_ctl_list, entry_buf);
	if (!enhance_ctl_list) {
		_E("eina_list_prepend failed");
		return -EINVAL;
	}

	if (get_oom_score_adj(entry_buf->pid, &oom_score_adj) < 0) {
		_E("fail to get adj value of pid: %d (%d)", pid);
		return -EINVAL;
	}
	return 0;
}

static void enhance_control_pid_cb(keynode_t *in_key, struct main_data *ad)
{
	int pid;

	if (vconf_get_int(VCONFKEY_ENHANCE_PID, &pid) != 0)
		return;

	if (check_entry_to_enhance_ctl_list(pid) == 0)
		return;

	restore_enhance_status(&default_enhance);
}

static void enhance_auto_control_cb(keynode_t *in_key, struct main_data *ad)
{
	int val;

	if (vconf_get_bool(VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT, &val) != 0) {
		_E("fail to get %s", VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT);
		return;
	}

	_I("set auto adjust screen tone (%d)", val);
	device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_AUTO_SCREEN_TONE, val);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_MODE,
		default_enhance.type[ENHANCE_MODE]);
}

static void init_colorblind_status(void)
{
	struct color_blind_info info;
	char *str, *it;
	int val, cnt, cmd, ret, sum;
	unsigned int color[9];

	/* get the status if colorblind is ON or not */
	if (vconf_get_bool(VCONFKEY_SETAPPL_COLORBLIND_STATUS_BOOL, &val) != 0)
		return;

	if (!val) {
		_D("color blind status is FALSE");
		return;
	}

	/* get the value of color blind */
	str = vconf_get_str(VCONFKEY_SETAPPL_COLORBLIND_LAST_RGBCMY_STR);
	if (!str)
		return;

	cnt = strlen(str)/4 - 1;
	if (cnt != 8)
		return;

	/* token the color blind value string to integer */
	sum = 0;
	for (it = str+cnt*4; cnt >= 0 && it; --cnt, it -= 4) {
		color[cnt] = strtol(it, NULL, 16);
		sum += color[cnt];
		*it = '\0';
		_D("color[%d] : %d", cnt, color[cnt]);
	}

	/* ignore colorblind when all of value is invalid */
	if (!sum)
		return;

	cnt = 0;
	info.power = val;
	info.RrCr = color[cnt++];
	info.RgCg = color[cnt++];
	info.RbCb = color[cnt++];
	info.GrMr = color[cnt++];
	info.GgMg = color[cnt++];
	info.GbMb = color[cnt++];
	info.BrYr = color[cnt++];
	info.BgYg = color[cnt++];
	info.BbYb = color[cnt++];

	/* write to the kernel node */
	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_COLOR_BLIND, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, (int)&info);
	if (ret < 0)
		_E("fail to set color blind value : %d", ret);
}

static void reset_default_enhance_status(struct enhance_entry_t *entry)
{
	_D("reset [%d:%d:%d:%d]",
		entry->type[ENHANCE_MODE], entry->type[ENHANCE_SCENARIO],
		entry->type[ENHANCE_TONE], entry->type[ENHANCE_OUTDOOR]);

	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_MODE,
		entry->type[ENHANCE_MODE]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO,
		entry->type[ENHANCE_SCENARIO]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_TONE,
		entry->type[ENHANCE_TONE]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR,
		entry->type[ENHANCE_OUTDOOR]);
}

static void init_default_enhance_status(void)
{
	int val;

	memset(&default_enhance, 0, sizeof(struct enhance_entry_t));

	if (vconf_get_bool(VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT, &val) == 0) {
		_I("set auto adjust screen tone (%d)", val);
		device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_AUTO_SCREEN_TONE, val);
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT,
		(void *)enhance_auto_control_cb, NULL) < 0) {
		_E("failed to set : KEY(%s)", VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT);
	}

	if (vconf_get_int(VCONFKEY_ENHANCE_MODE, &val) == 0 && val >= 0) {
		default_enhance.type[ENHANCE_MODE] = val;
	}
	if (vconf_get_int(VCONFKEY_ENHANCE_SCENARIO, &val) == 0 && val >= 0) {
		default_enhance.type[ENHANCE_SCENARIO] = val;
	}
	if (vconf_get_int(VCONFKEY_ENHANCE_TONE, &val) == 0 && val >= 0) {
		default_enhance.type[ENHANCE_TONE] = val;
	}
	if (vconf_get_int(VCONFKEY_ENHANCE_OUTDOOR, &val) == 0 && val >= 0) {
		default_enhance.type[ENHANCE_OUTDOOR] = val;
	}
	reset_default_enhance_status(&default_enhance);
}

int changed_enhance_value(int pid, int prop, int val)
{
	int index;

	index = prop - PROP_DISPLAY_IMAGE_ENHANCE_MODE;

	if (change_default_enhance_status(pid, index, val) == 0)
		return 0;
	if (add_entry_to_enhance_ctl_list(pid, index, val) == 0)
		return 0;

	_E("fail to set enhance (p:%d,t:%d,v:%d)", pid, index, val);
	return -EINVAL;
}

int set_enhance_pid(int pid)
{
	return vconf_set_int(VCONFKEY_ENHANCE_PID, pid);
}

static DBusMessage *edbus_getenhancesupported(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, val, ret;

	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_INFO, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &val);
	if (ret >= 0)
		ret = val;

	_I("get imange enhance supported %d, %d", val, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getimageenhance(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int type, prop, cmd, val, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &type);

	_I("get image enhance type %d", type);

	switch (type) {
	case ENHANCE_MODE:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_MODE;
		break;
	case ENHANCE_SCENARIO:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO;
		break;
	case ENHANCE_TONE:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_TONE;
		break;
	case ENHANCE_OUTDOOR:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	cmd = DISP_CMD(prop, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &val);
	if (ret >= 0)
		ret = val;

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_setimageenhance(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int type, prop, cmd, val, ret;
	pid_t pid;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &type, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	_I("set image enhance type %d, %d", type, val);

	switch (type) {
	case ENHANCE_MODE:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_MODE;
		break;
	case ENHANCE_SCENARIO:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO;
		break;
	case ENHANCE_TONE:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_TONE;
		break;
	case ENHANCE_OUTDOOR:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	cmd = DISP_CMD(prop, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, val);

	/* notify to deviced with the purpose of stroing latest enhance value */
	pid = get_edbus_sender_pid(msg);
	changed_enhance_value(pid, prop, val);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_getenhancedtouch(E_DBus_Object *obj, DBusMessage *msg)
{
	int ret;
	int val;
	DBusMessageIter iter;
	DBusMessage *reply;

	ret = device_get_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_SCREEN_GLOVE_MODE, &val);
	if (ret < 0) {
		val = -1;
	}

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &val);
	return reply;
}

static DBusMessage *edbus_setenhancedtouch(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, exval, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (!val)
		ret = device_set_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_KEY_GLOVE_MODE, val);
	if (ret < 0)
		_E("fail to off touch key glove mode");

	ret = device_get_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_SCREEN_GLOVE_MODE, &exval);
	if (ret < 0 || exval != val)
		ret = device_set_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_SCREEN_GLOVE_MODE, val);
	if (ret < 0) {
		_E("fail to set touch screen glove mode (%d)", val);
		goto error;
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "getimageenhance",  "i",   "i", edbus_getimageenhance },	/* deprecated */
	{ "setimageenhance", "ii",   "i", edbus_setimageenhance },	/* deprecated */
	{ "GetEnhanceSupported", NULL, "i", edbus_getenhancesupported },
	{ "GetImageEnhance",  "i",   "i", edbus_getimageenhance },
	{ "SetImageEnhance", "ii",   "i", edbus_setimageenhance },
	{ "GetEnhancedTouch",    NULL,   "i", edbus_getenhancedtouch },
	{ "SetEnhancedTouch",     "i",   "i", edbus_setenhancedtouch },
};

static void enhance_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_DISPLAY,
		    edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("Failed to register edbus method! %d", ret);

	if (vconf_notify_key_changed(VCONFKEY_ENHANCE_PID,
		(void *)enhance_control_pid_cb, NULL) < 0) {
		_E("failed to set : KEY(%s)", VCONFKEY_ENHANCE_PID);
	}

	init_default_enhance_status();
	init_colorblind_status();
}

static const struct device_ops enhance_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "enhance",
	.init     = enhance_init,
};

DEVICE_OPS_REGISTER(&enhance_device_ops)
