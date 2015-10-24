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


#ifndef __MMC_HANDLER_H__
#define __MMC_HANDLER_H__

#include <stdbool.h>

#define MMC_MOUNT_POINT		"/opt/storage/sdcard"

#define MMC_POPUP_NAME		"mmc-syspopup"
#define MMC_POPUP_APP_KEY	"_APP_NAME_"
#define MMC_POPUP_SMACK_VALUE	"checksmack"

enum mmc_dev_type{
	MMC_DEV_REMOVED = 0,
	MMC_DEV_INSERTED = 1,
};

enum mmc_operation_type{
	MMC_NOT_INITIALIZED = -1,
	MMC_OPERATION_COMPLETED = 0,
	MMC_OPERATION_FAILED = 1,
};

int get_mmc_devpath(char devpath[]);
bool mmc_check_mounted(const char *mount_point);
void mmc_mount_done(void);

#endif /* __MMC_HANDLER_H__ */
