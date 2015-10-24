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
#include <errno.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <fcntl.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/udev.h"
#include "proc-handler.h"
#include "core/edbus-handler.h"
#include "shared/score-defines.h"

#define TEMPERATURE_DBUS_INTERFACE	"org.tizen.trm.siop"
#define TEMPERATURE_DBUS_PATH		"/Org/Tizen/Trm/Siop"
#define TEMPERATURE_DBUS_SIGNAL		"ChangedTemperature"

#define OOMADJ_SET			"oomadj_set"

#define BUFF_MAX	255
#define SIOP_CTRL_LEVEL_MASK	0xFFFF
#define SIOP_CTRL_LEVEL(val)	((val & SIOP_CTRL_LEVEL_MASK) << 16)
#define SIOP_CTRL_VALUE(s, r)	(s | (r << 4))
#define SIOP_VALUE(d, val)	((d) * (val & 0xF))
#define REAR_VALUE(val)	(val >> 4)

#define SIGNAL_SIOP_CHANGED	"ChangedSiop"
#define SIGNAL_REAR_CHANGED	"ChangedRear"
#define SIOP_LEVEL_GET		"GetSiopLevel"
#define REAR_LEVEL_GET		"GetRearLevel"
#define SIOP_LEVEL_SET		"SetSiopLevel"
#define OOM_PMQOS_TIME		2000 /* ms */

#define SIGNAL_PROC_STATUS	"ProcStatus"

enum proc_status_type {
	PROC_STATUS_LAUNCH,
	PROC_STATUS_RESUME,
	PROC_STATUS_TERMINATE,
	PROC_STATUS_FOREGROUND,
	PROC_STATUS_BACKGROUND,
};

enum SIOP_DOMAIN_TYPE {
	SIOP_NEGATIVE = -1,
	SIOP_POSITIVE = 1,
};

struct siop_data {
	int siop;
	int rear;
};

static int siop;
static int mode;
static int siop_domain = SIOP_POSITIVE;

enum siop_scenario {
	MODE_NONE = 0,
	MODE_LCD = 1,
};

int cur_siop_level(void)
{
	return  SIOP_VALUE(siop_domain, siop);
}

static void siop_level_action(int level)
{
	int val = SIOP_CTRL_LEVEL(level);
	static int old;
	static int siop_level;
	static int rear_level;
	static int initialized;
	static int domain;
	char *arr[1];
	char str_level[32];

	if (initialized && siop == level && mode == old && domain == siop_domain)
		return;
	initialized = 1;
	siop = level;
	domain = siop_domain;
	if (siop_domain == SIOP_NEGATIVE)
		val = 0;
	old = mode;
	val |= old;

	device_set_property(DEVICE_TYPE_POWER, PROP_POWER_SIOP_CONTROL, val);

	val = SIOP_VALUE(siop_domain, level);
	if (siop_level != val) {
		siop_level = val;
		snprintf(str_level, sizeof(str_level), "%d", siop_level);
		arr[0] = str_level;
		_I("broadcast siop %s", str_level);
		broadcast_edbus_signal(DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			SIGNAL_SIOP_CHANGED, "i", arr);
	}

	val = REAR_VALUE(level);
	if (rear_level != val) {
		rear_level = val;
		snprintf(str_level, sizeof(str_level), "%d", rear_level);
		arr[0] = str_level;
		_I("broadcast rear %s", str_level);
		broadcast_edbus_signal(DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			SIGNAL_REAR_CHANGED, "i", arr);
	}
	_I("level is d:%d(0x%x) s:%d r:%d", siop_domain, siop, siop_level, rear_level);
}

static int siop_changed(int argc, char **argv)
{
	int siop_level = 0;
	int rear_level = 0;
	int level;
	int ret;

	if (argc != 2 || argv[0] == NULL) {
		_E("fail to check value");
		return -1;
	}

	if (argv[0] == NULL)
		goto out;

	level = atoi(argv[0]);
	device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_ELVSS_CONTROL, level);
	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_SIOP_LEVEL, &level);
	if (ret != 0)
		goto check_rear;

	if (level <= SIOP_NEGATIVE)
		siop_domain = SIOP_NEGATIVE;
	else
		siop_domain = SIOP_POSITIVE;
	siop_level = siop_domain * level;

