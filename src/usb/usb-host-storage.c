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

#include "usb-host.h"
#include "core/device-notifier.h"
#include <sys/mount.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#define MOUNT_POINT     "/opt/storage/usb"
#define FS_TMPFS        "tmpfs"
#define SMACKFS_MNT	    "/smack"
#define SMACKFS_MAGIC	0x43415d53
#define SIZE_32GB       61315072

#define BLOCK_DEVICE_PATH         "/sys/block"
#define BLOCK_DEVICE_POLLING_TIME "events_poll_msecs"
#define POLLING_TIME_ABNORMAL     200
#define POLLING_TIME_NORMAL       500

#define RETRY_MAX 5

#define SYSTEM_SYSPOPUP         "system-syspopup"
#define METHOD_STORAGE_WARNING  "UsbotgWarningPopupLaunch"
#define METHOD_WATCHDOG         "WatchdogPopupLaunch"
#define METHOD_RECOVERY         "RecoveryPopupLaunch"
#define PARAM_MOUNTFAIL         "usbotg_mount_failed"
#define PARAM_REMOVED_UNSAFE    "usbotg_removed_unsafe"

#define USBOTG_SYSPOPUP         "usbotg-syspopup"
#define METHOD_STORAGE_MOUNT    "StoragePopupLaunch"
#define METHOD_CAMERA           "CameraPopupLaunch"
#define METHOD_STORAGE_UNMOUNT  "StorageUnmountPopupLaunch"
#define PARAM_STORAGE_ADD       "storage_add"
#define PARAM_STORAGE_REMOVE    "storage_remove"
#define POPUP_KEY_DEVICE_PATH   "_DEVICE_PATH_"

static int parts_num = 0;
static bool mount_flag = false;
static bool path_init = false;

static bool smack = false;
static dd_list *fs_list = NULL;

static int pipe_out[2];

struct job_data {
	int type;
	bool safe;
	struct usb_device dev;
};

static dd_list *storage_job = NULL;
static Ecore_Fd_Handler *pipe_handler;

static void __CONSTRUCTOR__ smack_check(void)
{
	struct statfs sfs;
	int ret;

	do {
		ret = statfs(SMACKFS_MNT, &sfs);
	} while (ret < 0 && errno == EINTR);

	if (ret == 0 && sfs.f_type == SMACKFS_MAGIC)
		smack = true;
	_I("smackfs check %d", smack);
}

void register_fs(const struct storage_fs_ops *ops)
{
	if (!ops)
		return;
	DD_LIST_APPEND(fs_list, ops);
}

void unregister_fs(const struct storage_fs_ops *ops)
{
	if (!ops)
		return;
	DD_LIST_REMOVE(fs_list, ops);
}

static struct storage_fs_ops *get_fs_ops(char *name)
{
	dd_list *l;
	struct storage_fs_ops *ops;

	if (!name)
		return NULL;

	DD_LIST_FOREACH(fs_list, l, ops) {
		if (!strncmp(ops->name, name, strlen(name)))
			return ops;
	}
	return NULL;
}

static int get_storage_size(char *name)
{
	int fd, ret;
	uint64_t size;

	if (!name)
		return -EINVAL;

	if (access(name, F_OK) != 0)
		return -EINVAL;

	fd = open(name, O_RDONLY);
	if (fd < 0) {
		_E("Failed to open (%s) with errno(%d)", name, errno);
		return -errno;
	}

	if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
		_E("Failed to get size of (%s), errno(%d)", name, errno);
		size = -errno;
	}

	close(fd);
	return size;
}

