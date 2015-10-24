/*
 * test
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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
#include "test.h"
#include <glib.h>
#include <sys/stat.h>
#include <time.h>

#define HAPTIC_DEVICE			0

#define METHOD_OPEN_DEVICE      "OpenDevice"
#define METHOD_CLOSE_DEVICE     "CloseDevice"
#define METHOD_STOP_DEVICE      "StopDevice"
#define METHOD_VIBRATE_MONOTONE "VibrateMonotone"
#define METHOD_VIBRATE_BUFFER   "VibrateBuffer"
#define METHOD_VIBRATE_BUFFER_L "VibrateBufferL"
#define METHOD_CREATE_EFFECT    "CreateEffect"
#define METHOD_SAVE_BINARY      "SaveBinary"

#define HAPTIC_EFFECT_TEST_NAME "/tmp/haptic_test.ivt"

#define NANO_SECOND_MULTIPLIER  1000000 /* 1ms = 1,000,000nsec */

struct haptic_dbus_byte {
	const unsigned char *data;
	int size;
};

static int haptic_handle;

static void haptic_open(void)
{
	DBusError err;
	DBusMessage *msg;

	char *arr[1];
	char buf_index[32];

	snprintf(buf_index, sizeof(buf_index), "%d", 0);
	arr[0] = buf_index;

	haptic_handle = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC,
			DEVICED_INTERFACE_HAPTIC,
			METHOD_OPEN_DEVICE, "i", arr);
}

static int haptic_vibrate_xml_buffer(unsigned int handle,
				const unsigned char *buffer,
				int size,
				int iteration,
				int feedback,
				int priority)
{
	char *arr[6];
	char buf_handle[32];
	char buf_iteration[32];
	char buf_feedback[32];
	char buf_priority[32];
	char *decode;
	unsigned int len;
	struct haptic_dbus_byte bytes;

	snprintf(buf_handle, sizeof(buf_handle), "%u", handle);
	decode = (char *)g_base64_decode(buffer, &len);

	arr[0] = buf_handle;
	bytes.size = len;
	bytes.data = decode;
	arr[2] = (char *)&bytes;
	snprintf(buf_iteration, sizeof(buf_iteration), "%d", iteration);
	arr[3] = buf_iteration;
	snprintf(buf_feedback, sizeof(buf_feedback), "%d", feedback);
	arr[4] = buf_feedback;
	snprintf(buf_priority, sizeof(buf_priority), "%d", priority);
	arr[5] = buf_priority;

	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_HAPTIC,
			DEVICED_INTERFACE_HAPTIC, METHOD_VIBRATE_BUFFER,
			"uayiii", arr);
}

static int haptic_vibrate_buffer(unsigned int handle,
				const unsigned char *buffer,
				int size,
				int iteration,
				int feedback,
				int priority)
{
	char *arr[6];
	char buf_handle[32];
	char buf_iteration[32];
	char buf_feedback[32];
	char buf_priority[32];
	struct haptic_dbus_byte bytes;

	snprintf(buf_handle, sizeof(buf_handle), "%u", handle);
	arr[0] = buf_handle;
	bytes.size = size;
	bytes.data = buffer;
	arr[2] = (char *)&bytes;
	snprintf(buf_iteration, sizeof(buf_iteration), "%d", iteration);
	arr[3] = buf_iteration;
	snprintf(buf_feedback, sizeof(buf_feedback), "%d", feedback);
	arr[4] = buf_feedback;
	snprintf(buf_priority, sizeof(buf_priority), "%d", priority);
	arr[5] = buf_priority;

	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_HAPTIC,
			DEVICED_INTERFACE_HAPTIC, METHOD_VIBRATE_BUFFER,
			"uayiii", arr);
}