check_rear:
	if (argv[1] == NULL)
		goto out;

	level = atoi(argv[1]);
	if (device_get_property(DEVICE_TYPE_POWER, PROP_POWER_REAR_LEVEL, &level) == 0)
		rear_level = level;

out:
	level = SIOP_CTRL_VALUE(siop_level, rear_level);
	siop_level_action(level);
	return 0;
}

static int siop_mode_lcd(keynode_t *key_nodes, void *data)
{
	int pm_state;
	if (vconf_get_int(VCONFKEY_PM_STATE, &pm_state) != 0)
		_E("Fail to get vconf value for pm state\n");
	if (pm_state == VCONFKEY_PM_STATE_LCDOFF)
		mode = MODE_LCD;
	else
		mode = MODE_NONE;
	siop_level_action(siop);
	return 0;
}

static void memcg_move_group(int pid, int oom_score_adj)
{
	char buf[100];
	FILE *f;
	int size;
	char exe_name[PATH_MAX];

	if (get_cmdline_name(pid, exe_name, PATH_MAX) != 0) {
		_E("fail to get process name(%d)", pid);
		return;
	}

	_SD("memcg_move_group : %s, pid = %d", exe_name, pid);
	if (oom_score_adj >= OOMADJ_BACKGRD_LOCKED)
		snprintf(buf, sizeof(buf), "/sys/fs/cgroup/memory/background/cgroup.procs");
	else if (oom_score_adj >= OOMADJ_FOREGRD_LOCKED && oom_score_adj < OOMADJ_BACKGRD_LOCKED)
		snprintf(buf, sizeof(buf), "/sys/fs/cgroup/memory/foreground/cgroup.procs");
	else
		return;

	f = fopen(buf, "w");
	if (f == NULL)
		return;
	size = snprintf(buf, sizeof(buf), "%d", pid);
	if (fwrite(buf, size, 1, f) != 1)
		_E("fwrite cgroup tasks : %d", pid);
	fclose(f);
}

int set_oom_score_adj_action(int argc, char **argv)
{
	FILE *fp;
	int pid = -1;
	int new_oom_score_adj = 0;

	if (argc < 2)
		return -1;
	pid = atoi(argv[0]);
	new_oom_score_adj = atoi(argv[1]);
	if (pid < 0 || new_oom_score_adj <= -20)
		return -1;

	_I("OOMADJ_SET : pid %d, new_oom_score_adj %d", pid, new_oom_score_adj);

	fp = open_proc_oom_score_adj_file(pid, "w");
	if (fp == NULL)
		return -1;
	if (new_oom_score_adj < OOMADJ_SU)
		new_oom_score_adj = OOMADJ_SU;
	fprintf(fp, "%d", new_oom_score_adj);
	fclose(fp);

	memcg_move_group(pid, new_oom_score_adj);
	return 0;
}

static DBusMessage *dbus_oom_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
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

	if (strncmp(type_str, OOMADJ_SET, strlen(OOMADJ_SET)) == 0)
		ret = set_oom_score_adj_action(argc, (char **)&argv);
	else
		ret = -EINVAL;

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_get_siop_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int level;

	level = SIOP_VALUE(siop_domain, siop);
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &level);
	return reply;
}

static DBusMessage *dbus_get_rear_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int level;

	level = REAR_VALUE(siop);
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &level);
	return reply;
}

static DBusMessage *dbus_set_siop_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}
	ret = siop_changed(2, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static void proc_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret, type;
	pid_t pid;

	ret = dbus_message_is_signal(msg, RESOURCED_INTERFACE_PROCESS,
	    SIGNAL_PROC_STATUS);
	if (!ret) {
		_E("It's not active signal!");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &type,
	    DBUS_TYPE_INT32, &pid, DBUS_TYPE_INVALID) == 0) {
		_E("There's no arguments!");
		return;
	}

	if (type == PROC_STATUS_BACKGROUND)
		device_notify(DEVICE_NOTIFIER_PROCESS_BACKGROUND, &pid);
}

static const struct edbus_method edbus_methods[] = {
	{ OOMADJ_SET, "siss", "i", dbus_oom_handler },
	{ SIOP_LEVEL_GET, NULL, "i", dbus_get_siop_level },
	{ REAR_LEVEL_GET, NULL, "i", dbus_get_rear_level },
	{ SIOP_LEVEL_SET, "ss", "i", dbus_set_siop_level },
};

