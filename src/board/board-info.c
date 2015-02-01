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


#include <fcntl.h>

#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/common.h"

#define FILE_BUFF_MAX	1024
#define NOT_INITIALIZED	(-1)

#define METHOD_GET_NUM	"GetNum"
#define METHOD_GET_SERIAL	"GetSerial"
#define SERIAL_PATH_NAME	"/csa/imei/serialno.dat"

#define METHOD_GET_REVISION	"GetHWRev"
#define PATH_NAME		"/proc/cpuinfo"
#define REVISION_NAME	"Revision"
#define SERIAL_NAME		"Serial"
#define SERIAL_TOK_DELIMITER ","
#define TOK_DELIMITER	":"
#define END_DELIMITER	" \n"

#define CONVERT_TYPE	10
#define REVISION_SIZE	4

struct serial_info {
	char serial[FILE_BUFF_MAX];
	char num[FILE_BUFF_MAX];
};

static struct serial_info info;

static int read_from_file(const char *path, char *buf, size_t size)
{
	int fd;
	size_t count;

	if (!path)
		return -1;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		_E("Could not open '%s'", path);
		return -1;
	}

	count = read(fd, buf, size);

	if (count > 0) {
		count = (count < size) ? count : size - 1;
		while (count > 0 && buf[count - 1] == '\n')
			count--;
		buf[count] = '\0';
	} else {
		buf[0] = '\0';
	}

	close(fd);

	return 0;
}

static int get_revision(char *rev)
{
	char buf[FILE_BUFF_MAX];
	char *tag;
	char *start, *ptr;
	long rev_num;
	const int radix = 16;

	if (rev == NULL) {
		_E("Invalid argument !\n");
		return -1;
	}

	if (read_from_file(PATH_NAME, buf, FILE_BUFF_MAX) < 0) {
		_E("fail to read %s\n", PATH_NAME);
		return -1;
	}

	tag = strstr(buf, REVISION_NAME);
	if (tag == NULL) {
		_E("cannot find Hardware in %s\n", PATH_NAME);
		return -1;
	}

	start = strstr(tag, TOK_DELIMITER);
	if (start == NULL) {
		_E("cannot find Hardware in %s\n", PATH_NAME);
		return -1;
	}

	start ++;
	ptr = strtok(start, END_DELIMITER);
	ptr += strlen(ptr);
	ptr -=2;

	memset(rev, 0x00, REVISION_SIZE);
	rev_num = strtol(ptr, NULL, radix);
	sprintf(rev, "%d", rev_num);

	return 0;
}

static int get_cpuinfo_serial(void)
{
	char buf[FILE_BUFF_MAX];
	char *tag;
	char *start, *ptr;

	if (read_from_file(PATH_NAME, buf, FILE_BUFF_MAX) < 0) {
		_E("fail to read %s\n", PATH_NAME);
		return -1;
	}

	tag = strstr(buf, SERIAL_NAME);
	if (tag == NULL) {
		_E("cannot find Hardware in %s\n", PATH_NAME);
		return -1;
	}

	start = strstr(tag, TOK_DELIMITER);
	if (start == NULL) {
		_E("cannot find Hardware in %s\n", PATH_NAME);
		return -1;
	}

	start ++;
	ptr = strtok(start, END_DELIMITER);
	strncpy(info.serial, ptr, strlen(ptr));
	_D("%s", info.serial);
	return 0;
}

static DBusMessage *dbus_revision_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char rev[FILE_BUFF_MAX];
	char *ptr;
	static int ret = NOT_INITIALIZED;

	if (ret != NOT_INITIALIZED)
		goto out;
	ret = get_revision(rev);
	if (ret == 0)
		ret = strtol(rev, &ptr, CONVERT_TYPE);
out:
	_D("rev : %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static int get_serial(void)
{
	static int ret = -EIO;
	int fd;
	int r;
	char *tag;
	char *serial;
	char buf[FILE_BUFF_MAX];

	if (ret == 0)
		return ret;

	fd = open(SERIAL_PATH_NAME, O_RDONLY, 0);
	if (fd < 0)
		return -ENOENT;

	r = read(fd, buf, FILE_BUFF_MAX);
	if (r < 0 || r >= FILE_BUFF_MAX)
		goto out;
	buf[r] = '\0';
	tag = buf;
	serial = strstr(buf, SERIAL_TOK_DELIMITER);
	if (serial) {
		serial = strtok(tag, SERIAL_TOK_DELIMITER);
		if (!serial)
			goto out;
		r = strlen(serial);
		strncpy(info.serial, serial, r);
	} else {
		strncpy(info.serial, buf, r);
	}
	_D("%s %d", info.serial, r);
	ret = 0;
out:
	close(fd);
	return ret;
}

static int get_num(void)
{
	static int ret = -EIO;
	int fd;
	int r;
	char *tag;
	char *num;
	char buf[FILE_BUFF_MAX];

	if (ret == 0)
		return ret;

	fd = open(SERIAL_PATH_NAME, O_RDONLY, 0);
	if (fd < 0)
		return -ENOENT;

	r = read(fd, buf, FILE_BUFF_MAX);
	if (r < 0 || r >= FILE_BUFF_MAX)
		goto out;
	buf[r] = '\0';
	num = strstr(buf, SERIAL_TOK_DELIMITER);
	if (!num)
		goto out;
	tag = buf;
	strtok(tag, SERIAL_TOK_DELIMITER);
	strtok(NULL, SERIAL_TOK_DELIMITER);
	num = strtok(NULL, END_DELIMITER);
	if (!num)
		goto out;
	r = strlen(num);
	strncpy(info.num, num, r);
	_D("%s %d", info.num, r);
	ret = 0;
out:
	close(fd);
	return ret;
}

static DBusMessage *dbus_serial_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter, arr;
	DBusMessage *reply;
	static int len = 0;
	static int ret = NOT_INITIALIZED;
	char buf[FILE_BUFF_MAX];
	char *param = buf;

	ret = get_serial();
	if (ret < 0)
		ret = get_cpuinfo_serial();
	if (ret == 0) {
		len = strlen(info.serial);
		strncpy(buf, info.serial, len);
		_D("%s %d", buf, len);
		goto out;
	}
	_E("fail to get serial");
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &param);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_num_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter, arr;
	DBusMessage *reply;
	static int len = 0;
	static char buf[FILE_BUFF_MAX];
	static char *param = buf;
	static int ret = NOT_INITIALIZED;

	ret = get_num();
	if (ret == 0) {
		len = strlen(info.num);
		strncpy(buf, info.num, len);
		_D("%s %d", buf, len);
		goto out;
	}
	_E("fail to get num");
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &param);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_GET_SERIAL,   NULL, "si", dbus_serial_handler },
	{ METHOD_GET_REVISION,   NULL, "i", dbus_revision_handler },
	{ METHOD_GET_NUM,   NULL, "si", dbus_num_handler },
};

static void board_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_BOARD, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
	ret = get_serial();
	if (ret < 0)
		_E("fail to get serial info(%d)", ret);
	ret = get_num();
	if (ret < 0)
		_E("fail to get num info(%d)", ret);
}

static const struct device_ops board_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "board",
	.init     = board_init,
};

DEVICE_OPS_REGISTER(&board_device_ops)
