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

#include "core/log.h"
#include "core/list.h"
#include "block/block.h"
#include "apps/apps.h"

#define SCSI_KEY_PATH		"_DEVICE_PATH_"
#define SCSI_MOUNTED		"usbotg_storage_mounted"
#define SCSI_MOUNT_FAILED	"usbotg_storage_mount_failed"
#define SCSI_UNMOUNTED		"usbotg_storage_unmounted"

static void scsi_mounted(struct block_data *data, int result)
{
	int ret;

	if (!data)
		return;
	if (data->state != BLOCK_MOUNT)
		return;
	if (data->mount_point == NULL)
		return;
	if (result == -EROFS)
		_I("Read only scsi device");
	else if (result < 0) {
		_E("scsi mount result is (%d)", result);
		goto out_err;
	}

	ret = launch_system_app(APP_DEFAULT,
			4, APP_KEY_TYPE, SCSI_MOUNTED,
			SCSI_KEY_PATH, data->mount_point);
	if (ret < 0)
		 _E("Failed to launch scsi mounted popup (%d)", ret);

	return;

out_err:
	ret = launch_system_app(APP_DEFAULT,
			2, APP_KEY_TYPE, SCSI_MOUNT_FAILED);
	if (ret < 0)
		_E("Failed to launch scsi mount failed popup (%d)", ret);
}

static void scsi_unmounted(struct block_data *data, int result)
{
	int ret;

	if (!data)
		return;
	if (data->state != BLOCK_UNMOUNT)
		return;
	if (data->mount_point == NULL)
		return;
	if (result < 0) {
		_E("scsi unmount result is (%d)", result);
		return;
	}

	ret = launch_system_app(APP_DEFAULT,
			4, APP_KEY_TYPE, SCSI_UNMOUNTED,
			SCSI_KEY_PATH, data->mount_point);
	if (ret < 0)
		_E("Failed to launch scsi unmounted popup (%d)", ret);
}

static void scsi_formatted(struct block_data *data, int result)
{
	/* Do nothing. This can be changed if UX is changed */
}

static void scsi_inserted(struct block_data *data)
{
	/* Do nothing. This can be changed if UX is changed */
}

static void scsi_removed(struct block_data *data)
{
	/* TODO:
	 * check if the block device is removed after unmount */
}

const struct block_dev_ops scsi_block_ops = {
	.name       = "scsi",
	.block_type = BLOCK_SCSI_DEV,
	.mounted    = scsi_mounted,
	.unmounted  = scsi_unmounted,
	.formatted  = scsi_formatted,
	.inserted   = scsi_inserted,
	.removed    = scsi_removed,
};

BLOCK_DEVICE_OPS_REGISTER(&scsi_block_ops)

