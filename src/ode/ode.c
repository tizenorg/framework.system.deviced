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
#include <stdbool.h>
#include <stdlib.h>
#include <vconf.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/mount.h>
#include <errno.h>
#include <mntent.h>

#include "display/poll.h"
#include "display/core.h"
#include "core/log.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "ode.h"
#include "noti.h"
#include "mmc/mmc-handler.h"

#define SIGNAL_REQUEST_GENERAL_MOUNT		"RequestGeneralMount"
#define SIGNAL_REQUEST_ODE_MOUNT			"RequestOdeMount"
#define SIGNAL_REQUEST_REMOVE_ERROR_NOTI	"RequestRemoveErrorNoti"
#define SIGNAL_REMOVE_MMC					"RemoveMmc"

#define VCONFKEY_ENCRYPT_NEEDED_SIZE		"db/sde/encrypt_size"

#define ODE_CRYPT_META_FILE ".MetaEcfsFile"
#define ODE_CRYPT_PROGRESS_FILE ".EncOngoing"
#define ODE_VERIFY_ENC_KEY	"/opt/etc/edk_p_sd"

#define PWLOCK_LAUNCH_NAME	"launch-app"
#define PWLOCK_NAME		"pwlock"

enum ode_progress_id {
	ENCRYPT_START,
	DECRYPT_START,
	ENCRYPT_MOUNTED,
	ODE_PROGRESS_MAX,
};

/* EN_DEV : Enable device policy
 * DE_DEV : Disable device policy
 * EN_MMC : Encrypted mmc
 * DE_MMC : Decrypted mmc
 * NO_MMC : no mmc
 */
enum ode_state {
	ENCRYPTED_DEVICE_ENCRYPTED_MMC = 0,			/* device policy ENABLE, mmc encryption ENABLE */
	ENCRYPTED_DEVICE_DECRYPTED_MMC,
	ENCRPYTED_DEVICE_NO_MMC,
	DECRYPTED_DEVICE_ENCRYPTED_MMC,
	DECRPYTED_DEVICE_DECRYPTED_MMC,
	ENCRYPT_ONGOING,
};

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static const char *ode_state_str[] = {
	[ENCRYPTED_DEVICE_ENCRYPTED_MMC] = "ENCRYPTED_DEVICE_ENCRYPTED_MMC",
	[ENCRYPTED_DEVICE_DECRYPTED_MMC] = "ENCRYPTED_DEVICE_DECRYPTED_MMC",
	[ENCRPYTED_DEVICE_NO_MMC] = "ENCRPYTED_DEVICE_NO_MMC",
	[DECRYPTED_DEVICE_ENCRYPTED_MMC] = "DECRYPTED_DEVICE_ENCRYPTED_MMC",
	[DECRPYTED_DEVICE_DECRYPTED_MMC] = "DECRPYTED_DEVICE_DECRYPTED_MMC",
	[ENCRYPT_ONGOING] = "ENCRYPT_OR_DECRYPT_ONGOING",
};

static int display_state = S_NORMAL;
static int ode_display_changed(void *data)
{
	enum state_t state = (enum state_t)data;
	char *str;
	int rate;
	if (display_state == S_LCDOFF && (state == S_NORMAL || state == S_LCDDIM)) {
		str = vconf_get_str(VCONFKEY_SDE_ENCRYPT_PROGRESS);
		if (str) {
			rate = atoi(str);
			if (rate >= 0 && rate <  100)
				noti_progress_update(rate);
		}
	}
	display_state = state;
	return 0;
}

static int ode_check_encrypt_sdcard()
{
	int ret = 0;
	struct stat src_stat, enc_stat;
	const char * cryptTempFile = ODE_CRYPT_META_FILE;
	const char * cryptProgressFile = ODE_CRYPT_PROGRESS_FILE;
	char *mMetaDataFile;

	mMetaDataFile = malloc(strlen (MMC_MOUNT_POINT) + strlen (cryptTempFile) +2);
	if (mMetaDataFile)
	{
		sprintf (mMetaDataFile, "%s%s%s", MMC_MOUNT_POINT, "/", cryptTempFile);
		if (lstat (mMetaDataFile, &src_stat) < 0)
			if (errno == ENOENT)
				ret = -1;
		free(mMetaDataFile);
	}
	if (!ret && lstat (ODE_VERIFY_ENC_KEY, &enc_stat) < 0)
		if (errno == ENOENT)
			ret = -1;
	_D("check sd card ecryption : %d", ret);
	return ret;
}

