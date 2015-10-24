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
#include <math.h>
#include <time.h>

#include "core.h"
#include "core/devices.h"
#include "core/log.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "proc/proc-handler.h"
#include "powersaver/weaks.h"
#include "device-interface.h"
#include "weaks.h"

#define VCONFKEY_ENHANCE_MODE		"db/private/sysman/enhance_mode"
#define VCONFKEY_ENHANCE_SCENARIO	"db/private/sysman/enhance_scenario"

#ifndef VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT
#define VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT	"db/setting/auto_display_adjustment"
#endif

#define SETTING_NAME	"setting"
#define CLUSTER_HOME	"cluster-home"
#define BROWSER_NAME	"browser"

#define SIGNAL_GLOVEMODE_ON	"GloveModeOn"
#define SIGNAL_GLOVEMODE_OFF	"GloveModeOff"
#define SIGNAL_MDNIE_ACTION	"mDNIeStatus"

#define MDNIE_BACKGROUND	0
#define MDNIE_FOREGROUND	1

#define SCENARIO_CAMERA		4
#define SCENARIO_BROWSER	5

enum lcd_enhance_type {
	ENHANCE_MODE = 0,
	ENHANCE_SCENARIO,
	ENHANCE_MAX,
};

struct enhance_entry_t {
	int pid;
	const char *name;
	int type[ENHANCE_MAX];
};

static Eina_List *enhance_ctl_list;
static struct enhance_entry_t default_enhance;
static pid_t last_enhance_pid;
static int last_brightness;

int get_glove_state(void)
{
	int ret, val;

	ret = device_get_property(DEVICE_TYPE_TOUCH, PROP_TOUCH_SCREEN_GLOVE_MODE, &val);
	if (ret < 0)
		return ret;

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

static void broadcast_glove_mode(int state)
{
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    (state ? SIGNAL_GLOVEMODE_ON : SIGNAL_GLOVEMODE_OFF), NULL, NULL);
}

static int check_default_process(int pid, char *default_name)
{
	char exe_name[PATH_MAX];
	if (get_cmdline_name(pid, exe_name, PATH_MAX) != 0) {
		_E("fail to check cmdline: %d (%s)", pid, default_name);
		return -EINVAL;
	}
	if (strncmp(exe_name, default_name, strlen(default_name)) != 0)
		return -EINVAL;

	return 0;
}

static int find_scenario_from_enhance_ctl_list(int scenario)
{
	Eina_List *n;
	Eina_List *next;
	struct enhance_entry_t *entry;

	EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
		if (entry && entry->type[ENHANCE_SCENARIO] == scenario)
			return true;
	}
	return false;
}

static void change_brightness_transit(int start, int end)
{
	int diff, step;
	int change_step = display_conf.brightness_change_step;
	struct timespec time = {0, 35 * NANO_SECOND_MULTIPLIER};

	diff = end - start;
	if (abs(diff) < change_step)
		step = (diff > 0 ? 1 : -1);
	else
		step = (int)ceil(diff / (float)change_step);

	_D("%d", step);
	while (start != end) {
		if (step == 0)
			break;

		start += step;
		if ((step > 0 && start > end) ||
		    (step < 0 && start < end))
			start = end;

		backlight_ops.set_default_brt(start);
		backlight_ops.update();

		nanosleep(&time, NULL);
	}
}

static void set_mbm_on(int on)
{
	int ret, cmd, old_level, val;

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	if (device_get_property(DEVICE_TYPE_DISPLAY, cmd, &old_level) < 0) {
		_E("Fail to get display brightness!");
		return;
	}

	_D("mbm %d, current brt %d", on, old_level);

	if (on) {
		change_brightness_transit(old_level, PM_MAX_BRIGHTNESS);
		if (mbm_turn_on)
			mbm_turn_on();
	} else {
		if (mbm_turn_off)
			mbm_turn_off();
		ret = vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &val);
		if (!ret && val != old_level)
			change_brightness_transit(old_level, val);
		backlight_ops.set_default_brt(val);
	}
}

static void check_mbm_scenario(int scenario)
{
	if (find_scenario_from_enhance_ctl_list(SCENARIO_CAMERA) ||
	    scenario == SCENARIO_CAMERA) {
		if (scenario == SCENARIO_CAMERA &&
		    pm_status_flag & EXTBRT_FLAG) {
			set_mbm_on(true);
		} else if (last_brightness == PM_MAX_BRIGHTNESS) {
			set_mbm_on(true);
		} else {
			set_mbm_on(false);
		}
	} else {
		set_mbm_on(false);
	}
}

