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


#ifndef __DD_BLOCK_H__
#define __DD_BLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <glib.h>
#include <gio/gio.h>
#include "dbus.h"


/**
 * @file        dd-block.h
 * @defgroup    CAPI_SYSTEM_DEVICED_BLOCK_MODULE BLOCK
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of block devices
 * @section CAPI_SYSTEM_DEVICED_BLOCK_MODULE_HEADER Required Header
 *   \#include <dd-block.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_BLOCK_MODULE
 * @{
 */

enum block_type {
	BLOCK_SCSI,
	BLOCK_MMC,
	BLOCK_ALL,
};

enum mount_state {
	BLOCK_UNMOUNT,
	BLOCK_MOUNT,
};

enum block_state {
	BLOCK_REMOVED,
	BLOCK_ADDED,
	BLOCK_CHANGED,
};

/**
 * @brief This structure defines the block device data
 */
typedef struct _block_device {
	enum block_type type;
	char *devnode;
	char *syspath;
	char *fs_usage;
	char *fs_type;
	char *fs_version;
	char *fs_uuid;
	bool readonly;
	char *mount_point;
	enum mount_state state;
	bool primary;   /* the first partition */
} block_device;

typedef GList block_list;

#define BLOCK_LIST_FOREACH(head, node) \
	block_list *elem; \
	for (elem = head, node = NULL; \
		 elem && ((node = elem->data) != NULL); \
		 elem = g_list_next(elem), node = NULL)

#define BLOCK_LIST_APPEND(head, node) \
	head = g_list_append(head, node);

#define BLOCK_LIST_REMOVE(head, node) \
	head = g_list_remove(head, node);

typedef int (*block_changed_cb)(block_device *dev, enum block_state state, void *data);
typedef int (*block_request_cb)(block_device *dev, int result, void *data);

void deviced_block_release_device(block_device **dev);
void deviced_block_release_list(block_list **list);
int deviced_block_get_list(block_list **list, enum block_type type);

int deviced_block_register_device_change(enum block_type type, block_changed_cb func, void *data);
void deviced_block_unregister_device_change(enum block_type type, block_changed_cb func);

block_device *deviced_block_duplicate_device(block_device *dev);
int deviced_block_update_device(block_device *src, block_device *dst);

int deviced_block_mount(block_device *dev, block_request_cb func, void *user_data);
int deviced_block_unmount(block_device *dev, block_request_cb func, void *user_data);
int deviced_block_format(block_device *dev, block_request_cb func, void *user_data);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_BLOCK_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif /* __DD_BLOCK_H__ */