static int ode_check_encrypt_progress()
{
	int ret = 0;
	struct stat src_stat;
	const char * cryptProgressFile = ODE_CRYPT_PROGRESS_FILE;
	char *mMetaDataFile;

	mMetaDataFile = malloc(strlen (MMC_MOUNT_POINT) + strlen (cryptProgressFile) +2);
	if (mMetaDataFile)
	{
		sprintf (mMetaDataFile, "%s%s%s", MMC_MOUNT_POINT, "/", cryptProgressFile);
		if (lstat (mMetaDataFile, &src_stat) == 0)
			ret = -1;
		free(mMetaDataFile);
	}
	return ret;
}

static int ode_check_ecryptfs(char* path)
{
	int ret = false;
	struct mntent* mnt;
	const char* table = "/etc/mtab";
	FILE* fp;
	fp = setmntent(table, "r");
	if (!fp)
		return ret;
	while (mnt=getmntent(fp)) {
		if (!strcmp(mnt->mnt_type, "ecryptfs") && !strcmp(mnt->mnt_dir, path)) {
			ret = true;
			break;
		}
	}
	endmntent(fp);
	return ret;
}

static int ode_unmount_encrypt_sdcard(char* path)
{
	int result = 0;
	if ( ode_check_ecryptfs(path)) {
		if(umount2(path, MNT_DETACH) != 0) {
			if(umount2(path, MNT_EXPIRE) != 0) {
				_E("Unmount failed for drive %s err(%d %s)\n", path, errno, strerror(errno));
				if(errno == EAGAIN) {
					SLOGE("Trying Unmount again\n");
					if(umount2(path, MNT_EXPIRE) != 0) {
						result = -1;
						_E("Unmount failed for drive %s err(%d %s)\n", path, errno, strerror(errno));
					}
				} else {
					_E("Drive %s unmounted failed \n", path);
					result = -1;
				}
			}
		}
		if (result == 0)
			_D("Drive %s unmounted successfully \n", path);
	}
	return result;
}

static void launch_syspopup(const char *option)
{
	int r;
	char str[256];
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	snprintf(str, sizeof(str), "%s%s", "ode", option);

	FIND_DEVICE_VOID(apps, "apps");

	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return;
	}
	params->name = MMC_POPUP_NAME;
	params->key = POPUP_KEY_CONTENT;
	params->value = strdup(str);
	apps->init((void *)params);
	free(params);
	return;
}

static int ode_check_error_state(void)
{
	char *str;
	int type, state, val, r;

	/* get progress value */
	str = vconf_get_str(VCONFKEY_SDE_ENCRYPT_PROGRESS);
	if (str) {
		if (strcmp(str, "-1")) {	/* normal state */
			free(str);
			return 0;
		} else						/* error state */
			free(str);
	}

	/* get memory size */
	r = vconf_get_int(VCONFKEY_ENCRYPT_NEEDED_SIZE, &val);
	/* get crypto state */
	str = vconf_get_str(VCONFKEY_SDE_CRYPTO_STATE);
	if (!str)
		goto out;

	if (r == 0 && val == 0) {
		state = OPERATION_FAIL;
		if (!strcmp(str, "encryption_start"))
			type = ENCRYPT_TYPE;
		else
			type = DECRYPT_TYPE;
	}
	else {
		state = NOT_ENOUGH_SPACE;
		if (!strcmp(str, "unencrypted"))
			type = ENCRYPT_TYPE;
		else
			type = DECRYPT_TYPE;
	}

	free(str);

	/* get memory size */
	r = vconf_get_int(VCONFKEY_ENCRYPT_NEEDED_SIZE, &val);
	if (r == 0 && val == 0)
		state = OPERATION_FAIL;
	else
		state = NOT_ENOUGH_SPACE;

	noti_error_show(type, state, val);

out:
	return -EPERM;
}

