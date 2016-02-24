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
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <vconf.h>
#include <device-node.h>

#include "core/log.h"
#include "core/common.h"

#define SAMPLE_INTERVAL	50
#define ITERATION_INFINITE	255

#define PTHREAD_SELF	(pthread_self())

enum led_brt {
	LED_OFF = 0,
	LED_ON,
};

static struct led_pattern {
	char *buffer;
	int len;
	int cnt;
	int index;
} pattern;

static pthread_t tid;

static int led_set_brt(enum led_brt brt)
{
	int ret, val;

	ret = device_get_property(DEVICE_TYPE_LED, PROP_LED_MAX_BRIGHTNESS, &val);
	if (ret < 0)
		return ret;

	if (brt != LED_OFF)
		brt = val;

	ret = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, brt);
	if (ret < 0)
		return ret;

	return 0;
}

static void clean_up(void *arg)
{
	struct led_pattern *pt = (struct led_pattern *)arg;
	int torch_status = 0;

	_D("[%u] Clean up thread", PTHREAD_SELF);

	/* Release memory after use */
	if (pt->buffer) {
		free(pt->buffer);
		pt->buffer = NULL;
	}

	/* Get assistive light status */
	if (vconf_get_bool(VCONFKEY_SETAPPL_ACCESSIBILITY_TORCH_LIGHT, &torch_status) < 0)
		_E("[%u] Failed to get VCONFKEY_SETAPPL_ACCESSIBILITY_TORCH_LIGHT value", PTHREAD_SELF);

	/* Restore assistive light */
	if (torch_status) {
		_D("[%u] assistive light turns on", PTHREAD_SELF);
		led_set_brt(LED_ON);
	} else	/* For abnormal termination */
		led_set_brt(LED_OFF);

	tid = 0;
}

static void *play_cb(void *arg)
{
	struct led_pattern *pt = (struct led_pattern *)arg;
	int val;
	struct timespec time = {0,};

	_D("[%u] Start thread", PTHREAD_SELF);
	pthread_cleanup_push(clean_up, arg);

	if (!pt->buffer)
		goto error;

	_D("[%u] index(%d), length(%d), repeat count(%d)", PTHREAD_SELF, pt->index, pt->len, pt->cnt);

	/* Play LED accroding to buffer */
	led_set_brt(LED_OFF);
	while (pt->cnt) {
		/* do not reset index to use exisiting value */
		for (; pt->index < pt->len; pt->index++) {
			val = (int)(pt->buffer[pt->index] - '0');
			/* Verify buffer data */
			if (val & 0xFE)
				break;

			led_set_brt(val);

			/* Sleep time 50*999us = 49.950ms (Vibration unit 50ms) */
			usleep(SAMPLE_INTERVAL*999);
		}

		/* reset index */
		pt->index = 0;

		/* Decrease count when play with limit */
		if (pt->cnt != ITERATION_INFINITE)
			pt->cnt--;
	}

	led_set_brt(LED_OFF);

	/* Sleep 500ms before turning on assistive light */
	time.tv_nsec = 500 * NANO_SECOND_MULTIPLIER;
	nanosleep(&time, NULL);

error:
	pthread_cleanup_pop(1);

	_D("[%u] End thread", PTHREAD_SELF);
	pthread_exit((void *)0);
}

static int create_thread(struct led_pattern *pattern)
{
	int ret;

	if (tid) {
		_E("pthread already created");
		return -EEXIST;
	}

	ret = pthread_create(&tid, NULL, play_cb, pattern);
	if (ret != 0) {
		_E("fail to create thread : %d", errno);
		return -errno;
	}

	return 0;
}

static int cancel_thread(void)
{
	int ret;
	void *retval;

	if (!tid) {
		_D("pthread not initialized");
		return 0;
	}

	ret = pthread_cancel(tid);
	if (ret != 0) {
		_E("fail to cancel thread(%d) : %d", tid, errno);
		return -errno;
	}

	ret = pthread_join(tid, &retval);
	if (ret != 0) {
		_E("fail to join thread(%d) : %d", tid, errno);
		return -errno;
	}

	if (retval == PTHREAD_CANCELED)
		_D("pthread canceled");
	else
		_D("pthread already finished");

	return 0;
}

static int detach_thread(void)
{
	int ret;

	if (!tid) {
		_D("pthread not initialized");
		return 0;
	}

	ret = pthread_detach(tid);
	if (ret != 0) {
		_E("fail to detach thread(%d) : %d", tid, errno);
		return ret;
	}

	tid = 0;
	return 0;
}

static int load_file(const char *path, char **buffer, int *len)
{
	FILE *fp;
	char *buf = NULL;
	int l;

	if (!path || !buffer || !len) {
		_E("invalid argument");
		return -EINVAL;
	}

	/* Open file */
	fp = fopen(path, "rb");
	if (!fp) {
		_E("fail to open file(%s) : %d", path, errno);
		return -errno;
	}

	/* Get file length */
	fseek(fp, 0, SEEK_END);
	l = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (l < 0)
		goto error;

	buf = malloc(l*sizeof(char));
	if (!buf) {
		_E("fail to allocate memory : %d", errno);
		goto error;
	}

	/* Read file contents into buffer */
	if (fread(buf, 1, l, fp) < l) {
		_E("fail to read file data : %d", errno);
		goto error;
	}

	/* Close file */
	fclose(fp);

	*buffer = buf;
	*len = l;
	return 0;

error:
	/* Close file */
	fclose(fp);

	/* Free unnecessary memory */
	free(buf);
	return -EPERM;
}

int play_pattern(const char *path, int cnt)
{
	int ret;

	/* Check if thread already started */
	ret = cancel_thread();
	if (ret < 0) {
		_E("fail to cancel thread");
		return ret;
	}

	/* Load led file to buffer */
	ret = load_file(path, &(pattern.buffer), &(pattern.len));
	if (ret < 0) {
		_E("fail to load file(%s)", path);
		return ret;
	}

	/* Set the led data */
	pattern.cnt = cnt;
	pattern.index = 0;

	/* Create and Execute thread */
	ret = create_thread(&pattern);
	if (ret < 0) {
		_E("fail to create thread");
		return ret;
	}

	return 0;
}

int stop_pattern(void)
{
	int ret;

	ret = cancel_thread();
	if (ret < 0) {
		_E("fail to cancel thread");
		return ret;
	}

	return 0;
}