static int get_block_device_name(const char *devname, char *block, int len)
{
	char *name;

	name = strstr(devname, "sd");
	if (!name) {
		_E("Cannot get the device name");
		return -ENOMEM;
	}

	snprintf(block, len, "%s", name);
	name = block;
	while (*name) {
		if (*name >= 48 && *name <= 57) {
			*name = '\0';
			break;
		}
		name = name + 1;
	}
	_I("Block device name: (%s)", block);

	return 0;
}
static int get_block_size(const char *devname)
{
	char block[32];
	char block_size[BUF_MAX];
	char size[32];
	FILE *fp;
	int ret;

	ret = get_block_device_name(devname, block, sizeof(block));
	if (ret < 0) {
		_E("Failed to get block device name(%s)", devname);
		return ret;
	}

	snprintf(block_size, sizeof(block_size), "%s/%s/size", BLOCK_DEVICE_PATH, block);

	fp = fopen(block_size, "r");
	if (!fp) {
		_E("Failed to open block device size node");
		return -ENOMEM;
	}

	ret = fread(size, 1, sizeof(size), fp);
	fclose(fp);
	if (ret <= 0) {
		_E("Cannot read size of block device(%d)", ret);
		return -ENOMEM;
	}

	return atoi(size);
}

static int set_block_polling_time(char *devname, int time)
{
	FILE *fp;
	char block[32];
	char ptime[32];
	char tpath[BUF_MAX];
	int ret;

	ret = get_block_device_name(devname, block, sizeof(block));
	if (ret < 0) {
		_E("Failed to get block device name(%s)", devname);
		return ret;
	}

	snprintf(ptime, sizeof(ptime), "%d", time);
	snprintf(tpath, sizeof(tpath), "%s/%s/%s",
			BLOCK_DEVICE_PATH, block, BLOCK_DEVICE_POLLING_TIME);
	fp = fopen(tpath, "w");
	if (!fp) {
		_E("Failed to open file (%s)", tpath);
		return -ENOMEM;
	}

	ret = fwrite(ptime, 1, strlen(ptime), fp);
	fclose(fp);
	if (ret <= 0) {
		_E("Failed to write (%s) to file (%s)", ptime, tpath);
		return -ENOMEM;
	}

	return 0;
}

static int check_changed_storage(struct udev_device *dev)
{
	const char *devname;
	int size;

	if (!dev)
		return -EINVAL;

	devname = udev_device_get_property_value(dev, "DEVNAME");
	if (!devname) {
		_E("Failed to get device name");
		return -EINVAL;
	}

	return get_block_size(devname);
}

static bool check_storage_exists(struct udev_device *dev)
{
	const char *devname;
	char mntpath[BUF_MAX];
	bool same;
	int ret;

	if (!dev)
		return false;

	devname = udev_device_get_property_value(dev, "DEVNAME");
	if (!devname) {
		_E("Failed to get device name");
		return false;
	}

	ret = get_mount_path(devname, mntpath, sizeof(mntpath));
	if (ret < 0) {
		_E("Cannot get mount path");
		return false;
	}

	same = check_same_mntpath(mntpath);
	if (!same)
		set_block_polling_time((char *)devname, POLLING_TIME_ABNORMAL);
	return same;
}

static int mount_path_init(void)
{
	int ret, retry;

	if (path_init)
		return 0;

	if (access(MOUNT_POINT, F_OK) < 0) {
		ret = mkdir(MOUNT_POINT, 0755);
		if (ret < 0) {
			_E("Failed to make directory to mount usb storage");
			return ret;
		}
	}

	retry = 0;
	do {
		ret = mount(FS_TMPFS, MOUNT_POINT, FS_TMPFS, 0, "");
		if (ret == 0)
			break;
	} while (retry ++ < RETRY_MAX);

	if (ret < 0) {
		_E("Failed to mount tmpfs to mount point(%s)", MOUNT_POINT);
		goto out;
	}

	ret = chmod(MOUNT_POINT, 0755);
	if (ret < 0) {
		_E("Failed to change permission(%d)", MOUNT_POINT);
		goto out_unmount;
	}

	path_init = true;
	return 0;

out_unmount:
	if (umount2(MOUNT_POINT, MNT_DETACH) < 0)
		_E("Failed to unmount tmpfs(%s)", MOUNT_POINT);
out:
	if (rmdir(MOUNT_POINT) < 0)
		_E("Failed to remove directory(%s)", MOUNT_POINT);

	return ret;
}