int ode_judge_state(void)
{
	int dev, mmc_en;
	int r;
	char *en_state;

	if (vconf_get_bool(VCONFKEY_SETAPPL_MMC_ENCRYPTION_STATUS_BOOL, &dev) < 0) {
		_E("fail to get ode policy vconf");
		return -EPERM;
	}

	en_state = vconf_get_str(VCONFKEY_SDE_CRYPTO_STATE);
	if (en_state) {
		if (ode_check_encrypt_progress() &&
			    (!strcmp(en_state, "encryption_start")  || !strcmp(en_state, "decryption_start"))) {
			r = ENCRYPT_ONGOING;
			_I("ODE state : %s(%d)", ode_state_str[r], r);
			free(en_state);
			return r;
		} else
			free(en_state);
	}

	mmc_en = ode_check_encrypt_sdcard();

	/* although mmc does not exist, this api will return ENCRYPTED_DEVICE_DECRYPTED_MMC */
	if (dev && mmc_en)
		r = ENCRYPTED_DEVICE_DECRYPTED_MMC;
	else if (dev && !mmc_en)
		r = ENCRYPTED_DEVICE_ENCRYPTED_MMC;
	else if (!dev && !mmc_en)
		r = DECRYPTED_DEVICE_ENCRYPTED_MMC;
	else
		r = DECRPYTED_DEVICE_DECRYPTED_MMC;

	_I("ODE state : %s(%d)", ode_state_str[r], r);
	return r;
}

static void launch_pwlock(void)
{
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	FIND_DEVICE_VOID(apps, "apps");

	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return;
	}
	params->name = PWLOCK_LAUNCH_NAME;
	params->key = PWLOCK_NAME;
	apps->init((void *)params);
	free(params);
}

int ode_launch_app(int state)
{
	switch (state) {
	case ENCRYPTED_DEVICE_DECRYPTED_MMC: launch_syspopup("encrypt"); break;
	case DECRYPTED_DEVICE_ENCRYPTED_MMC: launch_syspopup("decrypt"); break;
	case ENCRYPTED_DEVICE_ENCRYPTED_MMC:
	case ENCRYPT_ONGOING:
		launch_pwlock();
		break;
	default: break;
	}
	return 0;
}

static void progress_start(void *value)
{
	pm_lock_internal(1, LCD_OFF, STAY_CUR_STATE, 0);
	register_notifier(DEVICE_NOTIFIER_LCD, ode_display_changed);
	noti_progress_show((int)value);
	_D("progress start : type(%d)", (int)value);
}

static void progress_finish(int rate)
{
	char *str;
	int ret;

	pm_unlock_internal(1, LCD_OFF, PM_SLEEP_MARGIN);
	unregister_notifier(DEVICE_NOTIFIER_LCD, ode_display_changed);
	noti_progress_clear();
	_D("progress finish");

	/* in case of error state */
	ret = ode_check_error_state();
	if (ret < 0)
		return;

	/* when finished to encrypt or decrypt, update mount vconf in this time */
	mmc_mount_done();

	str = vconf_get_str(VCONFKEY_SDE_CRYPTO_STATE);
	if (!strcmp(str, "encrypted")) {
		noti_finish_show(ENCRYPT_TYPE);
		vconf_set_bool(VCONFKEY_SETAPPL_MMC_ENCRYPTION_STATUS_BOOL, true);
	}
	else if (!strcmp(str, "unencrypted")) {
		noti_finish_show(DECRYPT_TYPE);
		vconf_set_bool(VCONFKEY_SETAPPL_MMC_ENCRYPTION_STATUS_BOOL, false);
	}

	free(str);
}

static void ode_mount(void *value)
{
	mmc_mount_done();
	_D("set mount complete vconf");
}

static struct ode_progress_cb {
	const char *str;
	void (*func) (void *value);
	void *value;
} ode_progress[ODE_PROGRESS_MAX] = {
	[ENCRYPT_START] = {"encryption_start", progress_start, (void*)ENCRYPT_TYPE},
	[DECRYPT_START] = {"decryption_start", progress_start, (void*)DECRYPT_TYPE},
	[ENCRYPT_MOUNTED] = {"mounted", ode_mount, NULL},
};

