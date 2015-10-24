/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <vconf.h>
#include <stdbool.h>
#include <errno.h>
#include <gio/gio.h>
#include <glib.h>
#include <limits.h>

#include "list.h"
#include "log.h"
#include "dbus.h"
#include "common.h"
#include "dd-block.h"

#define CHECK_STR(a) (a ? a : "")

#define BLOCK_GET_LIST       "GetDeviceList"
#define BLOCK_METHOD_MOUNT   "Mount"
#define BLOCK_METHOD_UNMOUNT "Unmount"
#define BLOCK_METHOD_FORMAT  "Format"

#define BLOCK_OBJECT_ADDED   "ObjectAdded"
#define BLOCK_OBJECT_REMOVED "ObjectRemoved"
#define BLOCK_DEVICE_CHANGED "DeviceChanged"

#define DBUS_REPLY_TIMEOUT (-1)
#define FORMAT_TIMEOUT	(120*1000)

#define DEV_PREFIX           "/dev/"

struct block_callback {
	block_changed_cb func;
	enum block_type type;
	void *data;
	guint b_id;
	guint bm_id;
};

struct block_request {
	block_device *dev;
	block_request_cb func;
	void *user_data;
};

static GDBusConnection *conn;
static block_list *changed_list;

static void block_release_internal(block_device *dev)
{
	if (!dev)
		return;
	free(dev->devnode);
	free(dev->syspath);
	free(dev->fs_usage);
	free(dev->fs_type);
	free(dev->fs_version);
	free(dev->fs_uuid);
	free(dev->mount_point);
}

void deviced_block_release_device(block_device **dev)
{
	if (!dev || !*dev)
		return;
	block_release_internal(*dev);
	free(*dev);
	*dev = NULL;
}

void deviced_block_release_list(block_list **list)
{
	block_device *dev;

	if (*list == NULL)
		return;

	BLOCK_LIST_FOREACH(*list, dev) {
		block_release_internal(dev);
		free(dev);
	}

	g_list_free(*list);
	*list = NULL;
}

block_device *deviced_block_duplicate_device(block_device *dev)
{
	block_device *blk;

	if (!dev)
		return NULL;

	blk = calloc(1, sizeof(block_device));
	if (!blk) {
		_E("calloc() failed");
		return NULL;
	}

	blk->type = dev->type;
	blk->devnode = strdup(dev->devnode);
	blk->syspath = strdup(dev->syspath);
	blk->fs_usage = strdup(dev->fs_usage);
	blk->fs_type = strdup(dev->fs_type);
	blk->fs_version = strdup(dev->fs_version);
	blk->fs_uuid = strdup(dev->fs_uuid);
	blk->readonly = dev->readonly;
	blk->mount_point = strdup(dev->mount_point);
	blk->state = dev->state;
	blk->primary = dev->primary;

	return blk;
}

int deviced_block_update_device(block_device *src, block_device *dst)
{
	if (!src || !dst)
		return -EINVAL;

	block_release_internal(dst);

	dst->type = src->type;
	dst->devnode = strdup(src->devnode);
	dst->syspath = strdup(src->syspath);
	dst->fs_usage = strdup(src->fs_usage);
	dst->fs_type = strdup(src->fs_type);
	dst->fs_version = strdup(src->fs_version);
	dst->fs_uuid = strdup(src->fs_uuid);
	dst->readonly = src->readonly;
	dst->mount_point = strdup(src->mount_point);
	dst->state = src->state;
	dst->primary = src->primary;

	return 0;
}

static GDBusConnection *get_dbus_connection(void)
{
	GError *err = NULL;
	static GDBusConnection *conn = NULL;

	if (conn)
		return conn;

#if !GLIB_CHECK_VERSION(2,35,0)
	g_type_init();
#endif

	conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
	if (!conn) {
		_E("fail to get dbus connection : %s",
				err ? err->message : "g_bus_get_sync error");
		g_clear_error(&err);
		return NULL;
	}
	return conn;
}

static GVariant *dbus_method_call_sync(const gchar *dest, const gchar *path,
				const gchar *iface, const gchar *method, GVariant *param)
{
	GDBusConnection *conn;
	GError *err = NULL;
	GVariant *ret;

	if (!dest || !path || !iface || !method || !param)
		return NULL;

	conn = get_dbus_connection();
	if (!conn) {
		_E("fail to get dbus connection");
		return NULL;
	}

	ret = g_dbus_connection_call_sync(conn,
			dest, path, iface, method,
			param, NULL, G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, &err);
	if (!ret) {
		_E("dbus method sync call failed(%s)",
				err ? err->message : "g_dbus_connection_call_sync()");
		g_clear_error(&err);
		return NULL;
	}

	return ret;
}

