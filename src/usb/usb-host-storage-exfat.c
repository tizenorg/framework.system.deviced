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
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>

#include "core/common.h"
#include "usb-host.h"

#define EXFAT_MOUNT_OPT	"uid=5000,gid=5000,dmask=0002,fmask=0002"
#define SMACK_MOUNT_OPT "smackfsroot=*,smackfsdef=*"

static const char *exfat_check_arg[] = {
	"/sbin/fsck.exfat",
	"-f", "-R", NULL, NULL,
};

static const char *exfat_arg[] = {
	"/sbin/mkfs.exfat",
	"-t", "exfat", "-s", "9", "-c", "8", "-b", "11", "-f", "1", "-l", "tizen", NULL, NULL,
};

static int exfat_check(const char *devname)
{
	int argc;
	argc = ARRAY_SIZE(exfat_check_arg);
	exfat_check_arg[argc - 2] = devname;
	return run_child(argc, exfat_check_arg);
}

static void get_mount_options(bool smack, char *options, int len)
{
	if (smack)
		snprintf(options, len, "%s,%s", EXFAT_MOUNT_OPT, SMACK_MOUNT_OPT);
	else
		snprintf(options, len, "%s", EXFAT_MOUNT_OPT);
}

static int exfat_mount(bool smack, const char *devpath, const char *mount_point)
{
	char options[NAME_MAX];
	get_mount_options(smack, options, sizeof(options));
	return mount(devpath, mount_point, "exfat", 0, options);
}

static int exfat_mount_rdonly(bool smack, const char *devpath, const char *mount_point)
{
	char options[NAME_MAX];
	get_mount_options(smack, options, sizeof(options));
	return mount(devpath, mount_point, "exfat", MS_RDONLY, options);
}

static int exfat_format(const char *path)
{
	int argc;
	argc = ARRAY_SIZE(exfat_arg);
	exfat_arg[argc - 2] = path;
	return run_child(argc, exfat_arg);
}

static const struct storage_fs_ops exfat_ops = {
	.name         = "exfat",
	.check        = exfat_check,
	.mount        = exfat_mount,
	.mount_rdonly = exfat_mount_rdonly,
	.format       = exfat_format,
};

static void __CONSTRUCTOR__ exfat_init(void)
{
	register_fs(&exfat_ops);
}
