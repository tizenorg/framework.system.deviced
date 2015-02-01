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
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "core/common.h"
#include "core/log.h"
#include "mmc-handler.h"

#define FS_EXFAT_NAME "EXFAT"

#define FS_EXFAT_MOUNT_OPT	"uid=5000,gid=5000,dmask=0002,fmask=0002"

static const char *exfat_arg[] = {
	"/sbin/mkfs.exfat",
	"-t", "exfat", "-s", "9", "-c", "8", "-b", "11", "-f", "1", "-l", "tizen", NULL, NULL,
};

static const char *exfat_check_arg[] = {
	"/sbin/fsck.exfat",
	"-f", "-R", NULL, NULL,
};

static struct fs_check exfat_info = {
	FS_TYPE_EXFAT,
	"exfat",
};

static bool exfat_match(const char *devpath)
{
	int fd, len, r;
	int argc;
	char buf[BUF_LEN];
	char *tmpbuf;

	argc = ARRAY_SIZE(exfat_check_arg);
	exfat_check_arg[argc - 2] = devpath;

	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		_E("failed to open fd(%s) : %s", devpath, strerror(errno));
		return false;
	}

	/* check file system name */
	len = sizeof(buf);
	tmpbuf = buf;
	while (len != 0 && (r = read(fd, tmpbuf, len)) != 0) {
		if (r < 0) {
			if (errno == EINTR)
				continue;
			goto error;
		}
		len -= r;
		tmpbuf += r;
	}

	if (strncmp((buf + 3), FS_EXFAT_NAME, strlen(FS_EXFAT_NAME)))
		goto error;

	close(fd);
	_I("MMC type : %s", exfat_info.name);
	return true;

error:
	close(fd);
	_E("failed to match with exfat(%s %s)", devpath, buf);
	return false;
}

static int exfat_check(const char *devpath)
{
	int argc;
	argc = ARRAY_SIZE(exfat_check_arg);
	exfat_check_arg[argc - 2] = devpath;
	return run_child(argc, exfat_check_arg);
}

static int exfat_mount(bool smack, const char *devpath, const char *mount_point)
{
	char options[NAME_MAX];
	int r, retry = RETRY_COUNT;

	if (smack)
		snprintf(options, sizeof(options), "%s,%s", FS_EXFAT_MOUNT_OPT, SMACKFS_MOUNT_OPT);
	else
		snprintf(options, sizeof(options), "%s", FS_EXFAT_MOUNT_OPT);

	do {
		r = mount(devpath, mount_point, "exfat", 0, options);
		if (!r) {
			_I("Mounted mmc card [exfat]");
			return 0;
		}
		usleep(100000);
	} while (r < 0 && errno == ENOENT && retry-- > 0);

	return -errno;
}

static int exfat_format(const char *devpath)
{
	int argc;
	argc = ARRAY_SIZE(exfat_arg);
	exfat_arg[argc - 2] = devpath;
	return run_child(argc, exfat_arg);
}

static const struct mmc_fs_ops exfat_ops = {
	.type = FS_TYPE_EXFAT,
	.name = "exfat",
	.match = exfat_match,
	.check = exfat_check,
	.mount = exfat_mount,
	.format = exfat_format,
};

static void __CONSTRUCTOR__ module_init(void)
{
	add_fs(&exfat_ops);
}
/*
static void __DESTRUCTOR__ module_exit(void)
{
	remove_fs(&exfat_ops);
}
*/
