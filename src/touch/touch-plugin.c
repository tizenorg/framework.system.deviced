/*
 * Touch - Support plugin feature for Touch
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "touch-plugin.h"

#define TOUCH_BOOST_OFF		"/sys/devices/system/cpu/cpu0/cpufreq/touch_boost_off"

/*
 * Helper function
 * - Read from sysfs entry
 * - Write to sysfs entry
 */
#define BUFF_MAX 255
static int sys_read_buf(char *file, char *buf)
{
	int fd;
	int r;
	int ret = 0;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -ENOENT;

	r = read(fd, buf, BUFF_MAX);
	if ((r >= 0) && (r < BUFF_MAX))
		buf[r] = '\0';
	else
		ret = -EIO;

	close(fd);

	return ret;
}

static int sys_write_buf(char *file, char *buf)
{
	int fd;
	int r;
	int ret = 0;

	fd = open(file, O_WRONLY);
	if (fd == -1)
		return -ENOENT;

	r = write(fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;

	close(fd);

	return ret;
}

static int sys_get_int(char *fname, int *val)
{
	char buf[BUFF_MAX];
	int ret = 0;

	if (sys_read_buf(fname, buf) == 0) {
		*val = atoi(buf);
	} else {
		*val = -1;
		ret = -EIO;
	}

	return ret;
}

static int sys_set_int(char *fname, int val)
{
	char buf[BUFF_MAX];
	int ret = 0;

	snprintf(buf, sizeof(buf), "%d", val);

	if (sys_write_buf(fname, buf) != 0)
		ret = -EIO;

	return ret;
}

/*
 * Get/Set touch_boost_off state
 */
int get_cpufreq_touch_boost_off(int *touch_boost_off)
{
	return sys_get_int(TOUCH_BOOST_OFF, touch_boost_off);
}

int set_cpufreq_touch_boost_off(int touch_boost_off)
{
	return sys_set_int(TOUCH_BOOST_OFF, touch_boost_off);
}
