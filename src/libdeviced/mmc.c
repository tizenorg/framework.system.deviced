/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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
#include <errno.h>

#include "log.h"
#include "dbus.h"
#include "common.h"
#include "dd-mmc.h"
#include "dd-block.h"

#define METHOD_REQUEST_SECURE_MOUNT		"RequestSecureMount"
#define METHOD_REQUEST_SECURE_UNMOUNT	"RequestSecureUnmount"

#define ODE_MOUNT_STATE 1

#define FORMAT_TIMEOUT	(120*1000)

static block_device *mmc_dev;

API int mmc_secure_mount(const char *mount_point)
{
	if (mount_point == NULL)
		return -EINVAL;

	char *arr[1];
	arr[0] = (char *)mount_point;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC, METHOD_REQUEST_SECURE_MOUNT, "s", arr);
}

API int mmc_secure_unmount(const char *mount_point)
{
	if (mount_point == NULL)
		return -EINVAL;

	char *arr[1];
	arr[0] = (char *)mount_point;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC, METHOD_REQUEST_SECURE_UNMOUNT, "s", arr);
}

static block_device *get_mmc_device(void)
{
	block_device *dev = NULL;
	block_list *list = NULL;
	int ret;

	if (mmc_dev)
		return mmc_dev;

	ret = deviced_block_get_list(&list, BLOCK_MMC);
	if (ret < 0) {
		_E("Failed to get mmc list (%d)", ret);
		return NULL;
	}

	if (!list) {
		_E("There is no mmc");
		return NULL;
	}

	BLOCK_LIST_FOREACH(list, dev) {
		if (dev->primary)
			break;
	}

	if (!dev) {
		_E("There is no mmc which is primary device");
		goto out;
	}

	mmc_dev = deviced_block_duplicate_device(dev);
	if (!mmc_dev)
		_E("Failed to copy mmc object");

out:
	if (list)
		deviced_block_release_list(&list);

	return mmc_dev;
}

static void release_mmc_device(void)
{
	deviced_block_release_device(&mmc_dev);
}

static int mmc_changed_cb(block_device *dev, enum block_state state, void *data)
{
	static block_device *bdev;
	int ret;

	if (state == BLOCK_ADDED)
		return 0;

	bdev = get_mmc_device();
	if (!bdev)
		return 0;

	if (strncmp(bdev->devnode, dev->devnode, strlen(dev->devnode) + 1))
		return 0;

	if (state == BLOCK_CHANGED) {
		ret = deviced_block_update_device(dev, bdev);
		if (ret < 0)
			_E("Failed to update mmc object(%d)", ret);
		return ret;
	}

	if (state == BLOCK_REMOVED)
		release_mmc_device();

	return 0;
}

static int mmc_request_cb(block_device *dev, int result, void *data)
{
	struct mmc_contents *mmc_data = (struct mmc_contents *)data;

	if (mmc_data && mmc_data->mmc_cb)
		mmc_data->mmc_cb(result, mmc_data->user_data);

	return 0;
}

API int deviced_request_mount_mmc(struct mmc_contents *mmc_data)
{
	block_device *dev;
	int ret;

	if (!mmc_data)
		return -EINVAL;

	ret = deviced_block_register_device_change(BLOCK_MMC, mmc_changed_cb, NULL);
	if (ret == 0)
		_I("mmc changed callback is registered");

	dev = get_mmc_device();
	if (!dev) {
		_E("There is no mmc");
		return -ENOENT;
	}

	return deviced_block_mount(dev, mmc_request_cb, mmc_data);
}

API int deviced_request_unmount_mmc(struct mmc_contents *mmc_data, int option/* deprecated */)
{
	block_device *dev;
	int ret;

	if (!mmc_data)
		return -EINVAL;

	ret = deviced_block_register_device_change(BLOCK_MMC, mmc_changed_cb, NULL);
	if (ret == 0)
		_I("mmc changed callback is registered");

	dev = get_mmc_device();
	if (!dev) {
		_E("There is no mmc");
		return -ENOENT;
	}

	return deviced_block_unmount(dev, mmc_request_cb, mmc_data);
}

API int deviced_request_format_mmc(struct mmc_contents *mmc_data)
{
	return deviced_format_mmc(mmc_data, 1);
}

API int deviced_format_mmc(struct mmc_contents *mmc_data, int option /* deprecated */)
{
	block_device *dev;
	int ret;

	if (!mmc_data)
		return -EINVAL;

	ret = deviced_block_register_device_change(BLOCK_MMC, mmc_changed_cb, NULL);
	if (ret == 0)
		_I("mmc changed callback is registered");

	dev = get_mmc_device();
	if (!dev) {
		_E("There is no mmc");
		return -ENOENT;
	}

	return deviced_block_format(dev, mmc_request_cb, mmc_data);
}