int deviced_block_get_list(block_list **list, enum block_type type)
{
	GVariant *result = NULL;
	GVariantIter *iter = NULL;
	char *type_str;
	block_list *local = NULL;
	block_device *elem, info;
	int ret;

	if (!list)
		return -EINVAL;

	switch (type) {
	case BLOCK_SCSI:
		type_str = "scsi";
		break;
	case BLOCK_MMC:
		type_str = "mmc";
		break;
	case BLOCK_ALL:
		type_str = "all";
		break;
	default:
		return -EINVAL;
	}

	result = dbus_method_call_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER,
			BLOCK_GET_LIST,
			g_variant_new("(s)", type_str));
	if (!result) {
		_E("Failed to get block device info");
		return -EIO;
	}

	g_variant_get(result, "(a(issssssisib))", &iter);

	while (g_variant_iter_loop(iter, "(issssssisib)",
				&info.type, &info.devnode, &info.syspath,
				&info.fs_usage, &info.fs_type,
				&info.fs_version, &info.fs_uuid,
				&info.readonly, &info.mount_point,
				&info.state, &info.primary)) {

		elem = (block_device *)malloc(sizeof(block_device));
		if (!elem) {
			_E("malloc() failed");
			ret = -ENOMEM;
			g_variant_iter_free(iter);
			goto err_out;
		}

		elem->type = info.type;
		elem->readonly = info.readonly;
		elem->state = info.state;
		elem->primary = info.primary;
		elem->devnode = strdup(CHECK_STR(info.devnode));
		elem->syspath = strdup(CHECK_STR(info.syspath));
		elem->fs_usage = strdup(CHECK_STR(info.fs_usage));
		elem->fs_type = strdup(CHECK_STR(info.fs_type));
		elem->fs_version = strdup(CHECK_STR(info.fs_version));
		elem->fs_uuid = strdup(CHECK_STR(info.fs_uuid));
		elem->mount_point = strdup(CHECK_STR(info.mount_point));

		BLOCK_LIST_APPEND(local, elem);
	}

	g_variant_iter_free(iter);
	g_variant_unref(result);

	*list = local;

	return g_list_length(local);

err_out:
	if (result)
		g_variant_unref(result);

	deviced_block_release_list(list);
	return ret;
}

static char *get_devnode_from_path(char *path)
{
	if (!path)
		return NULL;
	/* 1 means '/' */
	return path + strlen(DEVICED_PATH_BLOCK_DEVICES) + 1;
}

static void block_object_path_changed(enum block_state state,
		GVariant *params, gpointer user_data)
{
	block_device *dev;
	struct block_callback *bc;
	gchar *path;
	char *devnode;
	int ret;

	if (!params)
		return;

	g_variant_get(params, "(s)", &path);

	devnode = get_devnode_from_path((char *)path);
	if (!devnode)
		return;

	dev = calloc(1, sizeof(block_device));
	if (!dev)
		return;

	dev->devnode = strdup(devnode);
	if (dev->devnode == NULL) {
		_E("strdup() failed");
		free(dev);
		return;
	}

	BLOCK_LIST_FOREACH(changed_list, bc) {
		if (!bc->func)
			continue;
		ret = bc->func(dev, state, bc->data);
		if (ret < 0)
			_E("Failed to call callback for devnode(%s, %d)", devnode, ret);
	}

	free(dev->devnode);
	free(dev);
}

static void block_device_added(GVariant *params, gpointer user_data)
{
	block_object_path_changed(BLOCK_ADDED, params, user_data);
}

static void block_device_removed(GVariant *params, gpointer user_data)
{
	block_object_path_changed(BLOCK_REMOVED, params, user_data);
}

static void block_device_changed(GVariant *params, gpointer user_data)
{
	block_device *dev;
	struct block_callback *bc;
	int ret;

	if (!params)
		return;

	dev = calloc(1, sizeof(block_device));
	if (!dev)
		return;

	g_variant_get(params, "(issssssisib)",
			&dev->type,
			&dev->devnode,
			&dev->syspath,
			&dev->fs_usage,
			&dev->fs_type,
			&dev->fs_version,
			&dev->fs_uuid,
			&dev->readonly,
			&dev->mount_point,
			&dev->state,
			&dev->primary);

	BLOCK_LIST_FOREACH(changed_list, bc) {
		if (!bc->func)
			continue;
		if (bc->type != BLOCK_ALL && bc->type != dev->type)
			continue;
		ret = bc->func(dev, BLOCK_CHANGED, bc->data);
		if (ret < 0)
			_E("Failed to call callback for devnode(%s, %d)", dev->devnode, ret);
	}

	free(dev);
}

