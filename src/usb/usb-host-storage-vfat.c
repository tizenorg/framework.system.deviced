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

#define VFAT_MOUNT_OPT	"uid=5000,gid=5000,dmask=0002,fmask=0002,iocharset=iso8859-1,utf8,shortname=mixed"
#define SMACK_MOUNT_OPT "smackfsroot=*,smackfsdef=*"

static const char *vfat_check_arg[] = {
	"/usr/bin/fsck_msdosfs",
	"-pf", NULL, NULL,
};

static const char *vfat_arg[] = {
	"/sbin/mkfs.vfat",
	NULL, NULL,
};

static int vfat_check(const char *devname)
{
	int argc;
	argc = ARRAY_SIZE(vfat_check_arg);
	vfat_check_arg[argc - 2] = devname;
	return run_child(argc, vfat_check_arg);
}

static void get_mount_options(bool smack, char *options, int len)
{
	if (smack)
		snprintf(options, len, "%s,%s", VFAT_MOUNT_OPT, SMACK_MOUNT_OPT);
	else
		snprintf(options, len, "%s", VFAT_MOUNT_OPT);
}

static int vfat_mount(bool smack, const char *devpath, const char *mount_point)
{
	char options[NAME_MAX];
	get_mount_options(smack, options, sizeof(options));
	return mount(devpath, mount_point, "vfat", 0, options);
}

static int vfat_mount_rdonly(bool smack, const char *devpath, const char *mount_point)
{
	char options[NAME_MAX];
	get_mount_options(smack, options, sizeof(options));
	return mount(devpath, mount_point, "vfat", MS_RDONLY, options);
}

static int vfat_format(const char *path)
{
	int argc;
	argc = ARRAY_SIZE(vfat_arg);
	vfat_arg[argc - 2] = path;
	return run_child(argc, vfat_arg);
}

static const struct storage_fs_ops vfat_ops = {
	.name         = "vfat",
	.check        = vfat_check,
	.mount        = vfat_mount,
	.mount_rdonly = vfat_mount_rdonly,
	.format       = vfat_format,
};

static void __CONSTRUCTOR__ vfat_init(void)
{
	register_fs(&vfat_ops);
}