static void mount_path_deinit(void)
{
	int ret, retry, len;
	dd_list *storage_list, *l;
	struct usb_device *dev;

	storage_list = get_storage_list();
	DD_LIST_FOREACH(storage_list, l, dev) {
		if (dev->is_mounted == true)
			return;
	}

	path_init = false;

	if (access(MOUNT_POINT, F_OK) < 0)
		return;

	if (umount2(MOUNT_POINT, MNT_DETACH) < 0)
		_E("Failed to unmount tmpfs(%s)", MOUNT_POINT);

	if (rmdir(MOUNT_POINT) < 0)
		_E("Failed to remove directory(%s)", MOUNT_POINT);

	return;
}

static void add_partition_number(int num)
{
	if (num <= 0)
		return;

	if (parts_num >= 0) {
		parts_num += num;
		return;
	}

	parts_num = num;
}

static void launch_mount_popup_by_flag(bool success, char *path)
{
	if (parts_num > 0)
		parts_num--;
	else
		parts_num = 0;

	if (success)
		mount_flag = true;

	if (parts_num != 0)
		return;

	if (mount_flag)
		mount_flag = false;
	else
		launch_host_syspopup(SYSTEM_SYSPOPUP, METHOD_STORAGE_WARNING,
				POPUP_KEY_CONTENT, PARAM_MOUNTFAIL, NULL, NULL);
}

int get_mount_path(const char *name, char *path, int len)
{
	char *dev;
	char buf[BUF_MAX];

	if (!name || !path || len <= 0)
		return -EINVAL;

	dev = strstr(name, "/sd");
	if (!dev)
		return -ENOMEM;

	dev = dev + 3;

	snprintf(buf, sizeof(buf), "%s", dev);
	if (*buf >= 97 && *buf <= 122)
		*buf = *buf - 32;

	snprintf(path, len, "%s/UsbDrive%s", MOUNT_POINT, buf);

	return 0;
}

int get_devname_by_path(char *path, char *name, int len)
{
	char *dev;
	char buf[BUF_MAX];

	if (!path || !name || len <= 0)
		return -EINVAL;

	dev = strstr(path, "UsbDrive");
	if (!dev)
		return -EINVAL;

	dev = dev + 8;

	snprintf(buf, sizeof(buf), "%s", dev);
	if (*buf >= 65 && *buf <= 90)
		*buf = *buf + 32;

	snprintf(name, len, "/dev/sd%s", buf);

	return 0;
}


static int find_app_accessing_storage(char *path)
{
	const char *argv[7] = {"/sbin/fuser", "-m", "-S", NULL, NULL, NULL};
	int argc;

	if (!path)
		return -EINVAL;

	argv[5] = path;

	argc = ARRAY_SIZE(argv);

	return run_child(argc, argv);
}

static int mount_storage(const char *fstype, const char *devname, char *path)
{
	dd_list *l;
	struct storage_fs_ops *ops;
	int ret, retry;

	if (!fstype || !devname || !path)
		return -EINVAL;

	ret = mount_path_init();
	if (ret < 0) {
		_E("Failed to init mount path");
		return ret;
	}

	DD_LIST_FOREACH(fs_list, l, ops) {
		if (strncmp(ops->name, fstype, strlen(fstype)))
			continue;

		ret = mkdir(path, 0755);
		if (ret < 0) {
			if (errno != EEXIST) {
				_E("Failed to make mount path(%s)", path);
				return -errno;
			}
		}

		if (ops->check) {
			ret = ops->check(devname);
			if (ret < 0) {
				_E("Failed to check device file system(%s, %s)", fstype, devname);
				goto out;
			}
		}

		if (!(ops->mount)) {
			_E("Cannot mount device");
			ret = -EINVAL;
			goto out;
		}

		retry = 0;
		do {
			ret = ops->mount(smack, devname, path);
			if (ret == 0 || errno == EROFS)
				break;
		} while (retry++ < RETRY_MAX);

		if (ret == 0)
			return ret;

		if (errno == EROFS && ops->mount_rdonly) {
			_I("Read only file system is connected");
			retry = 0;
			do {
				ret = ops->mount_rdonly(smack, devname, path);
				if (ret == 0) {
					ret = EROFS;
					break;
				}
			} while (retry++ < RETRY_MAX);
		}

		if (ret < 0) {
			_E("Failed to mount usb storage (devname: %s, ret: %d, errno: %d)", devname, ret, errno);
			goto out;
		}

		return ret;
	}

	_E("Not supported file system");
	ret = -ENOTSUP;
out:
	if (access(path, F_OK) == 0) {
		if (rmdir(path) < 0)
			_E("Failed to remove dir(%s)", path);
	}
	return ret;
}