static void block_changed(GDBusConnection *conn,
		const gchar *sender,
		const gchar *path,
		const gchar *iface,
		const gchar *signal,
		GVariant *params,
		gpointer user_data)
{
	size_t iface_len, signal_len;

	if (!params || !sender || !path || !iface || !signal)
		return;

	iface_len = strlen(iface) + 1;
	signal_len = strlen(signal) + 1;

	if (!strncmp(iface, DEVICED_INTERFACE_BLOCK_MANAGER, iface_len)) {
		if (!strncmp(signal, BLOCK_OBJECT_ADDED, signal_len))
			block_device_added(params, user_data);
		else if (!strncmp(signal, BLOCK_OBJECT_REMOVED, signal_len))
			block_device_removed(params, user_data);
		return;
	}

	if (!strncmp(iface, DEVICED_INTERFACE_BLOCK, iface_len) &&
		!strncmp(signal, BLOCK_DEVICE_CHANGED, signal_len)) {
		block_device_changed(params, user_data);
		return;
	}
}

int deviced_block_register_device_change(enum block_type type, block_changed_cb func, void *data)
{
	GDBusConnection *conn = NULL;
	GError *err = NULL;
	guint b_id = 0, bm_id = 0;
	struct block_callback *bc = NULL;
	int ret;

	if (!func)
		return -EINVAL;

	BLOCK_LIST_FOREACH(changed_list, bc) {
		if (bc->func != func)
			continue;
		if (bc->type != type)
			continue;
		if (bc->b_id == 0 || bc->bm_id == 0)
			continue;

		return -EEXIST;
	}

	bc = (struct block_callback *)malloc(sizeof(struct block_callback));
	if (!bc) {
		_E("malloc() failed");
		return -ENOMEM;
	}

	conn = get_dbus_connection();
	if (!conn) {
		_E("Failed to get dbus connection");
		ret = -EPERM;
		goto out;
	}

	b_id = g_dbus_connection_signal_subscribe(conn,
			DEVICED_BUS_NAME,
			DEVICED_INTERFACE_BLOCK,
			"DeviceChanged",
			NULL,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			block_changed,
			NULL,
			NULL);
	if (b_id == 0) {
		_E("Failed to subscrive bus signal");
		ret = -EPERM;
		goto out;
	}

	bm_id = g_dbus_connection_signal_subscribe(conn,
			DEVICED_BUS_NAME,
			DEVICED_INTERFACE_BLOCK_MANAGER,
			NULL,
			DEVICED_PATH_BLOCK_MANAGER,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			block_changed,
			NULL,
			NULL);
	if (bm_id == 0) {
		_E("Failed to subscrive bus signal");
		ret = -EPERM;
		goto out;
	}

	bc->func = func;
	bc->data = data;
	bc->b_id = b_id;
	bc->bm_id = bm_id;
	bc->type = type;

	BLOCK_LIST_APPEND(changed_list, bc);

	return 0;

out:
	free(bc);
	if (conn) {
		if (b_id > 0)
			g_dbus_connection_signal_unsubscribe(conn, b_id);
	}
	return ret;
}

void deviced_block_unregister_device_change(enum block_type type, block_changed_cb func)
{
	GDBusConnection *conn;
	GError *err = NULL;
	struct block_callback *bc;

	if (!func)
		return;

	conn = get_dbus_connection();
	if (!conn) {
		_E("fail to get dbus connection");
		return;
	}

	BLOCK_LIST_FOREACH(changed_list, bc) {
		if (bc->func != func)
			continue;
		if (bc->type != type)
			continue;
		if (bc->b_id > 0)
			g_dbus_connection_signal_unsubscribe(conn, bc->b_id);
		if (bc->bm_id > 0)
			g_dbus_connection_signal_unsubscribe(conn, bc->bm_id);

		BLOCK_LIST_REMOVE(changed_list, bc);
		free(bc);
	}
}

