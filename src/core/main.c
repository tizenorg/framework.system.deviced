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
#include <fcntl.h>
#include <sys/reboot.h>

#include "display/core.h"
#include "log.h"
#include "data.h"
#include "edbus-handler.h"
#include "devices.h"
#include "shared/dbus.h"
#include "device-notifier.h"

#define PIDFILE_PATH		"/var/run/.deviced.pid"

static void init_ad(struct main_data *ad)
{
	memset(ad, 0x0, sizeof(struct main_data));
}

static void writepid(char *pidpath)
{
	FILE *fp;

	fp = fopen(pidpath, "w");
	if (fp != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
}

static void sig_quit(int signo)
{
	_D("received SIGTERM signal %d", signo);
}

static void sig_usr1(int signo)
{
	_D("received SIGUSR1 signal %d, deviced'll be finished!", signo);

	ecore_main_loop_quit();
}

static void check_systemd_active(void)
{
	DBusError err;
	DBusMessage *msg;
	DBusMessageIter iter, sub;
	const char *state;
	char *pa[2];

	pa[0] = "org.freedesktop.systemd1.Unit";
	pa[1] = "ActiveState";

	_I("%s %s", pa[0], pa[1]);

	msg = dbus_method_sync_with_reply("org.freedesktop.systemd1",
			"/org/freedesktop/systemd1/unit/default_2etarget",
			"org.freedesktop.DBus.Properties",
			"Get", "ss", pa);
	if (!msg)
		return;

	dbus_error_init(&err);

	if (!dbus_message_iter_init(msg, &iter) ||
	    dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		goto out;

	dbus_message_iter_recurse(&iter, &sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING)
		goto out;

	dbus_message_iter_get_basic(&sub, &state);

	if (strncmp(state, "active", 6) == 0) {
		_I("notify relaunch");
		device_notify(DEVICE_NOTIFIER_BOOTING_DONE, (void *)TRUE);
	}
out:
	dbus_message_unref(msg);
	dbus_error_free(&err);
}

static int system_main(int argc, char **argv)
{
	struct main_data ad;

	init_ad(&ad);
	edbus_init(&ad);
	devices_init(&ad);
	check_systemd_active();
	signal(SIGTERM, sig_quit);
	signal(SIGUSR1, sig_usr1);

	ecore_main_loop_begin();

	devices_exit(&ad);
	edbus_exit(&ad);
	ecore_shutdown();
	return 0;
}

int main(int argc, char **argv)
{
	writepid(PIDFILE_PATH);
	ecore_init();
	return system_main(argc, argv);
}