static int proc_booting_done(void *data)
{
	static int done;

	if (data == NULL)
		goto out;
	done = *(int *)data;
	if (vconf_notify_key_changed(VCONFKEY_PM_STATE, (void *)siop_mode_lcd, NULL) < 0)
		_E("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_PM_STATE);
	siop_mode_lcd(NULL, NULL);
out:
	return done;
}

static int process_execute(void *data)
{
	struct siop_data *key_data = (struct siop_data *)data;
	int siop_level = 0;
	int rear_level = 0;
	int level;
	int ret;
	int booting_done;

	booting_done = proc_booting_done(NULL);
	if (!booting_done)
		return 0;

	if (key_data == NULL)
		goto out;

	level = key_data->siop;
	device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_ELVSS_CONTROL, level);
	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_SIOP_LEVEL, &level);
	if (ret != 0)
		goto check_rear;

	if (level <= SIOP_NEGATIVE)
		siop_domain = SIOP_NEGATIVE;
	else
		siop_domain = SIOP_POSITIVE;
	siop_level = siop_domain * level;

check_rear:
	level = key_data->rear;
	if (device_get_property(DEVICE_TYPE_POWER, PROP_POWER_REAR_LEVEL, &level) == 0)
		rear_level = level;

out:
	level = SIOP_CTRL_VALUE(siop_level, rear_level);
	siop_level_action(level);
	return 0;
}

static void temp_change_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int level = 0;

	if (dbus_message_is_signal(msg, TEMPERATURE_DBUS_INTERFACE, TEMPERATURE_DBUS_SIGNAL) == 0) {
		_E("there is no cool down signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &level, DBUS_TYPE_INVALID) == 0) {
		_E("there is no message");
		return;
	}
	_D("%d", level);
	device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_ELVSS_CONTROL, level);
}

static void proc_change_lowmemory(keynode_t *key, void *data)
{
	int state = 0;

	if (vconf_get_int(VCONFKEY_SYSMAN_LOW_MEMORY, &state))
		return;

	if (state == VCONFKEY_SYSMAN_LOW_MEMORY_HARD_WARNING)
		device_notify(DEVICE_NOTIFIER_PMQOS_OOM, (void *)OOM_PMQOS_TIME);
}

static void uevent_platform_handler(struct udev_device *dev)
{
	const char *devpath;
	const char *siop_level;
	const char *rear_level;
	struct siop_data params = {0,};

	devpath = udev_device_get_devpath(dev);
	if (!devpath)
		return;

	if (!fnmatch(THERMISTOR_PATH, devpath, 0)) {
		siop_level = udev_device_get_property_value(dev,
				"TEMPERATURE");
		if (!siop_level)
			params.siop = atoi(siop_level);

		rear_level = udev_device_get_property_value(dev,
				"REAR_TEMPERATURE");
		if (!rear_level)
			params.rear = atoi(rear_level);

		process_execute(&params);
	}
}

static struct uevent_handler uh = {
	.subsystem = PLATFORM_SUBSYSTEM,
	.uevent_func = uevent_platform_handler,
};

static void process_init(void *data)
{
	int ret;

	register_edbus_signal_handler(RESOURCED_PATH_PROCESS,
	    RESOURCED_INTERFACE_PROCESS, SIGNAL_PROC_STATUS, proc_signal_handler);

	register_edbus_signal_handler(TEMPERATURE_DBUS_PATH, TEMPERATURE_DBUS_INTERFACE,
			TEMPERATURE_DBUS_SIGNAL,
		    temp_change_signal_handler);

	ret = register_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register platform uevent : %d", ret);

	ret = register_edbus_method(DEVICED_PATH_PROCESS, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, proc_booting_done);

	vconf_notify_key_changed(VCONFKEY_SYSMAN_LOW_MEMORY,
			proc_change_lowmemory, NULL);
}

static void process_exit(void *data)
{
	int ret;

	ret = unregister_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister platform uevent : %d", ret);
}

static const struct device_ops process_device_ops = {
	.name     = "process",
	.init     = process_init,
	.exit     = process_exit,
	.execute  = process_execute,
};

DEVICE_OPS_REGISTER(&process_device_ops)
