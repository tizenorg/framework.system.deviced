/*
 * deviced
 *
 * Copyright (c) 2012 - 2015 Samsung Electronics Co., Ltd.
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


#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <vconf.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

#include "core/log.h"
#include "core/device-notifier.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/udev.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "core/config-parser.h"
#include "block/block.h"
#include "apps/apps.h"
#include "mmc-handler.h"
#include "config.h"

#define SIGNAL_MMC_MOUNT_STATUS "Mount"
#define SIGNAL_MMC_FORMAT_STATUS "Format"

#define ODE_MOUNT_STATE	1

#define COMM_PKG_MGR_DBUS_SERVICE     "org.tizen.pkgmgr"
#define COMM_PKG_MGR_DBUS_PATH          "/org/tizen/pkgmgr"
#define COMM_PKG_MGR_DBUS_INTERFACE COMM_PKG_MGR_DBUS_SERVICE
#define COMM_PKG_MGR_METHOD                "CreateExternalDirectory"

#define SMACK_LABELING_TIME (0.5)

struct mmc_data {
	int option;
	char *devpath;
};

static char *mmc_curpath;
static bool mmc_disabled;
static Ecore_Timer *smack_timer;
static int mmc_mnt_status;
static int mmc_fmt_status;

static int mmc_mount_status(void)
{
	return mmc_mnt_status;
}

static int mmc_format_status(void)
{
	return mmc_fmt_status;
}

bool mmc_check_mounted(const char *mount_point)
{
	struct stat parent_stat, mount_stat;
	char parent_path[PATH_MAX];

	snprintf(parent_path, sizeof(parent_path), "%s", MMC_PARENT_PATH);

	if (stat(mount_point, &mount_stat) != 0 || stat(parent_path, &parent_stat) != 0)
		return false;

	if (mount_stat.st_dev == parent_stat.st_dev)
		return false;

	return true;
}

static int launch_syspopup(char *str)
{
	return launch_system_app(APP_DEFAULT,
			2, APP_KEY_TYPE, str);
}

static void request_smack_broadcast(void)
{
	int ret;

	ret = dbus_method_sync_timeout(COMM_PKG_MGR_DBUS_SERVICE,
			COMM_PKG_MGR_DBUS_PATH,
			COMM_PKG_MGR_DBUS_INTERFACE,
			COMM_PKG_MGR_METHOD,
			NULL, NULL, SMACK_LABELING_TIME*1000);
	if (ret != 0)
		_E("Failed to call dbus method (err: %d)", ret);
}

static void mmc_broadcast(char *sig, int status)
{
	static int old;
	static char sig_old[32];
	char *arr[1];
	char str_status[32];

	if (strcmp(sig_old, sig) == 0 && old == status)
		return;

	_D("%s %d", sig, status);

	old = status;
	snprintf(sig_old, sizeof(sig_old), "%s", sig);
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_MMC, DEVICED_INTERFACE_MMC,
			sig, "i", arr);
}

static Eina_Bool smack_timer_cb(void *data)
{
	if (smack_timer) {
		ecore_timer_del(smack_timer);
		smack_timer = NULL;
	}
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	mmc_mnt_status = MMC_OPERATION_COMPLETED;
	mmc_broadcast(SIGNAL_MMC_MOUNT_STATUS, mmc_mnt_status);
	return EINA_FALSE;
}

void mmc_mount_done(void)
{
	request_smack_broadcast();
	smack_timer = ecore_timer_add(SMACK_LABELING_TIME,
			smack_timer_cb, NULL);
	if (smack_timer) {
		_I("Wait to check");
		return;
	}
	_E("Fail to add abnormal check timer");
	smack_timer_cb(NULL);
}

static void mmc_mount(struct block_data *data, int result)
{
	int r = result;
	int status;

	assert(data);

	/* Mobile specific:
	 * The primary partition is only valid. */
	if (!data->primary)
		return;

	if (r == -EROFS)
		launch_syspopup("mountrdonly");
	/* Do not need to show error popup, if mmc is disabled */
	else if (r == -EWOULDBLOCK)
		goto error_without_popup;
	else if (r < 0)
		goto error;

	mmc_set_config(data->devnode, MAX_RATIO);

	/* give a transmutable attribute to mount_point */
	r = setxattr(data->mount_point, "security.SMACK64TRANSMUTE",
			"TRUE", strlen("TRUE"), 0);
	if (r < 0)
		_E("setxattr error : %d", errno);

	request_smack_broadcast();
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	mmc_mnt_status = MMC_OPERATION_COMPLETED;
	mmc_broadcast(SIGNAL_MMC_MOUNT_STATUS, mmc_mnt_status);
	return;

error:
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
			VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	mmc_mnt_status = MMC_OPERATION_FAILED;
	mmc_broadcast(SIGNAL_MMC_MOUNT_STATUS, mmc_mnt_status);
	status = launch_syspopup("mounterr");
	if (status < 0 && status == -ENODEV)
		return;
	_E("failed to mount device", r);
	return;

error_without_popup:
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
			VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	mmc_mnt_status = MMC_OPERATION_FAILED;
	mmc_broadcast(SIGNAL_MMC_MOUNT_STATUS, mmc_mnt_status);
	_E("failed to mount device : %d", r);
	return;
}

static void mmc_unmount(struct block_data *data, int result)
{
	int r;

	assert(data);

	/* Mobile specific:
	 * The primary partition is only valid. */
	if (!data->primary)
		return;

	if (result == 0)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
				VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	else
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
				VCONFKEY_SYSMAN_MMC_MOUNTED);
}

