/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

#include "log.h"
#include "list.h"
#include "common.h"
#include "devices.h"
#include "shared/dbus.h"

#define SERVICE_NAME		"deviced"
#define DUMP_SIGNAL		"Dump"
#define DUMP_START_SIGNAL	"Start"
#define DUMP_FINISH_SIGNAL	"Finish"
#define DEFAULT_DUMP_PATH	"/var/log"

static void send_dump_signal(char *signal)
{
	char *arr[1];
	char str[6];

	snprintf(str, sizeof(str), "%d", getpid());
	arr[0] = str;

	broadcast_edbus_signal(DUMP_SERVICE_OBJECT_PATH,
	    DUMP_SERVICE_INTERFACE_NAME, signal, "i", arr);
}

static void dump_all_devices(int mode, char *path)
{
	dd_list *head = get_device_list_head();
	dd_list *elem;
	FILE *fp = NULL;
	char fname[PATH_MAX];
	const struct device_ops *dev;

	/* send start signal to dump service */
	send_dump_signal(DUMP_START_SIGNAL);

	/* open saved file */
	snprintf(fname, PATH_MAX, "%s/%s.log", path, SERVICE_NAME);

	if (path)
		fp = fopen(fname, "w+");
	if (!fp)
		_I("Failed to open %s, print to DLOG", fname);

	/* save dump each device ops */
	DD_LIST_FOREACH(head, elem, dev) {
		if (dev->dump) {
			_D("[%s] get dump", dev->name);
			LOG_DUMP(fp, "\n==== %s\n\n", dev->name);
			dev->dump(fp, mode, dev->dump_data);
		}
	}

	/* close file */
	if (fp)
		fclose(fp);

	/* send finish signal to dump service */
	send_dump_signal(DUMP_FINISH_SIGNAL);

	_D("%s(%d) dump is saved!", fname, mode);
}

static void dump_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret, mode = 0;
	char *path = NULL;

	ret = dbus_message_is_signal(msg, DUMP_SERVICE_INTERFACE_NAME,
	    DUMP_SIGNAL);
	if (!ret) {
		_E("there is no dump signal");
		return;
	}

	dbus_error_init(&err);

	/* get path for saving */
	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &mode,
	    DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		path = NULL;
	}

	dump_all_devices(mode, path);
}

static void dump_init(void *data)
{
	int ret;

	/* register dump signal */
	ret = register_edbus_signal_handler(DUMP_SERVICE_OBJECT_PATH,
	    DUMP_SERVICE_INTERFACE_NAME, DUMP_SIGNAL,
	    dump_signal_handler);
	if (ret < 0 && ret != -EEXIST)
		_E("Failed to register signal handler! %d", ret);
}

static const struct device_ops dump_device_ops = {
	.name     = "dump",
	.init     = dump_init,
};

DEVICE_OPS_REGISTER(&dump_device_ops)