static int unmount_storage(char *path)
{
	int ret;

	if (!path)
		return -EINVAL;

	ret = access(path, F_OK);
	if (ret < 0) {
		_E("The mount path (%s) does not exist", path);
		return ret;
	}

	ret = umount2(path, MNT_DETACH);
	if (ret < 0) {
		_E("Failed to unmount storage (%s), errno(%d)", path, errno);
		goto out;
	}

	ret = rmdir(path);
	if (ret < 0) {
		_E("Failed to remove mount path(%d), errno(%d)", path, errno);
		goto out;
	}

	return 0;

out:
	if (find_app_accessing_storage(path) < 0)
		_E("Failed to find apps accessing the storage(%d)", path);

	return ret;
}

int mount_usb_storage(const char *name)
{
	dd_list *storage_list, *l;
	struct usb_device *dev;
	bool found;
	int ret, mount_type;

	storage_list = get_storage_list();
	if (!storage_list) {
		_E("Storage list is NULL");
		return -ENOMEM;
	}

	found = false;
	DD_LIST_FOREACH(storage_list, l, dev) {
		if (strncmp(dev->name, name, strlen(name)))
			continue;
		found = true;
		break;
	}
	if (!found) {
		_E("Cannot find storage whose name is (%s)", name);
		return -EINVAL;
	}

	if (dev->is_mounted == true) {
		_E("(%s) is already mounted", dev->name);
		return -ECANCELED;
	}

	ret = mount_storage(dev->fs, dev->name, dev->mntpath);
	if (ret < 0) {
		_E("Failed to mount usb storage(%s, %s, %s): %d", dev->fs, dev->name, dev->mntpath, ret);
		launch_mount_popup_by_flag(false, dev->mntpath);
		return ret;
	}

	if (ret == EROFS)
		mount_type = STORAGE_MOUNT_RO;
	else
		mount_type = STORAGE_MOUNT_RW;

	dev->noti_id = activate_storage_notification(dev->mntpath, mount_type);
	dev->is_mounted = true;

	return ret;
}

static int format_usb_storage(const char *path)
{
	int ret, i;
	char name[BUF_MAX];
	dd_list *storage_list, *l;
	struct usb_device *dev;
	struct storage_fs_ops *fs_ops;
	bool found;

	if (!path)
		return -EINVAL;

	storage_list = get_storage_list();
	if (!storage_list) {
		_E("Storage list is NULL");
		return -ENOMEM;
	}

	found = false;
	DD_LIST_FOREACH(storage_list, l, dev) {
		if (strncmp(dev->mntpath, path, strlen(path)))
			continue;
		found = true;
		break;
	}
	if (!found) {
		_I("Cannot find storage whose name is (%s)", name);
		return -ENOENT;
	}

	if (dev->is_mounted == true) {
		ret = unmount_storage(dev->mntpath);
		if (ret < 0) {
			_E("Failed to unmount storage(%s): %d", dev->mntpath, ret);
			return ret;
		}
		dev->is_mounted = false;
		dev->noti_id = deactivate_storage_notification(dev->noti_id);
	}

	fs_ops = get_fs_ops(dev->fs);
	if (!fs_ops) {
		if (get_storage_size(dev->name) <= SIZE_32GB)
			fs_ops = get_fs_ops("vfat");
		else
			fs_ops = get_fs_ops("exfat");
		if (!fs_ops) {
			_E("Cannot get file system to format");
			return -EINVAL;
		}
	}

	ret = -1;
	if (fs_ops->format) {
		i = 0;
		do {
			ret = fs_ops->format(dev->name);
			if (ret < 0) {
				_E("Failed to format (%s, %d).. retry(%d)", dev->name, ret, i);
				continue;
			}
			break;
		} while (i++ < RETRY_MAX);
	}
	if (ret < 0) {
		_E("Falied to format (%s, %d)", dev->name, ret);
		return ret;
	}

	ret = mount_usb_storage(dev->name);
	if (ret < 0) {
		_E("Falied to mount storage(%s): %d", dev->mntpath, ret);
		return ret;
	}

	return 0;
}

