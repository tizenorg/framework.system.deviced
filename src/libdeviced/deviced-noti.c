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


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <limits.h>

#include "dd-deviced.h"
#include "deviced-priv.h"
#include "log.h"
#include "dbus.h"

#define PREDEF_PWROFF_POPUP			"pwroff-popup"
#define PREDEF_ENTERSLEEP			"entersleep"
#define PREDEF_LEAVESLEEP			"leavesleep"
#define PREDEF_REBOOT				"reboot"
#define PREDEF_BACKGRD				"backgrd"
#define PREDEF_FOREGRD				"foregrd"
#define PREDEF_ACTIVE				"active"
#define PREDEF_INACTIVE				"inactive"

#define PREDEF_SET_MAX_FREQUENCY		"set_max_frequency"
#define PREDEF_SET_MIN_FREQUENCY		"set_min_frequency"
#define PREDEF_RELEASE_MAX_FREQUENCY		"release_max_frequency"
#define PREDEF_RELEASE_MIN_FREQUENCY		"release_min_frequency"

#define PREDEF_FACTORY_MODE			"factorymode"

#define PREDEF_DUMP_LOG			"dump_log"
#define PREDEF_DELETE_DUMP		"delete_dump"
#define FLIGHT_MODE		"flightmode"

enum deviced_noti_cmd {
	ADD_deviced_ACTION,
	CALL_deviced_ACTION
};

#define SYSTEM_NOTI_SOCKET_PATH "/tmp/sn"
#define RETRY_READ_COUNT	10

static inline int send_int(int fd, int val)
{
	return write(fd, &val, sizeof(int));
}

static inline int send_str(int fd, char *str)
{
	int len;
	int ret;
	if (str == NULL) {
		len = 0;
		ret = write(fd, &len, sizeof(int));
	} else {
		len = strlen(str);
		if (len > SYSTEM_NOTI_MAXSTR)
			len = SYSTEM_NOTI_MAXSTR;
		write(fd, &len, sizeof(int));
		ret = write(fd, str, len);
	}
	return ret;
}

static int noti_send(struct sysnoti *msg)
{
	int client_len;
	int client_sockfd;
	int result;
	int r;
	int retry_count = 0;
	struct sockaddr_un clientaddr;
	int i;

	client_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client_sockfd == -1) {
		_E("socket create failed");
		return -1;
	}
	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	strncpy(clientaddr.sun_path, SYSTEM_NOTI_SOCKET_PATH, sizeof(clientaddr.sun_path) - 1);
	client_len = sizeof(clientaddr);

	if (connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len) <
	    0) {
		_E("connect failed");
		close(client_sockfd);
		return -1;
	}

	send_int(client_sockfd, msg->pid);
	send_int(client_sockfd, msg->cmd);
	send_str(client_sockfd, msg->type);
	send_str(client_sockfd, msg->path);
	send_int(client_sockfd, msg->argc);
	for (i = 0; i < msg->argc; i++)
		send_str(client_sockfd, msg->argv[i]);

	while (retry_count < RETRY_READ_COUNT) {
		r = read(client_sockfd, &result, sizeof(int));
		if (r < 0) {
			if (errno == EINTR) {
				_E("Re-read for error(EINTR)");
				retry_count++;
				continue;
			}
			_E("Read fail for str length");
			result = -1;
			break;

		}
		break;
	}
	if (retry_count == RETRY_READ_COUNT)
		_E("Read retry failed");

	close(client_sockfd);
	return result;
}

static int dbus_flightmode_handler(char *type, char *buf)
{
	char *pa[3];
	int ret;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_POWER, DEVICED_INTERFACE_POWER,
			pa[0], "sis", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_POWER, pa[0], ret);
	return ret;
}

API int deviced_change_flightmode(int mode)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", mode);
	return dbus_flightmode_handler(FLIGHT_MODE, buf);
}

API int deviced_call_predef_action(const char *type, int num, ...)
{
	struct sysnoti *msg;
	int ret;
	va_list argptr;
	int i;
	char *args = NULL;

	if (type == NULL || num > SYSTEM_NOTI_MAXARG) {
		errno = EINVAL;
		return -1;
	}

	msg = malloc(sizeof(struct sysnoti));

	if (msg == NULL) {
		/* Do something for not enought memory error */
		return -1;
	}

	msg->pid = getpid();
	msg->cmd = CALL_deviced_ACTION;
	msg->type = (char *)type;
	msg->path = NULL;

	msg->argc = num;
	va_start(argptr, num);
	for (i = 0; i < num; i++) {
		args = va_arg(argptr, char *);
		msg->argv[i] = args;
	}
	va_end(argptr);

	ret = noti_send(msg);
	free(msg);

	return ret;
}