static int haptic_vibrate_buffer_with_level(unsigned int handle,
				const unsigned char *buffer,
				int size,
				int iteration,
				int feedback,
				int priority)
{
	char *arr[6];
	char buf_handle[32];
	char buf_iteration[32];
	char buf_feedback[32];
	char buf_priority[32];
	struct haptic_dbus_byte bytes;

	snprintf(buf_handle, sizeof(buf_handle), "%u", handle);
	arr[0] = buf_handle;
	bytes.size = size;
	bytes.data = buffer;
	arr[2] = (char *)&bytes;
	snprintf(buf_iteration, sizeof(buf_iteration), "%d", iteration);
	arr[3] = buf_iteration;
	snprintf(buf_feedback, sizeof(buf_feedback), "%d", feedback);
	arr[4] = buf_feedback;
	snprintf(buf_priority, sizeof(buf_priority), "%d", priority);
	arr[5] = buf_priority;

	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_HAPTIC,
			DEVICED_INTERFACE_HAPTIC, METHOD_VIBRATE_BUFFER_L,
			"uayiii", arr);
}

static int haptic_vibrate(int duration)
{
	char str_handle[32];
	char str_duration[32];
	char str_feedback[32];
	char str_priority[32];
	char *arr[4];
	int ret, priority;

	if (!haptic_handle)
		return -1;

	if (duration < 0)
		return -2;

	snprintf(str_handle, sizeof(str_handle), "%u", (unsigned int)haptic_handle);
	arr[0] = str_handle;
	snprintf(str_duration, sizeof(str_duration), "%d", duration);
	arr[1] = str_duration;
	snprintf(str_feedback, sizeof(str_feedback), "%d", 60);
	arr[2] = str_feedback;
	snprintf(str_priority, sizeof(str_priority), "%d", 2);
	arr[3] = str_priority;

	/* request to deviced to vibrate haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
	DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
	METHOD_VIBRATE_MONOTONE, "uiii", arr);
	if (ret < 0)
		return ret;

	return 0;
}

struct haptic_effect_element {
	int haptic_duration;
	int haptic_level;
};


static int haptic_create_effect(unsigned char *out,
		int out_size,
		struct haptic_effect_element *elem,
		int elem_size)
{
	DBusError err;
	DBusMessage *msg;
	int i, temp, size, ret , val;
	struct haptic_dbus_byte bytes;
	char *data;
	char str_bufsize[32];
	char str_elemcnt[32];
	char *arr[4];

	/* check if passed arguments are valid */
	if (!out || out_size <= 0 ||
	    !elem || elem_size <= 0)
		return -ENOMEM;

	snprintf(str_bufsize, sizeof(str_bufsize), "%d", out_size);
	arr[0] = str_bufsize;
	bytes.size = sizeof(struct haptic_effect_element)*elem_size;
	bytes.data = (unsigned char *)elem;
	arr[2] = (char *)&bytes;
	snprintf(str_elemcnt, sizeof(str_elemcnt), "%d", elem_size);
	arr[3] = str_elemcnt;

	/* request to deviced to open haptic device */
	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_CREATE_EFFECT, "iayi", arr);
	if (!msg)
		return -EINVAL;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
			DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		goto err;
	}

	if (val < 0) {
		_E("%s-%s failed : %d", DEVICED_INTERFACE_HAPTIC, METHOD_CREATE_EFFECT, val);
		goto err;
	}

	if (out_size < size) {
		_E("max output(%d) is smaller than effect buffer size(%d)", out_size, size);
		goto err;
	}

	memcpy(out, data, out_size);
	dbus_message_unref(msg);
	return 0;
err:
	dbus_message_unref(msg);
	return -EINVAL;
}

static int save_data(const unsigned char *data, int size, const char *file_path)
{
	FILE *file;
	int fd;

	file = fopen(file_path, "wb+");
	if (file == NULL) {
		_E("To open file is failed : %d", errno);
		return -1;
	}

	if (fwrite(data, 1, size, file) != size) {
		_E("To write file is failed : %d", errno);
		fclose(file);
		return -1;
	}

	fd = fileno(file);
	if (fd < 0) {
		_E("To get file descriptor is failed : %d", errno);
		fclose(file);
		return -1;
	}

	if (fsync(fd) < 0) {
		_E("To be synchronized with the disk is failed : %d", errno);
		fclose(file);
		return -1;
	}

	fclose(file);
	return 0;
}