int unmount_usb_storage(const char *path, bool safe)
{
	int ret;
	char name[BUF_MAX];
	dd_list *storage_list, *l;
	struct usb_device *dev;
	bool found;

	if (!path)
		return -EINVAL;

	storage_list = get_storage_list();
	if (!storage_list) {
		_E("Storage list is NULL");
		return -ENOMEM;
	}

	found = false;
	DD_LIST_FOREACH(storage_list, l, dev) {
		if (strncmp(dev->mntpath, path, strlen(path)))
			continue;
		found = true;
		break;
	}
	if (!found) {
		_I("Cannot find storage whose name is (%s)", name);
		mount_path_deinit();
		return -ENOENT;
	}

	if (dev->is_mounted == false) {
		_I("(%s) is already unmounted", dev->name);
		mount_path_deinit();
		return -ENOENT;
	}

	ret = unmount_storage(dev->mntpath);
	if (ret < 0) {
		_E("Failed to unmount storage(%s): %d", dev->mntpath, ret);
		return ret;
	}

	dev->is_mounted = false;
	dev->noti_id = deactivate_storage_notification(dev->noti_id);
	mount_path_deinit();

	return 0;
}

static void storage_device_added (struct udev_device *dev)
{
	const char *nparts;
	const char *fstype;
	const char *devname;
	const char *vendor;
	const char *model;
	char mountpath[BUF_MAX];
	int num, ret;
	bool mounted;
	int noti_id;
	struct usb_device usb_dev;

	if (!dev)
		return;

	nparts = udev_device_get_property_value(dev, "NPARTS");
	if (nparts) {
		num = atoi(nparts);
		if (num > 0) {
			add_partition_number(num);
			return;
		}
	}

	fstype = udev_device_get_property_value(dev, "ID_FS_TYPE");
	if (!fstype) {
		_E("Failed to get file system type");
		return;
	}

	devname = udev_device_get_property_value(dev, "DEVNAME");
	if (!devname) {
		_E("Failed to get device name");
		return;
	}

	model = udev_device_get_property_value(dev, "ID_MODEL");
	if (!model)
		_E("Failed to get model information");

	vendor = udev_device_get_property_value(dev, "ID_VENDOR");
	if (!vendor)
		_E("Failed to get vendor information");

	snprintf(usb_dev.name, sizeof(usb_dev.name), "%s", devname);
	snprintf(usb_dev.fs, sizeof(usb_dev.fs), "%s", fstype);
	snprintf(usb_dev.model, sizeof(usb_dev.model), "%s", model);
	snprintf(usb_dev.vendor, sizeof(usb_dev.vendor), "%s", vendor);
	ret = get_mount_path(devname, usb_dev.mntpath, sizeof(usb_dev.mntpath));
	if (ret < 0) {
		_E("Failed to get mount path(%d, %s)", ret, usb_dev.name);
		return ;
	}

	set_block_polling_time(usb_dev.name, POLLING_TIME_NORMAL);

	ret = add_job(STORAGE_ADD, &usb_dev, true);
	if (ret < 0) {
		_E("Failed to add job (%d, %s, %d)", STORAGE_ADD, usb_dev.name, ret);
		return ;
	}

	ret = add_job(STORAGE_MOUNT, &usb_dev, true);
	if (ret < 0)
		_E("Falied to mount storage (%d, %s, %d)", STORAGE_MOUNT, usb_dev.name, ret);
}

