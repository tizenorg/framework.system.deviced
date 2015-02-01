/*
 * LKM in TIMA(TZ based Integrity Measurement Architecture)
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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
#include <assert.h>
#include <limits.h>
#include <libudev.h>
#include <Ecore.h>
#include <QSEEComAPI.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/udev.h"

#include "noti.h"

/*
 * tzapp interface - commands for tima tzapp
 */
#define TIMA_PKM_ID		0x00070000
#define TIMA_MEASURE_REG_PROC	(TIMA_PKM_ID | 0x00)
#define TIMA_MEASURE_PROC	(TIMA_PKM_ID | 0x01)
#define TIMA_MEASURE_KERNEL	(TIMA_PKM_ID | 0x02)
#define TIMA_MEASURE_UNREG_PROC	(TIMA_PKM_ID | 0x03)
#define TIMA_MEASURE_VERIFY	(TIMA_PKM_ID | 0x04)
#define TIMA_MEASURE_KERNEL_ONDEMAND (TIMA_PKM_ID | 0x05)
#define TIMA_SECURE_LOG		(TIMA_PKM_ID | 0x06)
#define TIMA_INIT		(TIMA_PKM_ID | 0x07)

/* return value from tzapp(lkmauth) */
#define PKM_MODIFIED		-1
#define PKM_SUCCESS		0
#define PKM_ERROR		1
#define PKM_META_MODIFIED	2
#define PKM_INIT_FAIL		3
#define PKM_INIT_SUCCESS	4

struct tima_gen_req {
	int cmd_id;
	union {
		unsigned int buf_size;
		struct {
			int log_region;		/* 0: Page Table, 1: Periodic TIMA & LKM */
			int log_entry_index;	/* 0: Log Meta Data, others: Log Entry Index */
		} __attribute__((packed)) log;
	} __attribute__((packed)) param;
} __attribute__((packed));

struct tima_measure_rsp_s {
	int cmd_id;
	int ret;
	union {
		unsigned char hash[20];
		char result_ondemand[4096];
	}  __attribute__((packed)) result;
} __attribute__((packed));

/*
 * QSEECom interface
 */
#define FIRMWARE_PATH		"/lib/firmware"

struct QSEECom_handle *qseecom_handle = NULL;

/*
 * ECore interface
 */
#define TIMA_TIMER_START	(5)
#define TIMA_TIMER_INTERVAL	(5 * 60)

static Ecore_Timer *timer_id;

/*
 * kernel signature file handle
 */
#define KERN_HASH_FILE		"/usr/system/kern_sec_info"

static int kern_info_fd = -1;
static unsigned int kern_info_len;

static int noti_id = 0;


/*
 * tima_shutdown_tzapp
 *
 * This function is written to temporarily bypass a problem in QSEECOM
 * driver. In ideal situations, this fuction should not be needed.
 *
 * We are running into the -22 error, which stops the tima tzapp from
 * working after it occurs. This function is used to shut down the tima
 * tzapp every time when a periodic measurement is done.
 * When the -22 error is fixed, this function shouldn't be needed.
 */
static int tima_shutdown_tzapp(void)
{
	int ret = -1;

	if ((qseecom_handle != NULL) &&
	    (QSEECom_app_load_query(qseecom_handle, "tima") == QSEECOM_APP_ALREADY_LOADED)) {
		ret = QSEECom_shutdown_app(&qseecom_handle);
		if (!ret) {
			qseecom_handle = NULL;
			_D("tzapp is shut down!");
		} else {
			_E("Unable to shut down the TIMA tzapp; ret(%d)", errno);
		}
	} else {
		_D("tzapp is not loaded! nothing to shut down.");
	}

	return ret;
}

/*
 * tima_send_initcmd
 *
 * This function is written explictly to tell the TIMA periodic
 * Trustzone application to accept the passed KERN_HASH_FILE
 * via the buffer. We saw a lot of issues with the listener
 * service, when the TIMA application was explicitly calling
 * the filesystem API's to load this file on the TZ side
 */
