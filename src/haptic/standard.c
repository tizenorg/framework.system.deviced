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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <Ecore.h>

#include "core/log.h"
#include "haptic.h"

#define DEFAULT_HAPTIC_HANDLE	0xFFFF
#define MAX_MAGNITUDE			0xFFFF
#define PERIODIC_MAX_MAGNITUDE	0x7FFF	/* 0.5 * MAX_MAGNITUDE */

#define DEV_INPUT   "/dev/input"
#define EVENT		"event"

#define BITS_PER_LONG (sizeof(long) * 8)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

static struct ff_effect ff_effect;
static char ff_path[PATH_MAX];
static int ff_use_count;
static int ff_fd;
static Ecore_Timer *timer;

static int ff_stop(int fd);
static Eina_Bool timer_cb(void *data)
{
	/* stop previous vibration */
	ff_stop(ff_fd);
	_I("stop vibration by timer");

	return ECORE_CALLBACK_CANCEL;
}

static int register_timer(int ms)
{
	/* add new timer */
	timer = ecore_timer_add(ms/1000.f, timer_cb, NULL);
	if (!timer)
		return -EPERM;

	return 0;
}

static int unregister_timer(void)
{
	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}

	return 0;
}

static int ff_find_device(char *path, int size)
{
	DIR *dir;
	struct dirent *dent;
	char ev_path[PATH_MAX];
	unsigned long features[1+FF_MAX/sizeof(unsigned long)];
	int fd, ret;

	dir = opendir(DEV_INPUT);
	if (!dir)
		return -errno;

	while ((dent = readdir(dir))) {
		if (dent->d_type == DT_DIR ||
			!strstr(dent->d_name, "event"))
			continue;

		snprintf(ev_path, sizeof(ev_path), "%s/%s", DEV_INPUT, dent->d_name);

		fd = open(ev_path, O_RDWR);
		if (fd < 0)
			continue;

		/* get force feedback device */
		memset(features, 0, sizeof(features));
		ret = ioctl(fd, EVIOCGBIT(EV_FF, sizeof(features)), features);
		if (ret == -1) {
			close(fd);
			continue;
		}

		if (test_bit(FF_CONSTANT, features))
			_D("%s type : constant", ev_path);
		if (test_bit(FF_PERIODIC, features))
			_D("%s type : periodic", ev_path);
		if (test_bit(FF_SPRING, features))
			_D("%s type : spring", ev_path);
		if (test_bit(FF_FRICTION, features))
			_D("%s type : friction", ev_path);
		if (test_bit(FF_RUMBLE, features))
			_D("%s type : rumble", ev_path);

		if (test_bit(FF_PERIODIC, features)) {
			memcpy(ff_path, ev_path, strlen(ev_path));
			close(fd);
			closedir(dir);
			return 0;
		}

		close(fd);
	}

	closedir(dir);
	return -1;
}

static int ff_play(int fd, int length, int level)
{
	struct input_event play;
	int r = 0;
	double magnitude;

	if (fd < 0)
		return -EINVAL;

	/* unregister existing timer */
	unregister_timer();

	magnitude = (double)level/HAPTIC_MODULE_FEEDBACK_MAX;
	magnitude *= PERIODIC_MAX_MAGNITUDE;

	_I("info : magnitude(%d) length(%d)", (int)magnitude, length);

	/* Set member variables in effect struct */
	ff_effect.type = FF_PERIODIC;
	if (!ff_effect.id)
		ff_effect.id = -1;
	ff_effect.u.periodic.waveform = FF_SQUARE;
	ff_effect.u.periodic.period = 0.1*0x100;	/* 0.1 second */
	ff_effect.u.periodic.magnitude = (int)magnitude;
	ff_effect.u.periodic.offset = 0;
	ff_effect.u.periodic.phase = 0;
	ff_effect.direction = 0x4000;	/* Along X axis */
	ff_effect.u.periodic.envelope.attack_length = 0;
	ff_effect.u.periodic.envelope.attack_level = 0;
	ff_effect.u.periodic.envelope.fade_length = 0;
	ff_effect.u.periodic.envelope.fade_level = 0;
	ff_effect.trigger.button = 0;
	ff_effect.trigger.interval = 0;
	ff_effect.replay.length = length;
	ff_effect.replay.delay = 0;

	if (ioctl(fd, EVIOCSFF, &ff_effect) == -1) {
		/* workaround: if effect is erased, try one more */
		ff_effect.id = -1;
		if (ioctl(fd, EVIOCSFF, &ff_effect) == -1)
			return -errno;
	}

	/* Play vibration*/
	play.type = EV_FF;
	play.code = ff_effect.id;
	play.value = 1; /* 1 : PLAY, 0 : STOP */

	if (write(fd, (const void*)&play, sizeof(play)) == -1)
		return -errno;

	/* register timer */
	register_timer(length);

	return 0;
}