static int get_object_path(char *devnode, char *path, size_t len)
{
	char *name;
	size_t prefix_len;

	if (!devnode || !path)
		return -EINVAL;

	prefix_len = strlen(DEV_PREFIX);

	if (strncmp(devnode, DEV_PREFIX, prefix_len))
		return -EINVAL;

	name = devnode + prefix_len;
	snprintf(path, len, "%s/%s", DEVICED_PATH_BLOCK_DEVICES, name);
	return 0;
}

static struct block_request *make_request_obj(block_device *dev,
		block_request_cb func, void *user_data)
{
	struct block_request *req;

	req = calloc(1, sizeof(struct block_request));
	if (!req) {
		_E("calloc() failed");
		return NULL;
	}

	req->dev = dev;
	req->func = func;
	req->user_data = user_data;

	return req;
}

static void block_request_operation_cb(GObject *obj, GAsyncResult *res, gpointer data)
{
	GDBusConnection *connection;
	struct block_request *req;
	GVariant *dbus_res;
	GError *err = NULL;
	int ret, result;

	req = (struct block_request *)data;
	if (!req)
		return;

	if (!req->func)
		goto out;

	connection = G_DBUS_CONNECTION(obj);
	dbus_res = g_dbus_connection_call_finish(connection, res, &err);

	g_variant_get(dbus_res, "(i)", &result);

	ret = req->func(req->dev, result, req->user_data);
	if (ret < 0)
		_E("Failed to call callback(%d)", ret);

	g_variant_unref(dbus_res);

out:
	free(req);
}

static int dbus_method_call_async(const gchar *dest, const gchar *path,
		const gchar *iface, const gchar *method, GVariant *param,
		gint timeout, GAsyncReadyCallback func, gpointer user_data)
{
	GDBusConnection *conn;
	GError *err = NULL;

	conn = get_dbus_connection();
	if (!conn) {
		_E("fail to get dbus connection");
		return -EPERM;
	}

	g_dbus_connection_call(conn,
			dest, path, iface, method,
			param, NULL, G_DBUS_CALL_FLAGS_NONE,
			timeout, NULL, func, user_data);

	return 0;
}

int deviced_block_mount(block_device *dev, block_request_cb func, void *user_data)
{
	struct block_request *req;
	char path[256];
	int ret;

	if (!dev)
		return -EINVAL;

	ret = get_object_path(dev->devnode, path, sizeof(path));
	if (ret < 0) {
		_E("Failed to get object path (%d)", ret);
		return ret;
	}

	req = make_request_obj(dev, func, user_data);
	if (!req) {
		_E("Failed to make request object");
		return -ENOMEM;
	}

	return dbus_method_call_async(DEVICED_BUS_NAME,
			path, DEVICED_INTERFACE_BLOCK,
			BLOCK_METHOD_MOUNT,
			g_variant_new("(s)",""),
			DBUS_REPLY_TIMEOUT,
			block_request_operation_cb,
			(gpointer)req);
}

int deviced_block_unmount(block_device *dev, block_request_cb func, void *user_data)
{
	struct block_request *req;
	char path[256];
	int ret;

	if (!dev)
		return -EINVAL;

	ret = get_object_path(dev->devnode, path, sizeof(path));
	if (ret < 0) {
		_E("Failed to get object path (%d)", ret);
		return ret;
	}

	req = make_request_obj(dev, func, user_data);
	if (!req) {
		_E("Failed to make request object");
		return -ENOMEM;
	}

	return dbus_method_call_async(DEVICED_BUS_NAME,
			path, DEVICED_INTERFACE_BLOCK,
			BLOCK_METHOD_UNMOUNT,
			g_variant_new("(i)", 1),
			DBUS_REPLY_TIMEOUT,
			block_request_operation_cb,
			(gpointer)req);
}

int deviced_block_format(block_device *dev, block_request_cb func, void *user_data)
{
	struct block_request *req;
	char path[256];
	int ret;

	if (!dev)
		return -EINVAL;

	ret = get_object_path(dev->devnode, path, sizeof(path));
	if (ret < 0) {
		_E("Failed to get object path (%d)", ret);
		return ret;
	}

	req = make_request_obj(dev, func, user_data);
	if (!req) {
		_E("Failed to make request object");
		return -ENOMEM;
	}

	return dbus_method_call_async(DEVICED_BUS_NAME,
			path, DEVICED_INTERFACE_BLOCK,
			BLOCK_METHOD_FORMAT,
			g_variant_new("(i)", 1),
			FORMAT_TIMEOUT,
			block_request_operation_cb,
			(gpointer)req);
}