static int tima_send_initcmd(uint32_t req_len, struct tima_measure_rsp_s *recv_cmd)
{
	int ret = -1;
	void *file_ptr = NULL;
	struct tima_gen_req *req;
	struct tima_measure_rsp_s *rsp;
	uint32_t rsp_len = 0;

	_D("signature load request\n");

	if ((kern_info_fd < 0) || (kern_info_len == 0)) {
		_E("%s not loaded", KERN_HASH_FILE);
		return ret;
	}

	rsp_len = sizeof(struct tima_measure_rsp_s);

	req = (struct tima_gen_req *)qseecom_handle->ion_sbuffer;
	req->cmd_id = TIMA_INIT;
	req->param.buf_size = kern_info_len;
	file_ptr = (void *)((char *)req + sizeof(struct tima_gen_req));

	if (read(kern_info_fd, file_ptr, kern_info_len) != kern_info_len) {
		_E("cann't load signature, ret(%d)", errno);
		return ret;
	}

	rsp = (struct tima_measure_rsp_s *)(qseecom_handle->ion_sbuffer + req_len);

	_D("req: cmd(0x%08X), size(%d)@%p", req->cmd_id, req_len, req);

	QSEECom_set_bandwidth(qseecom_handle, true);
	do {
		ret = QSEECom_send_cmd(qseecom_handle, req, req_len, rsp, rsp_len);
		if (ret) {
			_E("QSEECom_send_cmd failed, ret(%d)", ret);
			break;
		} else if ((rsp->ret != 0) && (rsp->ret != 512)) {
			_E("internal error in tima tzapp, ret(%d)", rsp->ret);
			ret = -1;
			break;
		}
	} while(rsp->ret == 512);
	QSEECom_set_bandwidth(qseecom_handle, false);

	_D("rsp: cmd(0x%08x), ret(%d), msg(%s)", rsp->cmd_id, rsp->ret, rsp->result.result_ondemand);

	return ret;
}

static int tima_load_tzapp(struct tima_measure_rsp_s *recv_cmd)
{
	int ret = -1;
	uint32_t req_len, rsp_len;

	if ((qseecom_handle != NULL) &&
	    (QSEECom_app_load_query(qseecom_handle, "tima") == QSEECOM_APP_ALREADY_LOADED)) {
		recv_cmd->ret = 0;
		return 0;
	}

	kern_info_fd = open(KERN_HASH_FILE, O_RDONLY);
	if (kern_info_fd < 0) {
		_E("%s open failed, ret(%d)", KERN_HASH_FILE, errno);
		/*
		 * Simulate TZ response for kern metadata modified
		 * This is necessary for the notification to show up.
		 * I cringed while writing this code, so that makes the two of us.
		 */
		recv_cmd->ret = 2;
		snprintf(recv_cmd->result.result_ondemand, 256, "MSG=kern_metadata_modified;");
		/* Indicate load_app succeed, but open kern_kern_info fail */
		return 0;
	}

	/* determine the length of the kern_info file */
	lseek(kern_info_fd, 0, SEEK_END);
	kern_info_len = lseek(kern_info_fd, 0, SEEK_CUR);
	lseek(kern_info_fd, 0, SEEK_SET);

	req_len = kern_info_len + sizeof(struct tima_gen_req);
	rsp_len = sizeof(struct tima_measure_rsp_s);

	req_len = (req_len > rsp_len) ? req_len : rsp_len;

	if (req_len & QSEECOM_ALIGN_MASK)
		req_len = QSEECOM_ALIGN(req_len);

	_D("attempting to load tzapp");

	/* Load up the secure world counter-part */
	ret = QSEECom_start_app(&qseecom_handle, FIRMWARE_PATH, "tima", req_len * 2);
	if (!ret) {
		_I("tzapp(%s) successfully loaded\n", FIRMWARE_PATH "/tima");

		if ((ret = tima_send_initcmd(req_len, recv_cmd)) != 0) {
			_E("failed to send QSEE cmd (TIMA_INIT) to tzapp!");
			tima_shutdown_tzapp();
			ret = -1;
			goto error;
		} else {
			_D("signature successfully loaded");
		}
	} else {
		_E("failed to load the tzapp, ret(%d)", errno);
	}

error:
	close(kern_info_fd);
	kern_info_fd = -1;
	return ret;
}