static void restore_enhance_status(struct enhance_entry_t *entry, int force)
{
	int ret, cmd;
	int scenario, mode;
	static int pid;

	if (pid == entry->pid && !force)
		return;

	pid = entry->pid;

	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &scenario);
	if ((!ret && entry->type[ENHANCE_SCENARIO] != scenario) || force)
		device_set_property(DEVICE_TYPE_DISPLAY,
		    PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO,
		    entry->type[ENHANCE_SCENARIO]);

	cmd = DISP_CMD(PROP_DISPLAY_IMAGE_ENHANCE_MODE, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &mode);
	if ((!ret && entry->type[ENHANCE_MODE] != mode) || force)
		device_set_property(DEVICE_TYPE_DISPLAY,
		    PROP_DISPLAY_IMAGE_ENHANCE_MODE,
		    entry->type[ENHANCE_MODE]);

	_D("restore(%d,%s) [%d:%d]", pid, entry->name,
	    entry->type[ENHANCE_MODE], entry->type[ENHANCE_SCENARIO]);

	check_mbm_scenario(entry->type[ENHANCE_SCENARIO]);
}

static int find_entry_from_enhance_ctl_list(int pid, int force)
{
	Eina_List *n;
	Eina_List *next;
	struct enhance_entry_t *entry;
	char exe_name[PATH_MAX];

	EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
		if (entry != NULL && entry->pid == pid && get_cmdline_name(entry->pid, exe_name, PATH_MAX) == 0) {
			_D("find enhance list");
			restore_enhance_status(entry, force);
			return 1;
		}
	}
	return 0;
}

static void remove_entry_from_enhance_ctl_list(const char *name)
{
	Eina_List *n;
	Eina_List *next;
	struct enhance_entry_t *entry;

	if (!name)
		return;

	EINA_LIST_FOREACH_SAFE(enhance_ctl_list, n, next, entry) {
		if (entry != NULL && entry->name != NULL &&
		    !strncmp(entry->name, name, strlen(name))) {
			_D("remove enhance list %d, %s", entry->pid, entry->name);
			if (entry->type[ENHANCE_SCENARIO] == SCENARIO_CAMERA)
				set_mbm_on(false);
			enhance_ctl_list = eina_list_remove(enhance_ctl_list, entry);
			free((void *)entry->name);
			free(entry);
		}
	}
}

static int update_enhance_status(int index, int val)
{
	int type, old_val;
	int ret, cmd;

	if (index == ENHANCE_MODE)
		type = PROP_DISPLAY_IMAGE_ENHANCE_MODE;
	else if (index == ENHANCE_SCENARIO)
		type = PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO;
	else {
		_E("abnormal type(%d)", index);
		return -EINVAL;
	}

	/* get enhance value */
	cmd = DISP_CMD(type, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY,
	    cmd, &old_val);

	if (ret < 0) {
		_E("Failed to get value %d", type);
		return ret;
	}

	/* set enhance value */
	if (old_val != val) {
		_I("[ENHANCE(%d)] %d", index, val);
		ret = device_set_property(DEVICE_TYPE_DISPLAY, type, val);
	}

	return ret;
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
	default:
		_E("input index is abnormal value");
		return -EINVAL;
	}
	update_enhance_status(index, val);
	return 0;
}

static int add_entry_to_enhance_ctl_list(int pid, const char *name, int index, int val)
{
	int find_node = 0;
	Eina_List *n, *next;
	struct enhance_entry_t *entry;
	struct enhance_entry_t *entry_buf;

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
		entry_buf->name = strndup(name, strlen(name));
		if (!entry_buf->name)
			_E("Failed to malloc for entry name!");
		entry_buf->type[index] = val;
		remove_entry_from_enhance_ctl_list(name);
	} else {
		memset(entry_buf, 0, sizeof(struct enhance_entry_t));
		entry_buf->type[ENHANCE_MODE] = default_enhance.type[ENHANCE_MODE];
		entry_buf->pid = pid;
		entry_buf->name = strndup(name, strlen(name));
		if (!entry_buf->name)
			_E("Failed to malloc for entry name!");
		entry_buf->type[index] = val;
	}

	enhance_ctl_list = eina_list_prepend(enhance_ctl_list, entry_buf);
	if (!enhance_ctl_list) {
		_E("eina_list_prepend failed");
		return -EINVAL;
	}

	if (last_enhance_pid == pid)
		restore_enhance_status(entry_buf, false);

	return 0;
}

static void update_enhance_state(pid_t pid, int force)
{
	if (pid < 0)
		return;

	if (!find_entry_from_enhance_ctl_list(pid, force))
		restore_enhance_status(&default_enhance, force);
}

static void enhance_auto_control_cb(keynode_t *in_key, void *data)
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

static void reset_default_enhance_status(struct enhance_entry_t *entry)
{
	_D("reset [%d:%d]",
		entry->type[ENHANCE_MODE], entry->type[ENHANCE_SCENARIO]);

	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_MODE,
		entry->type[ENHANCE_MODE]);
	device_set_property(DEVICE_TYPE_DISPLAY,
		PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO,
		entry->type[ENHANCE_SCENARIO]);

	check_mbm_scenario(entry->type[ENHANCE_SCENARIO]);
}