static void storage_device_removed (struct udev_device *dev)
{
	const char *devname = NULL;
	const char *nparts = NULL;
	char mountpath[BUF_MAX];
	int ret;
	struct usb_device usb_dev;
	dd_list *storage_list, *l;

	if (!dev)
		return;

	nparts = udev_device_get_property_value(dev, "NPARTS");
	if (nparts) {
		ret = atoi(nparts);
		if (ret > 0)
			return;
		if (ret == 0) {
			if (!udev_device_get_property_value(dev, "ID_FS_TYPE")
					&& !udev_device_get_property_value(dev, "DISK_MEDIA_CHANGE")) {
				return;
			}
		}
	}

	devname = udev_device_get_property_value(dev, "DEVNAME");
	if (!devname) {
		_E("cannot get device name");
		return;
	}

	snprintf(usb_dev.name, sizeof(usb_dev.name), "%s", devname);
	ret = get_mount_path(devname, usb_dev.mntpath, sizeof(usb_dev.mntpath));
	if (ret < 0) {
		_E("Failed to get mount path(%d, %s)", ret, usb_dev.name);
		return ;
	}

	ret = add_job(STORAGE_UNMOUNT, &usb_dev, false);
	if (ret < 0) {
		_E("Failed to add job (%d, %s, %d)", STORAGE_UNMOUNT, usb_dev.name, ret);
		return ;
	}

	ret = add_job(STORAGE_REMOVE, &usb_dev, true);
	if (ret < 0)
		_E("Falied to mount storage (%d, %s, %d)", STORAGE_REMOVE, usb_dev.name, ret);
}

static void *start_job(void *data)
{
	int result;
	struct job_data *job;
	struct usb_device *dev;

	if (DD_LIST_LENGTH(storage_job) <= 0) {
		_E("storage job list is NULL");
		result = -1;
		goto out;
	}

	job = DD_LIST_NTH(storage_job, 0);
	if (!job) {
		_E("cannot get 0th job");
		result = -1;
		goto out;
	}

	dev = &(job->dev);
	if (!dev) {
		_E("job->dev == NULL");
		result = -1;
		goto out;
	}

	switch (job->type) {
	case STORAGE_ADD:
		if (check_same_mntpath(dev->mntpath))
			result = -1;
		else
			result = add_usb_storage_to_list(dev->name, dev->vendor, dev->model, dev->fs);
		break;

	case STORAGE_REMOVE:
		if (check_same_mntpath(dev->mntpath))
			result = remove_usb_storage_from_list(dev->name);
		else
			result = -1;
		break;

	case STORAGE_MOUNT:
		if (is_storage_mounted(dev->mntpath))
			result = -1;
		else
			result = mount_usb_storage(dev->name);
		break;

	case STORAGE_UNMOUNT:
		if (is_storage_mounted(dev->mntpath))
			result = unmount_usb_storage(dev->mntpath, job->safe);
		else
			result = -1;
		break;

	case STORAGE_FORMAT:
		if (check_same_mntpath(dev->mntpath))
			result = format_usb_storage(dev->mntpath);
		else
			result = -1;
		break;

	default:
		_E("Unknown job type (%d)", job->type);
		result = -1;
		break;
	}

	_I("Job(%d, %s) is done: (%d)", job->type, dev->mntpath, result);

out:
	write(pipe_out[1], &result, sizeof(int));
	return NULL;
}

static int trigger_job(void)
{
	pthread_t th;
	int ret;

	if (DD_LIST_LENGTH(storage_job) <= 0) {
		return 0;
	}

	ret = pthread_create(&th, NULL, start_job, NULL);
	if (ret < 0) {
		_E("Failed to create pthread");
		return ret;
	}
	pthread_detach(th);

	return 0;
}

int add_job(int type, struct usb_device *dev, bool safe)
{
	struct job_data *job;
	struct usb_device *usb_dev;
	int len;

	len = DD_LIST_LENGTH(storage_job);

	job = (struct job_data *)malloc(sizeof(struct job_data));
	if (!job) {
		_E("malloc() failed");
		return -ENOMEM;
	}

	job->type = type;
	job->safe = safe;
	snprintf((job->dev).name, sizeof((job->dev).name), "%s", dev->name);
	snprintf((job->dev).mntpath, sizeof((job->dev).mntpath), "%s", dev->mntpath);
	snprintf((job->dev).vendor, sizeof((job->dev).vendor), "%s", dev->vendor);
	snprintf((job->dev).model, sizeof((job->dev).model), "%s", dev->model);
	snprintf((job->dev).fs, sizeof((job->dev).fs), "%s", dev->fs);

	DD_LIST_APPEND(storage_job, job);

	_I("ADD Job(%d, %s, %d)", job->type, (job->dev).name, job->safe);

	if (len > 0)
		return 0;

	if (trigger_job() < 0)
		_E("Failed to trigger job");


	return 0;
}