static int dbus_proc_handler(char *type, char *buf)
{
	char *pa[3];
	int ret;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			pa[0], "sis", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_PROCESS, pa[0], ret);
	return ret;
}

API int deviced_inform_foregrd(void)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", getpid());
	return dbus_proc_handler(PREDEF_FOREGRD, buf);
}

API int deviced_inform_backgrd(void)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", getpid());
	return dbus_proc_handler(PREDEF_BACKGRD, buf);
}

API int deviced_inform_active(pid_t pid)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", pid);
	return dbus_proc_handler(PREDEF_ACTIVE, buf);
}

API int deviced_inform_inactive(pid_t pid)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", pid);
	return dbus_proc_handler(PREDEF_INACTIVE, buf);
}

static int dbus_power_handler(char *type)
{
	char *pa[2];
	int ret;

	pa[0] = type;
	pa[1] = "0";

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_POWER, DEVICED_INTERFACE_POWER,
			pa[0], "si", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_POWER, pa[0], ret);
	return ret;
}

API int deviced_request_poweroff(void)
{
	return dbus_power_handler(PREDEF_PWROFF_POPUP);
}

API int deviced_request_entersleep(void)
{
	return dbus_power_handler(PREDEF_ENTERSLEEP);
}

API int deviced_request_leavesleep(void)
{
	return dbus_power_handler(PREDEF_LEAVESLEEP);
}

API int deviced_request_reboot(void)
{
	return dbus_power_handler(PREDEF_REBOOT);
}

static int dbus_cpu_handler(char *type, char *buf_pid, char *buf_freq)
{
	char *pa[4];
	int ret;

	pa[0] = type;
	pa[1] = "2";
	pa[2] = buf_pid;
	pa[3] = buf_freq;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			pa[0], "siss", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_SYSNOTI, pa[0], ret);
	return ret;
}

API int deviced_request_set_cpu_max_frequency(int val)
{
	char buf_pid[8];
	char buf_freq[256];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_freq, sizeof(buf_freq), "%d", val * 1000);

	return dbus_cpu_handler(PREDEF_SET_MAX_FREQUENCY, buf_pid, buf_freq);
}

API int deviced_request_set_cpu_min_frequency(int val)
{
	char buf_pid[8];
	char buf_freq[256];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_freq, sizeof(buf_freq), "%d", val * 1000);

	return dbus_cpu_handler(PREDEF_SET_MIN_FREQUENCY, buf_pid, buf_freq);
}

API int deviced_release_cpu_max_frequency()
{
	char buf_pid[8];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());

	return dbus_cpu_handler(PREDEF_RELEASE_MAX_FREQUENCY, buf_pid, "2");
}

API int deviced_release_cpu_min_frequency()
{
	char buf_pid[8];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());

	return dbus_cpu_handler(PREDEF_RELEASE_MIN_FREQUENCY, buf_pid, "2");
}

static int dbus_factory_handler(char *type, char *buf)
{
	char *pa[3];
	int ret;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			pa[0], "sis", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_SYSNOTI, pa[0], ret);
	return ret;
}

API int deviced_request_set_factory_mode(int val)
{
	char buf_mode[8];
	if (val == 0 || val == 1) {
		snprintf(buf_mode, sizeof(buf_mode), "%d", val);
		return dbus_factory_handler(PREDEF_FACTORY_MODE, buf_mode);
	} else {
		return -1;
	}
}

static int dbus_crash_handler(char *type, char *buf)
{
	char *pa[3];
	int ret;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;
	ret = dbus_method_sync(CRASHD_BUS_NAME,
			CRASHD_PATH_CRASH, CRASHD_INTERFACE_CRASH,
			pa[0], "sis", pa);

	_D("%s-%s : %d", CRASHD_INTERFACE_CRASH, pa[0], ret);
	return ret;
}

API int deviced_request_dump_log(int type)
{
	char buf_mode[8];
	if (type == AP_DUMP || type == CP_DUMP || type == ALL_DUMP) {
		snprintf(buf_mode, sizeof(buf_mode), "%d", type);
		return dbus_crash_handler(PREDEF_DUMP_LOG, buf_mode);
	} else {
		return -1;
	}
}

API int deviced_request_delete_dump(char *ticket)
{
	char buf[255];
	if (ticket == NULL)
		return -1;
	snprintf(buf, sizeof(buf), "%s", ticket);
	return dbus_crash_handler(PREDEF_DELETE_DUMP, buf);
}