static int ff_stop(int fd)
{
	struct input_event stop;
	int r = 0;

	if (fd < 0)
		return -EINVAL;

	/* Stop vibration */
	stop.type = EV_FF;
	stop.code = ff_effect.id;
	stop.value = 0;

	if (write(fd, (const void*)&stop, sizeof(stop)) == -1)
		return -errno;

	return 0;
}

/* START: Haptic Module APIs */
static int get_device_count(int *count)
{
	if (count)
		*count = 1;

	return 0;
}

static int open_device(int device_index, int *device_handle)
{
	/* open ff driver */
	if (ff_use_count == 0) {
		ff_fd = open(ff_path, O_RDWR);
		if (!ff_fd) {
			_E("Failed to open %s : %s", ff_path, strerror(errno));
			return -errno;
		}
	}

	/* Increase handle count */
	ff_use_count++;

	if (device_handle)
		*device_handle = DEFAULT_HAPTIC_HANDLE;

	return 0;
}

static int close_device(int device_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	if (ff_use_count == 0)
		return -EPERM;

	ff_stop(ff_fd);

	/* Decrease handle count */
	ff_use_count--;

	/* close ff driver */
	if (ff_use_count == 0) {
		/* Initialize effect structure */
		memset(&ff_effect, 0, sizeof(ff_effect));
		close(ff_fd);
		ff_fd = -1;
	}

	return 0;
}

static int vibrate_monotone(int device_handle, int duration, int feedback, int priority, int *effect_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	return ff_play(ff_fd, duration, feedback);
}

static int vibrate_buffer(int device_handle, const unsigned char *vibe_buffer, int iteration, int feedback, int priority, int *effect_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	/* temporary code */
	return ff_play(ff_fd, 300, feedback);
}

static int stop_device(int device_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	return ff_stop(ff_fd);
}

static int get_device_state(int device_index, int *effect_state)
{
	int status;

	if (ff_effect.id != 0)
		status = 1;
	else
		status = 0;

	if (effect_state)
		*effect_state = status;

	return 0;
}

static int create_effect(unsigned char *vibe_buffer, int max_bufsize, haptic_module_effect_element *elem_arr, int max_elemcnt)
{
	_E("Not support feature");
	return -EACCES;
}

static int get_buffer_duration(int device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	_E("Not support feature");
	return -EACCES;
}

static int convert_binary(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	_E("Not support feature");
	return -EACCES;
}
/* END: Haptic Module APIs */

static const struct haptic_plugin_ops default_plugin = {
	.get_device_count    = get_device_count,
	.open_device         = open_device,
	.close_device        = close_device,
	.vibrate_monotone    = vibrate_monotone,
	.vibrate_buffer      = vibrate_buffer,
	.stop_device         = stop_device,
	.get_device_state    = get_device_state,
	.create_effect       = create_effect,
	.get_buffer_duration = get_buffer_duration,
	.convert_binary      = convert_binary,
};

static bool is_valid(void)
{
	int ret;

	ret = ff_find_device(ff_path, sizeof(ff_path));

	if (ret < 0)
		return false;

	return true;
}

static const struct haptic_plugin_ops *load(void)
{
	return &default_plugin;
}

static const struct haptic_ops std_ops = {
	.is_valid = is_valid,
	.load     = load,
};

HAPTIC_OPS_REGISTER(&std_ops)