static bool check_to_skip_unmount_error(struct usb_device *dev)
{
	dd_list *l;
	struct job_data *job;
	int i;

	if (!storage_job || DD_LIST_LENGTH(storage_job) <= 0)
		return false;
	if (!dev)
		return false;

	i = 0;
	DD_LIST_FOREACH(storage_job, l, job) {
		if (i++ == 0)
			continue;
		if (strncmp((job->dev).mntpath, dev->mntpath, strlen((job->dev).mntpath)))
			continue;

		if (job->type == STORAGE_MOUNT)
			return true;
	}

	return false;
}

static int send_storage_notification(struct job_data *job, int result)
{
	char *noti_type;

	if (!job)
		return -EINVAL;

	if (result < 0 && job->type != STORAGE_FORMAT)
		return -EINVAL;

	switch (job->type) {
	case STORAGE_ADD:
		(job->dev).is_mounted = false;
		if (send_storage_changed_info(&(job->dev), "storage_unmount") < 0)
			_E("Failed to send signal");
		break;

	case STORAGE_REMOVE:
		(job->dev).is_mounted = false;
		if (send_storage_changed_info(&(job->dev), "storage_remove") < 0)
			_E("Failed to send signal");
		break;

	case STORAGE_FORMAT:
		if (result < 0) {
			(job->dev).is_mounted = false;
			send_msg_storage_removed(true);
			if (send_storage_changed_info(&(job->dev), "storage_unmount") < 0)
				_E("Failed to send signal");
			break;
		}
		/* if result >= 0, the notifications are same with storage mount */

	case STORAGE_MOUNT:
		if (result == EROFS)
			noti_type = TICKER_NAME_STORAGE_RO_CONNECTED;
		else
			noti_type = TICKER_NAME_STORAGE_CONNECTED;

		(job->dev).is_mounted = true;
		launch_mount_popup_by_flag(true, (job->dev).mntpath);
		launch_ticker_notification(noti_type);
		send_msg_storage_added(true);
		if (send_storage_changed_info(&(job->dev), "storage_mount") < 0)
			_E("Failed to send signal");
		launch_host_syspopup(USBOTG_SYSPOPUP, METHOD_STORAGE_MOUNT,
				POPUP_KEY_CONTENT, PARAM_STORAGE_ADD,
				POPUP_KEY_DEVICE_PATH, (job->dev).mntpath);
		break;

	case STORAGE_UNMOUNT:
		(job->dev).is_mounted = false;
		_I("Safe: (%d)", job->safe ? 1 : 0);
		if (job->safe)
			launch_ticker_notification(TICKER_NAME_STORAGE_DISCONNECTED_SAFE);
		else {
			if (!check_to_skip_unmount_error(&(job->dev)))
				launch_host_syspopup(SYSTEM_SYSPOPUP, METHOD_STORAGE_WARNING,
						POPUP_KEY_CONTENT, PARAM_REMOVED_UNSAFE, NULL, NULL);
		}
		send_msg_storage_removed(true);
		if (send_storage_changed_info(&(job->dev), "storage_unmount") < 0)
			_E("Failed to send signal");
		launch_host_syspopup(USBOTG_SYSPOPUP, METHOD_STORAGE_MOUNT,
				POPUP_KEY_CONTENT, PARAM_STORAGE_REMOVE,
				POPUP_KEY_DEVICE_PATH, (job->dev).mntpath);

		break;

	default:
		_E("Invalid job type(%d)", job->type);
		return -EINVAL;
	}

	return 0;
}

static void remove_job(int result)
{
	struct job_data *job;

	if (!storage_job || DD_LIST_LENGTH(storage_job) <= 0) {
		return ;
	}

	job = DD_LIST_NTH(storage_job, 0);
	if (!job) {
		_E("Failed to get job from the storage_job list");
		return;
	}

	if (send_storage_notification(job, result) < 0)
		_E("Failed to send notification");

	_I("Remove Job(%d, %s)", job->type, (job->dev).name);

	DD_LIST_REMOVE(storage_job, job);
	free(job);

	if (DD_LIST_LENGTH(storage_job) <= 0) {
		_I("REMOVE storage_job FULL");
		DD_LIST_FREE_LIST(storage_job);
		storage_job = NULL;
		return ;
	}

	if (trigger_job() < 0)
		_E("Failed to trigger job");

}