static void init_default_enhance_status(void)
{
	int val, ret;

	memset(&default_enhance, 0, sizeof(struct enhance_entry_t));

	if (vconf_get_bool(VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT, &val) == 0) {
		_I("set auto adjust screen tone (%d)", val);
		device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_AUTO_SCREEN_TONE, val);
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT,
		(void *)enhance_auto_control_cb, NULL) < 0) {
		_E("failed to set : KEY(%s)", VCONFKEY_SETAPPL_LCD_AUTO_DISPLAY_ADJUSTMENT);
	}

	if (vconf_get_int(VCONFKEY_ENHANCE_MODE, &val) == 0 && val >= 0)
		default_enhance.type[ENHANCE_MODE] = val;
	if (vconf_get_int(VCONFKEY_ENHANCE_SCENARIO, &val) == 0 && val >= 0)
		default_enhance.type[ENHANCE_SCENARIO] = val;

	default_enhance.pid = getpid();
	reset_default_enhance_status(&default_enhance);
}

static int check_extend_brightness_scenario(int index, int val)
{
	if (index != ENHANCE_SCENARIO)
		return false;

	if (val == SCENARIO_CAMERA)
		return true;

	return false;
}

static int changed_enhance_value(int pid, const char *name, int prop, int val)
{
	int index;

	index = prop - PROP_DISPLAY_IMAGE_ENHANCE_MODE;

	if (check_extend_brightness_scenario(index, val))
		pm_status_flag |= EXTBRT_FLAG;

	if (change_default_enhance_status(pid, index, val) == 0)
		return 0;
	if (add_entry_to_enhance_ctl_list(pid, name, index, val) == 0)
		return 0;

	_E("fail to set enhance (p:%d,n:%s,t:%d,v:%d)", pid, name, index, val);
	return -EINVAL;
}

static void reset_enhance(const char *sender, void *data)
{
	if (!sender)
		return;

	_D("%s", sender);
	remove_entry_from_enhance_ctl_list(sender);
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
	const char *name;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &type, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	switch (type) {
	case ENHANCE_MODE:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_MODE;
		break;
	case ENHANCE_SCENARIO:
		prop = PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	/* notify to deviced with the purpose of stroing latest enhance value */
	pid = get_edbus_sender_pid(msg);
	name = dbus_message_get_sender(msg);

	_I("set image enhance type %d, %d, %d", pid, type, val);

	if (last_enhance_pid == pid)
		ret = update_enhance_status(type, val);

	changed_enhance_value(pid, name, prop, val);
	register_edbus_watch(name, reset_enhance, NULL);
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
	if (ret < 0)
		val = -1;

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
	broadcast_glove_mode(val);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static void mdnie_status_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret, action, pid;

	ret = dbus_message_is_signal(msg, RESOURCED_INTERFACE_PROCESS,
	    SIGNAL_MDNIE_ACTION);
	if (!ret) {
		_E("there is no %s signal", SIGNAL_MDNIE_ACTION);
		return;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &action,
	    DBUS_TYPE_INT32, &pid, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
	}

	if (action == MDNIE_FOREGROUND) {
		_D("pid : %d", pid);
		last_enhance_pid = pid;
		update_enhance_state((pid_t)pid, false);
	}
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

static int lcd_state_changed(void *data)
{
	int state = (int)data;
	int ret;

	switch (state) {
	case S_LCDDIM:
	case S_LCDOFF:
	case S_SLEEP:
		if (hbm_turn_off)
			hbm_turn_off();
		break;
	}

	return 0;
}

static int setting_brightness_changed(void *data)
{
	int cmd, ret, scenario;

	if (!data)
		return 0;

	last_brightness = *(int *)data;

	if (last_brightness == PM_MAX_BRIGHTNESS &&
	    find_scenario_from_enhance_ctl_list(SCENARIO_CAMERA)) {
		if (mbm_turn_on)
			mbm_turn_on();
	} else {
		if (mbm_turn_off)
			mbm_turn_off();
	}

	return 0;
}

static void enhance_init(void *data)
{
	int ret;

	/* init dbus methods */
	ret = register_edbus_method(DEVICED_PATH_DISPLAY,
		    edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("Failed to register edbus method! %d", ret);

	/* register signal handlers */
	ret = register_edbus_signal_handler(RESOURCED_PATH_PROCESS,
		    RESOURCED_INTERFACE_PROCESS, SIGNAL_MDNIE_ACTION,
		    mdnie_status_signal_handler);
	if (ret < 0 && ret != -EEXIST)
		_E("Failed to register signal handler! %d", ret);

	/* init enhance status */
	init_default_enhance_status();

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_LCD, lcd_state_changed);
	register_notifier(DEVICE_NOTIFIER_SETTING_BRT_CHANGED,
	    setting_brightness_changed);
}

static void enhance_exit(void *data)
{
	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_LCD, lcd_state_changed);
	unregister_notifier(DEVICE_NOTIFIER_SETTING_BRT_CHANGED,
	    setting_brightness_changed);
}

static const struct device_ops enhance_device_ops = {
	.name     = "enhance",
	.init     = enhance_init,
	.exit     = enhance_exit,
};

DEVICE_OPS_REGISTER(&enhance_device_ops)