static void ode_crypto_state_cb(keynode_t *key, void* data)
{
	char *str;
	int i;

	str = vconf_keynode_get_str(key);
	if (!str)
		return;

	for (i = 0; i < ODE_PROGRESS_MAX; ++i) {
		if (!strcmp(str, ode_progress[i].str)) {
			if (ode_progress[i].func)
				ode_progress[i].func(ode_progress[i].value);
			break;
		}
	}
}

static void ode_crypto_progress_cb(keynode_t *key, void* data)
{
	char *str;
	int rate;

	str = vconf_keynode_get_str(key);
	if (!str)
		return;

	rate = atoi(str);

	if (display_state == S_NORMAL || display_state == S_LCDDIM)
		noti_progress_update(rate);
	_D("progress update : rate(%d)", rate);

	if (rate == 100 || rate < 0)
		progress_finish(rate);
}

static void ode_request_general_mount(void *data, DBusMessage *msg)
{
	mmc_mount_done();
	vconf_set_bool(VCONFKEY_SETAPPL_MMC_ENCRYPTION_STATUS_BOOL, false);
	_D("ode_request_general_mount occurs!!!");
}

static void ode_request_ode_mount(void *data, DBusMessage *msg)
{
	launch_pwlock();
	vconf_set_bool(VCONFKEY_SETAPPL_MMC_ENCRYPTION_STATUS_BOOL, true);
	_D("ode_request_ode_mount occurs!!!");
}

static void ode_request_remove_error_noti(void *data, DBusMessage *msg)
{
	if (noti_error_clear() < 0)
		_E("Failed to remove error notification");
}

int ode_mmc_removed(void)
{
	int r;

	r = ode_unmount_encrypt_sdcard(MMC_MOUNT_POINT);
	if (r < 0)
		_E("fail to sde_crypto_unmount");

	/* delete error noti */
	noti_error_clear();

	/* send signal to syspopup for deleting a popup when removed mmc */
	broadcast_edbus_signal(DEVICED_PATH_ODE, DEVICED_INTERFACE_ODE,
			SIGNAL_REMOVE_MMC, NULL, NULL);
	return r;
}

int ode_mmc_inserted(void)
{
	int op;

	op = ode_judge_state();
	switch (op) {
	case ENCRYPTED_DEVICE_ENCRYPTED_MMC:
	case DECRYPTED_DEVICE_ENCRYPTED_MMC:
		vconf_set_str(VCONFKEY_SDE_CRYPTO_STATE, "encrypted");
		break;
	case ENCRYPT_ONGOING:
		break;
	default:
		vconf_set_str(VCONFKEY_SDE_CRYPTO_STATE, "unencrypted");
		break;
	}

	if (op != DECRPYTED_DEVICE_DECRYPTED_MMC) {
		ode_launch_app(op);
		return -1;
	}

	return 0;
}

static void ode_internal_init(void *data)
{
	/* init dbus interface */
	register_edbus_signal_handler(DEVICED_PATH_ODE, DEVICED_INTERFACE_ODE,
			SIGNAL_REQUEST_GENERAL_MOUNT,
			ode_request_general_mount);
	register_edbus_signal_handler(DEVICED_PATH_ODE, DEVICED_INTERFACE_ODE,
			SIGNAL_REQUEST_ODE_MOUNT,
			ode_request_ode_mount);
	register_edbus_signal_handler(DEVICED_PATH_ODE, DEVICED_INTERFACE_ODE,
			SIGNAL_REQUEST_REMOVE_ERROR_NOTI,
			ode_request_remove_error_noti);


	/* register vconf callback */
	vconf_notify_key_changed(VCONFKEY_SDE_CRYPTO_STATE,
			ode_crypto_state_cb, NULL);
	vconf_notify_key_changed(VCONFKEY_SDE_ENCRYPT_PROGRESS,
			ode_crypto_progress_cb, NULL);
}

static void ode_internal_exit(void *data)
{
	/* unregister vconf callback */
	vconf_ignore_key_changed(VCONFKEY_SDE_CRYPTO_STATE,
			ode_crypto_state_cb);
	vconf_ignore_key_changed(VCONFKEY_SDE_ENCRYPT_PROGRESS,
			ode_crypto_progress_cb);
}

static const struct device_ops ode_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "ode",
	.init     = ode_internal_init,
	.exit     = ode_internal_exit,
};

DEVICE_OPS_REGISTER(&ode_device_ops)