static int tima_send_command(struct tima_measure_rsp_s *recv_cmd)
{
	int req_len = 0, rsp_len = 0;
	struct tima_gen_req *req;
	struct tima_measure_rsp_s *rsp;
	int ret = -1;

	req_len = sizeof(struct tima_gen_req);
	rsp_len = sizeof(struct tima_measure_rsp_s);
	if (req_len & QSEECOM_ALIGN_MASK)
		req_len = QSEECOM_ALIGN(req_len);

	if (rsp_len & QSEECOM_ALIGN_MASK)
		rsp_len = QSEECOM_ALIGN(rsp_len);

	req = (struct tima_gen_req *)qseecom_handle->ion_sbuffer;
	req->cmd_id = TIMA_MEASURE_KERNEL_ONDEMAND;
	rsp = (struct tima_measure_rsp_s *)(qseecom_handle->ion_sbuffer + req_len);

	_D("req: cmd(0x%08X), size(%d)@%p", req->cmd_id, req_len, req);

	QSEECom_set_bandwidth(qseecom_handle, true);
	do {
		ret = QSEECom_send_cmd(qseecom_handle, req, req_len, rsp, rsp_len);
		if (ret) {
			_E("QSEECom_send_cmd failed, ret(%d)", errno);
			break;
		}

		usleep(5000);
	} while (rsp->ret == 512);
	QSEECom_set_bandwidth(qseecom_handle, false);

	_D("rsp: cmd(0x%08x), ret(%d), msg(%s)", rsp->cmd_id, rsp->ret, rsp->result.result_ondemand);

	memcpy(recv_cmd, rsp, sizeof(struct tima_measure_rsp_s));

	return ret;
}

static int tima_check_event(void)
{
	int ret = -1;
	struct tima_measure_rsp_s recv_cmd;

	if (tima_load_tzapp(&recv_cmd)) {
		_E("Failed to load tzapp!");
		return -1;
	} else if (recv_cmd.ret != 0) {
		goto skip_measurement;
	}

	if (tima_send_command(&recv_cmd)) {
		_E("Failed to send QSEE cmd to tzapp!");
		/* The following call needs to be removed after fixing the -22 error problem */
		tima_shutdown_tzapp ();

		return -1;
	}

	/* The following call needs to be removed after fixing the -22 error problem */
	tima_shutdown_tzapp ();

	if ((recv_cmd.ret == 0) &&
	    (recv_cmd.cmd_id != TIMA_MEASURE_KERNEL_ONDEMAND)) {
		_E("Invalid QSEE result!");
		return -1;
	}

	switch (recv_cmd.ret) {
	case PKM_SUCCESS:
		_I("verify successed");
		ret = 0;
		break;
	case PKM_MODIFIED:
		_I("verify failed");
		ret = 1;
		break;
	case PKM_META_MODIFIED:
		_I("check metadata (%s)", KERN_HASH_FILE);
		ret = 1;
		break;
	default:
		_E("internal error (%d)", recv_cmd.ret);
		break;
	}

skip_measurement:
	return ret;
}

static Eina_Bool tima_pkm_monitor(void *data)
{
	int ret;
	static int is_first = 1;

	ret = tima_check_event();
	if (ret) {
		noti_id = tima_pkm_detection_noti_on();
		if (noti_id < 0) {
			_E("FAIL: tima_pkm_detection_noti_on()");
			noti_id = 0;
		}

		ecore_timer_del(timer_id);
		timer_id = NULL;
		return EINA_FALSE;
	}

	if (is_first) {
		is_first = 0;
		ecore_timer_interval_set(timer_id, TIMA_TIMER_INTERVAL);
	}

	return EINA_TRUE;
}

/* tima-pkm init funcion */
void tima_pkm_init(void)
{
	timer_id = ecore_timer_add(TIMA_TIMER_START, tima_pkm_monitor, NULL);
	if (!timer_id) {
		_E("can't add timer for tima-pkm");
		return;
	}
}

/* tima-pkm exit funcion */
void tima_pkm_exit(void)
{
	if (timer_id) {
		ecore_timer_del(timer_id);
		timer_id = NULL;
	}
}