static int haptic_save_effect(const unsigned char *data, int buf_max, const char *path)
{
	struct stat buf;
	int size, ret;

	/* check if passed arguments are valid */
	if (!data || buf_max <= 0 ||
	    !path)
		return -ENOMEM;

	/* check if the file already exists */
	if (!stat(path, &buf))
		remove(path);

	_D("file path : %s", path);
	ret = save_data(data, buf_max, path);
	if (ret < 0) {
		_E("fail to save data");
		return -ENFILE;
	}

	return 0;
}

static unsigned char *convert_file_to_buffer(const char *file_name, int *size)
{
	FILE *pf;
	long file_size;
	unsigned char *pdata = NULL;

	if (!file_name)
		return NULL;

	/* Get File Stream Pointer */
	pf = fopen(file_name, "rb");
	if (!pf) {
		_E("fopen failed : %d", errno);
		return NULL;
	}

	if (fseek(pf, 0, SEEK_END)) {
		_E("fail to seek end");
		goto error;
	}

	file_size = ftell(pf);
	if (fseek(pf, 0, SEEK_SET)) {
		_E("fail to seek set");
		goto error;
	}

	if (file_size < 0) {
		_E("fail to check file size");
		goto error;
	}

	pdata = (unsigned char *)malloc(file_size);
	if (!pdata) {
		_E("fail to alloc");
		goto error;
	}

	if (fread(pdata, 1, file_size, pf) != file_size) {
		_E("fail to read data");
		goto err_free;
	}

	fclose(pf);
	*size = file_size;
	return pdata;

err_free:
	free(pdata);

error:
	fclose(pf);

	_E("failed to convert file to buffer (%d)", errno);
	return NULL;
}

static int haptic_vibrate_file(unsigned int handle, const char *path)
{
	char *buf;
	int size, ret;

	buf = convert_file_to_buffer(path, &size);
	if (!buf) {
		_E("Convert file to buffer error");
		return -EINVAL;
	}
	ret = haptic_vibrate_buffer(handle, buf, size, 1, 60, 1);
	free(buf);
	return ret;
}

static int haptic_vibrate_file_with_level(unsigned int handle, const char *path, int level, int iteration)
{
	char *buf;
	int size, ret;

	buf = convert_file_to_buffer(path, &size);
	if (!buf) {
		_E("Convert file to buffer error");
		return -EINVAL;
	}
	ret = haptic_vibrate_buffer_with_level(handle, buf, size, iteration, level, 1);
	free(buf);
	return ret;
}

static int haptic_close(void)
{
	char *arr[1];
	char buf_handle[32];

	if (!haptic_handle)
		return 0;
	snprintf(buf_handle, sizeof(buf_handle), "%u", haptic_handle);
	arr[0] = buf_handle;

	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_HAPTIC,
			DEVICED_INTERFACE_HAPTIC, METHOD_CLOSE_DEVICE,
			"u", arr);
}

static void haptic_init(void *data)
{
	_I("start test");
}

static void haptic_exit(void *data)
{
	_I("end test");
}

