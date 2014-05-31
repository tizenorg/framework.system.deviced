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


#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <vconf.h>

#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include "dd-deviced.h"
#include "log.h"
#include "launch.h"
#include "queue.h"
#include "device-handler.h"
#include "device-node.h"
#include "predefine.h"
#include "proc/proc-handler.h"
#include "data.h"
#include "common.h"
#include "display/poll.h"
#include "display/setting.h"
#include "devices.h"
#include "battery/battery.h"
#include "edbus-handler.h"

#define VCONFKEY_SYSMAN_FACTORY_MODE			"memory/sysman/factory_mode"

#define PREDEFINE_SO_DIR		PREFIX"/lib/ss_predefine/"
#define PREDEF_CALL			"call"
#define PREDEF_FACTORY_MODE		"factorymode"

#define CALL_EXEC_PATH			PREFIX"/bin/call"

#define LOWBAT_EXEC_PATH		PREFIX"/bin/lowbatt-popup"

#define HDMI_NOTI_EXEC_PATH		PREFIX"/bin/hdmi_connection_noti"


static int bFactoryMode = 0;

int predefine_get_pid(const char *execpath)
{
	DIR *dp;
	struct dirent *dentry;
	int pid = -1, fd;
	int ret;
	char buf[PATH_MAX];
	char buf2[PATH_MAX];

	dp = opendir("/proc");
	if (!dp) {
		_E("FAIL: open /proc");
		return -1;
	}

	while ((dentry = readdir(dp)) != NULL) {
		if (!isdigit(dentry->d_name[0]))
			continue;

		pid = atoi(dentry->d_name);

		snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;
		ret = read(fd, buf2, PATH_MAX);
		close(fd);

		if (ret < 0 || ret >=PATH_MAX)
			continue;

		buf2[ret] = '\0';

		if (!strcmp(buf2, execpath)) {
			closedir(dp);
			return pid;
		}
	}

	errno = ESRCH;
	closedir(dp);
	return -1;
}

#ifdef NOUSE
int call_predefine_action(int argc, char **argv)
{
	char argstr[128];
	int pid;

	if (argc < 2)
		return -1;

	snprintf(argstr, sizeof(argstr), "-t MT -n %s -i %s", argv[0], argv[1]);
	pid = launch_if_noexist(CALL_EXEC_PATH, argstr);
	if (pid < 0) {
		_E("call predefine action failed");
		return -1;
	}
	return pid;
}
#endif

void predefine_pm_change_state(unsigned int s_bits)
{
	if (is_factory_mode() == 1)
		_D("skip LCD control for factory mode");
	else
		pm_change_internal(getpid(), s_bits);
}

int is_factory_mode(void)
{
	return bFactoryMode;
}
int set_factory_mode(int bOn)
{
	int ret = -1;
	if ( bOn==1 || bOn==0 ) {
		bFactoryMode = bOn;
		/* For USB-server to refer the value */
		ret = vconf_set_int(VCONFKEY_SYSMAN_FACTORY_MODE, bOn);
		if(ret != 0) {
			_E("FAIL: vconf_set_int()");
		}
	}
	return bFactoryMode;
}

int factory_mode_action(int argc, char **argv)
{
	int bOn;
	if (argc != 1 || argv[0] == NULL) {
		_E("Factory Mode Set predefine action failed");
		return -1;
	}
	bOn = atoi(argv[0]);
	bOn = set_factory_mode(bOn);
	return 0;

}

static void action_entry_load_from_sodir()
{
	DIR *dp;
	struct dirent *dentry;
	struct sysnoti *msg;
	char *ext;
	char tmp[128];

	dp = opendir(PREDEFINE_SO_DIR);
	if (!dp) {
		_E("fail open %s", PREDEFINE_SO_DIR);
		return;
	}

	msg = malloc(sizeof(struct sysnoti));
	if (msg == NULL) {
		_E("Malloc failed");
		closedir(dp);
		return;
	}

	msg->pid = getpid();

	while ((dentry = readdir(dp)) != NULL) {
		if ((ext = strstr(dentry->d_name, ".so")) == NULL)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", PREDEFINE_SO_DIR,
			 dentry->d_name);
		msg->path = tmp;
		*ext = 0;
		msg->type = &(dentry->d_name[3]);
		register_msg(msg);
	}
	free(msg);

	closedir(dp);
}

static DBusMessage *dbus_factory_mode(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
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

	ret = set_factory_mode(atoi(argv));
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_FACTORY_MODE, "sis", "i", dbus_factory_mode },
};

static void predefine_init(void *data)
{
	/* telephony initialize */
	int ret;

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
#ifdef NOUSE
	register_action(PREDEF_CALL, call_predefine_action, NULL, NULL);
#endif
	register_action(PREDEF_FACTORY_MODE, factory_mode_action, NULL, NULL);

	action_entry_load_from_sodir();
}

static const struct device_ops predefine_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "predefine",
	.init     = predefine_init,
};

DEVICE_OPS_REGISTER(&predefine_device_ops)