static void mmc_format(struct block_data *data, int result)
{
	int r;

	assert(data);

	/* Mobile specific:
	 * The primary partition is only valid. */
	if (!data->primary)
		return;

	/* Mobile specific:
	 * When try to unmount partition,
	 * it can make below key UNMOUNTED state.
	 * So it should be updated to original value. */
	if (data->state == BLOCK_MOUNT)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
				VCONFKEY_SYSMAN_MMC_MOUNTED);

	if (result == 0) {
		_I("Format Successful");
		mmc_fmt_status = MMC_OPERATION_COMPLETED;
	} else {
		_E("Format Failed : %d", result);
		mmc_fmt_status = MMC_OPERATION_FAILED;
	}

	mmc_broadcast(SIGNAL_MMC_FORMAT_STATUS, mmc_fmt_status);
}

static void mmc_insert(struct block_data *data)
{
	assert(data);

	/* Mobile specific:
	 * The primary partition is only valid. */
	if (!data->primary)
		return;

	if (mmc_curpath)
		return;
	mmc_curpath = strdup(data->devnode);

	_I("MMC inserted : %s", data->devnode);
}

static void mmc_remove(struct block_data *data)
{
	assert(data);

	/* Mobile specific:
	 * The primary partition is only valid. */
	if (!data->primary)
		return;

	free(mmc_curpath);
	mmc_curpath = NULL;
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);

	_I("MMC removed");
}

static DBusMessage *edbus_request_secure_mount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *path;
	int ret;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INVALID);
	if (!ret) {
		ret = -EBADMSG;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	/* check mount point */
	if (access(path, R_OK) != 0) {
		if (mkdir(path, 0755) < 0) {
			ret = -errno;
			goto error;
		}
	}

	_I("mount path : %s", path);
	ret = change_mount_point_legacy(mmc_curpath, path);
	if (ret < 0) {
		_E("fail to change mount point");
		goto error;
	}

	ret = mount_block_device_legacy(mmc_curpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_secure_unmount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *path;
	int ret;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INVALID);
	if (!ret) {
		ret = -EBADMSG;
		goto error;
	}

	_I("unmount path : %s", path);
	ret = unmount_block_device_legacy(mmc_curpath, UNMOUNT_NORMAL);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_mount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	if (mmc_disabled) {
		ret = -EWOULDBLOCK;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	ret = mount_block_device_legacy(mmc_curpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_unmount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	if (mmc_disabled) {
		ret = -EWOULDBLOCK;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	ret = unmount_block_device_legacy(mmc_curpath, opt);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_format(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	if (mmc_disabled) {
		ret = -EWOULDBLOCK;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}


	ret = format_block_device_legacy(mmc_curpath, NULL, opt);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_insert(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *devpath;
	int ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &devpath, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	/* TODO need to update it like udev */
	ret = -ENOTSUP;

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_remove(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	/* TODO need to update it like udev */
	ret = -ENOTSUP;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_change_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;
	char params[NAME_MAX];
	struct mmc_data *pdata;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}
	if (opt == VCONFKEY_SYSMAN_MMC_MOUNTED)
		mmc_mount_done();
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

int get_mmc_devpath(char devpath[])
{
	if (mmc_disabled)
		return -EWOULDBLOCK;
	if (!mmc_curpath)
		return -ENODEV;
	snprintf(devpath, NAME_MAX, "%s", mmc_curpath);
	return 0;
}

static DBusMessage *edbus_get_mount_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = mmc_mount_status();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_format_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = mmc_format_status();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "RequestSecureMount",    "s", "i", edbus_request_secure_mount },
	{ "RequestSecureUnmount",  "s", "i", edbus_request_secure_unmount },
	{ "RequestMount",         NULL, "i", edbus_request_mount },
	{ "RequestUnmount",        "i", "i", edbus_request_unmount },
	{ "RequestFormat",         "i", "i", edbus_request_format },
	{ "RequestInsert",         "s", "i", edbus_request_insert },
	{ "RequestRemove",        NULL, "i", edbus_request_remove },
	{ "ChangeStatus",          "i", "i", edbus_change_status },
	{ SIGNAL_MMC_MOUNT_STATUS, NULL, "i", edbus_get_mount_status },
	{ SIGNAL_MMC_FORMAT_STATUS, NULL, "i", edbus_get_format_status },
};

static void mmc_init(void *data)
{
	int ret;

	mmc_load_config();

	ret = register_edbus_interface_and_method(DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static int mmc_start(enum device_flags flags)
{
	mmc_disabled = false;
	_I("start");
	return 0;
}

static int mmc_stop(enum device_flags flags)
{
	mmc_disabled = true;
	/* TODO Don't you need to unmount the mounted devices? */
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	_I("stop");
	return 0;
}

const struct device_ops mmc_device_ops = {
	.name     = "mmc",
	.init     = mmc_init,
	.start    = mmc_start,
	.stop     = mmc_stop,
};

DEVICE_OPS_REGISTER(&mmc_device_ops)

const struct block_dev_ops mmc_block_ops = {
	.name       = "mmc",
	.block_type = BLOCK_MMC_DEV,
	.mounted    = mmc_mount,
	.unmounted  = mmc_unmount,
	.formatted  = mmc_format,
	.inserted   = mmc_insert,
	.removed    = mmc_remove,
};

BLOCK_DEVICE_OPS_REGISTER(&mmc_block_ops)
