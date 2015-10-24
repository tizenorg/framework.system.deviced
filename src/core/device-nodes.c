/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <dirent.h>
#include <fcntl.h>

#include "log.h"
#include "common.h"
#include "device-nodes.h"

#define LCD_PATH		"/sys/class/lcd/"
#define BACKLIGHT_PATH		"/sys/class/backlight/"
#define DEVICE_PATH		"/device"
#define ALPM_PATH		"/alpm"
#define HBM_PATH		"/hbm"
#define MBM_PATH		"/mbm"

#define INPUT_PATH		"/sys/class/input/"
#define KEY_CAPABILITIES_PATH	"/device/capabilities/key"
#define ENABLED_PATH		"/device/enabled"
#define TOUCHKEY_CAPABILITY	200
#define TOUCHSCREEN_CAPABILITY	400
#define ROTARY_CAPABILITY	10000

static char *nodes[DEVICE_TYPE_MAX];

static char *find_device_input_node(int type)
{
	DIR *d;
	struct dirent entry;
	struct dirent *dir;
	char buf[PATH_MAX];
	char *result = NULL;
	int ret, capa, val;

	if (type == DEVICE_TOUCHKEY)
		capa = TOUCHKEY_CAPABILITY;
	else if (type == DEVICE_TOUCHSCREEN)
		capa = TOUCHSCREEN_CAPABILITY;
	else if (type == DEVICE_ROTARY)
		capa = ROTARY_CAPABILITY;
	else
		return NULL;

	d = opendir(INPUT_PATH);
	if (!d)
		return NULL;

	while (readdir_r(d, &entry, &dir) == 0 && dir != NULL) {
		if (dir->d_name[0] == '.')
			continue;

		snprintf(buf, sizeof(buf), "%s%s%s", INPUT_PATH,
		    dir->d_name, KEY_CAPABILITIES_PATH);

		ret = sys_get_int(buf, &val);
		if (ret < 0)
			continue;

		if (val == capa) {
			snprintf(buf, sizeof(buf), "%s%s%s", INPUT_PATH,
			    dir->d_name, ENABLED_PATH);
			result = strndup(buf, strlen(buf));
			_I("%s %d", buf, val);
			break;
		}
	}
	closedir(d);

	return result;
}

static char *check_and_copy_path(char *path)
{
	int fd;
	char *buf = NULL;

	fd = open(path, O_RDONLY);
	if (fd >= 0) {
		_I("%s", path);
		buf = strndup(path, strlen(path));
		close(fd);
	}

	return buf;
}

static char *find_device_lcd_node(int type)
{
	DIR *d;
	struct dirent entry;
	struct dirent *dir;
	char buf[PATH_MAX];
	char *path, *result = NULL;
	int fd;

	if (type == DEVICE_ALPM)
		path = ALPM_PATH;
	else if (type == DEVICE_HBM)
		path = HBM_PATH;
	else if (type == DEVICE_MBM)
		path = MBM_PATH;
	else
		return NULL;

	d = opendir(LCD_PATH);
	if (!d)
		return NULL;

	while (readdir_r(d, &entry, &dir) == 0 && dir != NULL) {
		if (dir->d_name[0] == '.')
			continue;

		snprintf(buf, sizeof(buf), "%s%s%s", LCD_PATH,
		    dir->d_name, path);

		result = check_and_copy_path(buf);
		if (result)
			break;

		snprintf(buf, sizeof(buf), "%s%s%s%s", LCD_PATH,
		    dir->d_name, DEVICE_PATH, path);

		result = check_and_copy_path(buf);
		if (result)
			break;
	}
	closedir(d);

	return result;
}

static char *find_device_backlight_node(int type)
{
	DIR *d;
	struct dirent entry;
	struct dirent *dir;
	char buf[PATH_MAX];
	char *path, *result = NULL;
	int fd;

	if (type == DEVICE_HBM)
		path = HBM_PATH;
	else if (type == DEVICE_MBM)
		path = MBM_PATH;
	else
		return NULL;

	d = opendir(BACKLIGHT_PATH);
	if (!d)
		return NULL;

	while (readdir_r(d, &entry, &dir) == 0 && dir != NULL) {
		if (dir->d_name[0] == '.')
			continue;

		snprintf(buf, sizeof(buf), "%s%s%s", BACKLIGHT_PATH,
		    dir->d_name, path);

		result = check_and_copy_path(buf);
		if (result)
			break;

		snprintf(buf, sizeof(buf), "%s%s%s%s", BACKLIGHT_PATH,
		    dir->d_name, DEVICE_PATH, path);

		result = check_and_copy_path(buf);
		if (result)
			break;
	}
	closedir(d);

	return result;
}

char *find_device_node(int type)
{
	if (type < DEVICE_TYPE_MIN || type >= DEVICE_TYPE_MAX)
		return NULL;

	if (nodes[type])
		return nodes[type];

	switch (type) {
	case DEVICE_ROTARY:
	case DEVICE_TOUCHKEY:
	case DEVICE_TOUCHSCREEN:
		nodes[type] = find_device_input_node(type);
		break;
	case DEVICE_ALPM:
		nodes[type] = find_device_lcd_node(type);
		break;
	case DEVICE_HBM:
	case DEVICE_MBM:
		nodes[type] = find_device_lcd_node(type);
		if (!nodes[type])
			nodes[type] = find_device_backlight_node(type);
		break;
	default:
		_I("No find-logic for type %d", type);
	}

	return nodes[type];
}