static Eina_Bool job_done(void *data, Ecore_Fd_Handler *fd_handler)
{
	int result;
	int ret, retry;

	update_usbhost_state();

	if (!ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ)) {
		_E("ecore_main_fd_handler_active_get error , return");
		return EINA_TRUE;
	}

	retry = 0;
	do {
		ret = read(pipe_out[0], &result, sizeof(int));
		if (ret >= 0)
			break;
		if (errno != EINTR)
			goto out;
	} while (retry++ < RETRY_MAX);


	if (result < 0) {
		/*Error handling*/
		_E("Job of thread failed (%d)", result);
	}

out:
	remove_job(result);

	return EINA_TRUE;
}

static void subsystem_block_changed (struct udev_device *dev)
{
	const char *action = NULL;
	const char *bus = NULL;
	int size;

	if (!dev)
		return;

	if (!is_host_uevent_enabled())
		return;

	/* Check whether or not the block device is usb device */
	bus = udev_device_get_property_value(dev, "ID_BUS");
	if (!bus)
		return;
	if (strncmp(bus, "usb", strlen(bus)))
		return;


	action = udev_device_get_property_value(dev, "ACTION");
	if (!action)
		return;

	if (!strncmp(action, "add", strlen("add"))) {
		send_msg_storage_added(false);
		storage_device_added(dev);
		return;
	}

	if (!strncmp(action, "remove", strlen("remove"))) {
		send_msg_storage_removed(false);
		storage_device_removed(dev);
		return;
	}

	if (!strncmp(action, "change", strlen("change"))) {
		size = check_changed_storage(dev);
		_I("block size(%d)", size);
		if (size > 0) {
			send_msg_storage_added(false);
			storage_device_added(dev);
		} else {
			if (check_storage_exists(dev)) {
				send_msg_storage_removed(false);
				storage_device_removed(dev);
			}
		}
		return;
	}
}

static int pipe_start(void)
{
	int ret;

	ret = pipe(pipe_out);
	if (ret < 0) {
		_E("Failed to make pipe(%d)", ret);
		return ret;
	}

	pipe_handler = ecore_main_fd_handler_add(pipe_out[0], ECORE_FD_READ,
			job_done, NULL, NULL, NULL);
	if (!pipe_handler) {
		_E("Failed to register pipe handler");
		return -ENOMEM;
	}

	return 0;
}

static void pipe_stop(void)
{
	if (pipe_handler) {
		ecore_main_fd_handler_del(pipe_handler);
		pipe_handler = NULL;
	}

	if (pipe_out[0] >= 0)
		close(pipe_out[0]);
	if (pipe_out[1] >= 0)
		close(pipe_out[1]);
}

const static struct uevent_handler uhs_storage[] = {
	{ BLOCK_SUBSYSTEM       ,     subsystem_block_changed       ,    NULL    },
};

static int usbhost_storage_init_booting_done(void *data)
{
	int ret, i;

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_storage_init_booting_done);

	for (i = 0 ; i < ARRAY_SIZE(uhs_storage) ; i++) {
		ret = register_uevent_control(&uhs_storage[i]);
		if (ret < 0)
			_E("FAIL: reg_uevent_control()");
	}

	ret = pipe_start();
	if (ret < 0)
		_E("Failed to make pipe(%d)", ret);

	return 0;
}

static void usbhost_storage_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, usbhost_storage_init_booting_done);
}

static void usbhost_storage_exit(void *data)
{
	int i, ret;

	for (i = 0 ; i < ARRAY_SIZE(uhs_storage) ; i++) {
		unregister_uevent_control(&uhs_storage[i]);
	}

	pipe_stop();
}

static const struct device_ops usbhost_device_storage_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "usbhost_storage",
	.init     = usbhost_storage_init,
	.exit     = usbhost_storage_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_storage_ops)
