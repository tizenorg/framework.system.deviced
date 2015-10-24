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
#include <errno.h>
#include <dlfcn.h>

#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "mmc/mmc-handler.h"

#define APP2EXT_PLUGIN_PATH	LIBDIR"/libapp2ext.so.0"

static void *app2ext_dl;
static int (*app2ext_enable)(const char *pkgid);
static int (*app2ext_disable)(const char *pkgid);

static dd_list *pkgid_head;

static int app2ext_mount(const char *pkgid)
{
	int r;

	if (!app2ext_enable)
		return -ENOENT;

	_I("Attempt mount %s app to sdcard", pkgid);
	r = app2ext_enable(pkgid);
	if (r < 0)
		return r;

	_I("Mount %s app to sdcard", pkgid);
	DD_LIST_APPEND(pkgid_head, strdup(pkgid));
	return 0;
}

int app2ext_unmount(void)
{
	char *str;
	dd_list *elem;
	int r, n, i;

	if (!app2ext_disable)
		return -ENOENT;

	/* if there is no mounted pkg */
	if (!pkgid_head)
		return 0;

	n = DD_LIST_LENGTH(pkgid_head);
	_I("pkgid_head count : %d", n);

	for (i = 0; i < n; ++i) {
		str = DD_LIST_NTH(pkgid_head, 0);
		_I("Attempt unmount %s app from sdcard", str);
		r = app2ext_disable(str);
		if (r < 0)
			return r;
		_I("Unmount %s app from sdcard", str);
		DD_LIST_REMOVE(pkgid_head, str);
		free(str);
	}

	return 0;
}

static DBusMessage *edbus_request_mount_app2ext(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *pkgid;
	int ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &pkgid, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (!mmc_check_mounted(MMC_MOUNT_POINT)) {
		_I("Do not mounted");
		ret = -ENODEV;
		goto error;
	}

	ret = app2ext_mount(pkgid);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "RequestMountApp2ext",    "s", "i", edbus_request_mount_app2ext },
};

static void app2ext_init(void *data)
{
	int ret;

	app2ext_dl = dlopen(APP2EXT_PLUGIN_PATH, RTLD_LAZY|RTLD_GLOBAL);
	if (!app2ext_dl) {
		_E("dlopen error (%s)", dlerror());
		return;
	}

	app2ext_enable = dlsym(app2ext_dl, "app2ext_enable_external_pkg");
	if (!app2ext_enable) {
		_E("dlsym error (%s)", dlerror());
		dlclose(app2ext_dl);
		app2ext_dl = NULL;
		return;
	}

	app2ext_disable = dlsym(app2ext_dl, "app2ext_disable_external_pkg");
	if (!app2ext_disable) {
		_E("dlsym error (%s)", dlerror());
		dlclose(app2ext_dl);
		app2ext_dl = NULL;
		return;
	}

	ret = register_edbus_interface_and_method(DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static void app2ext_exit(void *data)
{
	if (!app2ext_dl)
		return;

	dlclose(app2ext_dl);
	app2ext_dl = NULL;
}

const struct device_ops app2ext_device_ops = {
	.name     = "app2ext",
	.init     = app2ext_init,
	.exit     = app2ext_exit,
};

DEVICE_OPS_REGISTER(&app2ext_device_ops)
