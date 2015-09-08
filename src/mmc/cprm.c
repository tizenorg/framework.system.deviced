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


#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "mmc-handler.h"
#include "core/edbus-handler.h"

#define CPRM_E_BADARG		-2	/* Bad argments */
#define CPRM_E_FAIL		-1	/* General error */
#define CPRM_E_SUCCESS		0	/* Success */

#define SETRESP(x)	(x << 11)
#define R1RESP		SETRESP(1)			/* R1 response command */
#define ACMD		0x0400				/* Is ACMD */
#define DT		0x8000				/* With data */
#define DIR_IN		0x0000				/* Data transfer read */
#define DIR_OUT	0x4000				/* Data transfer write */

#define ACMD18		(18+R1RESP+ACMD+DT+DIR_IN)	/* Secure Read Multi Block */
#define ACMD25		(25+R1RESP+ACMD+DT+DIR_OUT)	/* Secure Write Multiple Block */
#define ACMD43		(43+R1RESP+ACMD+DT+DIR_IN)	/* Get MKB */
#define ACMD44		(44+R1RESP+ACMD+DT+DIR_IN)	/* Get MID */
#define ACMD45		(45+R1RESP+ACMD+DT+DIR_OUT)	/* Set CER RN1 */
#define ACMD46		(46+R1RESP+ACMD+DT+DIR_IN)	/* Get CER RN2 */
#define ACMD47		(47+R1RESP+ACMD+DT+DIR_OUT)	/* Set CER RES2 */
#define ACMD48		(48+R1RESP+ACMD+DT+DIR_IN)	/* Get CER RES1 */

#define MMC_IOCTL_BASE	0xB3		/* Same as MMC block device major number */
#define MMC_IOCTL_SET_RETRY_AKE_PROCESS	_IOR(MMC_IOCTL_BASE, 104, int)

struct cprm_request {
	unsigned int	cmd;
	unsigned long	arg;
	unsigned int	*buff;
	unsigned int	len;
};

static int cprm_fd;

static DBusMessage *edbus_cprm_init(E_DBus_Object *obj, DBusMessage *msg)
{

	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret = CPRM_E_SUCCESS;
	char path[NAME_MAX];

	ret = get_mmc_devpath(path);
	if (ret < 0) {
		ret = CPRM_E_SUCCESS;
		_E("fail to check mmc");
		if (cprm_fd)
			close(cprm_fd);
		goto out;
	}
	_I("devpath :%s", path);
	cprm_fd = open(path, O_RDWR);
	if (cprm_fd < 0)
		_E("fail to open");
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_cprm_terminate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret = CPRM_E_SUCCESS;

	close(cprm_fd);
	_I("cprm is terminated");
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_cprm_sendcmd(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter, arr;
	DBusMessage *reply;
	DBusError err;
	int ret = CPRM_E_BADARG;
	int result = CPRM_E_FAIL;
	int slot;
	struct cprm_request cprm_req;
	char *data;
	char path[NAME_MAX];

	memset(&cprm_req, 0, sizeof(struct cprm_request));
	ret = get_mmc_devpath(path);
	if (ret < 0) {
		_E("fail to check mmc");
		ret = CPRM_E_FAIL;
		if (cprm_fd)
			close(cprm_fd);
		goto out;
	}

	if (cprm_fd <= 0) {
		_E("fail to init");
		goto out;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err,
			DBUS_TYPE_INT32, &slot,
			DBUS_TYPE_INT32, &cprm_req.cmd,
			DBUS_TYPE_UINT32, &cprm_req.arg,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &cprm_req.len,
			DBUS_TYPE_INVALID);

	if (!ret) {
		_E("there is no message");
		ret = CPRM_E_BADARG;
		goto out;
	}

	if (cprm_req.len <= 0) {
		_E("fail to check length %d", cprm_req.len);
		goto out;
	}

	if ((cprm_req.cmd == ACMD43) || (cprm_req.cmd == ACMD18) || (cprm_req.cmd == ACMD25))
		ret = ((cprm_req.len % 512) != 0) ? CPRM_E_BADARG : CPRM_E_SUCCESS;
	else if ((cprm_req.cmd == ACMD44) || (cprm_req.cmd == ACMD45) || (cprm_req.cmd == ACMD46) ||
	    (cprm_req.cmd == ACMD47) || (cprm_req.cmd == ACMD48))
		ret = (cprm_req.len != 8) ? CPRM_E_BADARG : CPRM_E_SUCCESS;
	else
		ret = CPRM_E_BADARG;

	if (ret != CPRM_E_SUCCESS) {
			_E("fail to check param cmd %d. Invalid arguments..", (cprm_req.cmd & 0x3f));
			goto out;
	}

	cprm_req.buff = (unsigned int *)data;

	result = ioctl(cprm_fd, cprm_req.cmd, &cprm_req);

	if (result != CPRM_E_SUCCESS) {
		_E("cprm_drvr_sendcmd: STUB_SENDCMD() return fail! result:%d", result);
		ret = CPRM_E_FAIL;
	} else {
		ret = CPRM_E_SUCCESS;
	}
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arr);
	dbus_message_iter_append_fixed_array(&arr, DBUS_TYPE_BYTE, &data, cprm_req.len);
	dbus_message_iter_close_container(&iter, &arr);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_cprm_getslotstat(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned long ret = 0xFFFFFFFF;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &ret);
	return reply;
}

static DBusMessage *edbus_cprm_retry(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = CPRM_E_BADARG;
	char path[NAME_MAX];

	ret = get_mmc_devpath(path);

	if (ret < 0) {
		_E("[cprm_drvr_retry] fail, so close cprm handle");
		ret = CPRM_E_FAIL;
		if (cprm_fd)
			close(cprm_fd);
		goto out;
	}

	ret = ioctl(cprm_fd, MMC_IOCTL_SET_RETRY_AKE_PROCESS, NULL);

	if (ret != CPRM_E_SUCCESS) {
		_E("fail to retry :%d", ret);
		ret = CPRM_E_FAIL;
	} else {
		ret = CPRM_E_SUCCESS;
	}
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "CprmInit",		NULL,	"i",	edbus_cprm_init},
	{ "CprmTerminate",	NULL,	"i",	edbus_cprm_terminate},
	{ "CprmSendcmd",	"iiuay","ayi",	edbus_cprm_sendcmd},
	{ "CprmGetslotstat",	NULL,	"u",	edbus_cprm_getslotstat},
	{ "CprmRetry",		NULL,	"i",	edbus_cprm_retry},
};

static void cprm_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_MMC, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

const struct device_ops cprm_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "cprm",
	.init     = cprm_init,
};

DEVICE_OPS_REGISTER(&cprm_device_ops)
