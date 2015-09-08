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
#include <stdbool.h>
#include <sys/stat.h>
#include <vconf.h>
#include <errno.h>

#include "log.h"
#include "common.h"
#include "dd-deviced.h"
#include "dd-storage.h"

#define INT_PATH				"/opt/media"
#define EXT_PATH				"/opt/storage/sdcard"
#define EXT_PARENT_PATH			"/opt/storage"
#define EXT_OPTION1_PATH		"/mnt/mmc"

static bool check_ext_mount(void)
{
	struct stat parent, ext;

	if (stat(EXT_PATH, &ext) != 0 || stat(EXT_PARENT_PATH, &parent) != 0)
		return false;

	if (ext.st_dev == parent.st_dev)
		return false;

	return true;
}

API int storage_get_path(int type, unsigned char *path, int size)
{
	char *ext_path = EXT_PATH;
	int option;

	if (type > STORAGE_MAX || path == NULL)
		return -EINVAL;

	if (vconf_get_int(VCONFKEY_DEVICED_MOUNT_PATH_OPTION, &option) < 0)
		return -EPERM;

	_D("VCONFKEY_DEVICED_MOUNT_PATH_OPTION1 : %d, option : %d", VCONFKEY_DEVICED_MOUNT_PATH_OPTION1, option);
	if (option == VCONFKEY_DEVICED_MOUNT_PATH_OPTION1)
		ext_path = EXT_OPTION1_PATH;

	switch (type) {
	case STORAGE_DEFAULT:
		if (check_ext_mount())
			snprintf(path, size, "%s", ext_path);
		else
			snprintf(path, size, "");
		break;
	case STORAGE_INTERNAL:
		snprintf(path, size, "%s", INT_PATH);
		break;
	case STORAGE_EXTERNAL:
		snprintf(path, size, "%s", ext_path);
		break;
	default:
		break;
	}

	return 0;
}