static int haptic_unit(int argc, char **argv)
{
	int duration = 0;
	int sleep_duration = 0;
	int count;
	int level;
	int iteration;
	int base_index;
	int index;
	int i;
	int ret = 0;
	struct haptic_effect_element *elem = NULL;
	unsigned char *effect = NULL;
	char *path;
	struct timespec time = {0,};

	for (i = 0; i < argc; i++)
		_D("argv[%d] = %s(%d)", i, argv[i], strlen(argv[i]));

	haptic_open();
	_D("handle %d", haptic_handle);

	if (argc > 3 &&
	    !strcmp("create", argv[2])) {
		/* create effect */
		path = argv[3];
		base_index = 4;
		if (!strcmp("level", argv[base_index])) {
			level = atoi(argv[base_index+1]);
			base_index += 2;
		} else
			level = 60;
		if (!strcmp("iteration", argv[base_index])) {
			iteration = atoi(argv[base_index+1]);
			base_index += 2;
		} else
			iteration = 1;
		if (!strcmp("duration", argv[base_index])) {
			duration = atoi(argv[base_index+1]);
			base_index += 2;
			count = argc -  base_index;
			_D("duration %d", duration);
		} else
			count = (argc - base_index)/2;
		elem = (struct haptic_effect_element *)malloc(sizeof(struct haptic_effect_element)*count);
		_D("pattern count %d", count);
		for (i = 0; i < count; i++) {
			if (duration) {
				index = i + base_index;
				if (index > argc) {
					_E("there is no arg %d", base_index);
					break;
				}
				sleep_duration += duration;/*elem_duration*/
				elem[i].haptic_duration = duration;/*elem_duration*/
				elem[i].haptic_level = atoi(argv[index]);/*elem_level*/
			} else {
				index = 2*i + base_index;
				if (base_index > argc || (index + 1) > argc) {
					_E("there is no arg %d", index);
					break;
				}
				sleep_duration += atoi(argv[index]);/*elem_duration*/
				elem[i].haptic_duration = atoi(argv[index]);/*elem_duration*/
				elem[i].haptic_level = atoi(argv[index+1]);/*elem_level*/
			}
			_D("[%d]duration %d, level %d", i, elem[i].haptic_duration, elem[i].haptic_level);
		}
		effect = (unsigned char *)malloc(64*1024);
		if (!effect)
			goto out;
		ret = haptic_create_effect(effect, 64*1024, elem, count);
		if (ret < 0) {
			_E("fail to create effect");
			goto out;
		}
		ret = haptic_save_effect(effect, 64*1024, path);
		if (ret < 0) {
			_E("fail to create file");
			goto out;
		}
		free(elem);
		elem = NULL;
		_D("path %s", path);
		haptic_vibrate_file_with_level(haptic_handle, path, level, iteration);
		time.tv_nsec = sleep_duration * iteration * NANO_SECOND_MULTIPLIER;
		nanosleep(&time, NULL);
		goto out;
	}
	/* effect create test & file test or direct file test*/
	if (argc > 3 &&
	    !strcmp("file", argv[2])) {
		path = argv[3];
		if (argc > 4)
			level = atoi(argv[4]);
		else
			level = 60;
		if (argc > 5)
			iteration = atoi(argv[5]);
		else
			iteration = 1;
		_D("path %s", path);
		haptic_vibrate_file_with_level(haptic_handle, path, level, iteration);
		sleep(5);
		goto out;
	}
	switch (argc) {
	case 3:/* xml buffer test */
		i = strlen(argv[2]);
		haptic_vibrate_xml_buffer(haptic_handle, argv[2], strlen(argv[2]), 1, 60, 1);
		time.tv_nsec = i * 10 * NANO_SECOND_MULTIPLIER;
		nanosleep(&time, NULL);
		break;
	case 5:/* monotone test */
		sleep_duration = duration = atoi(argv[2]);
		sleep_duration += atoi(argv[3]);
		count = atoi(argv[4]);
		do {
			ret = haptic_vibrate(duration);
			if (ret < 0)
				break;
			time.tv_nsec = sleep_duration * NANO_SECOND_MULTIPLIER;
			nanosleep(&time, NULL);
		} while (count--);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	if (elem)
		free(elem);
	if (effect)
		free(effect);
	haptic_close();
	_R("[OK] ---- %s          : monotone test finished", __func__);
	return ret;
}

static const struct test_ops haptic_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "haptic",
	.init     = haptic_init,
	.exit     = haptic_exit,
	.unit     = haptic_unit,
};

TEST_OPS_REGISTER(&haptic_test_ops)
