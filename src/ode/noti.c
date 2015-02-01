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

#include "core/log.h"
#include "core/edbus-handler.h"
#include "noti.h"

#define METHOD_COMPLETE_NOTI_ON		"CompNotiOn"
#define METHOD_PROGRESS_NOTI_ON		"ProgNotiOn"
#define METHOD_PROGRESS_NOTI_OFF	"ProgNotiOff"
#define METHOD_PROGRESS_NOTI_UPDATE	"ProgNotiUpdate"
#define METHOD_ERROR_NOTI_ON		"ErrorNotiOn"
#define METHOD_ERROR_NOTI_OFF		"ErrorNotiOff"

#define RETRY_CNT	3

static const char *ode_type_str[] = {
	[ENCRYPT_TYPE] = "encrypt",
	[DECRYPT_TYPE] = "decrypt",
};

static int progress_h;
static int error_h;

static int method_noti(const char *method, const char *sig, char *param[])
{
	int ret, retry = RETRY_CNT;

	while (retry--) {
		ret = dbus_method_sync(POPUP_BUS_NAME,
				POPUP_PATH_ODE,
				POPUP_INTERFACE_ODE,
				method,
				sig, param);
		if (ret > 0)
			break;
	}

	return ret;
}

int noti_progress_show(int type)
{
	char str_type[32];
	char *arr[1];
	int ret;

	/* If ongoing noti already exists, delete the noti */
	if (progress_h > 0)
		noti_progress_clear();

	/* If error noti already exists, delete the noti */
	if (error_h > 0)
		noti_error_clear();

	snprintf(str_type, sizeof(str_type), "%s", ode_type_str[type]);
	arr[0] = str_type;

	ret = method_noti(METHOD_PROGRESS_NOTI_ON, "s", arr);
	if (ret < 0)
		return ret;

	progress_h = ret;
	_D("insert progress noti handle : %d", progress_h);
	return 0;
}

int noti_progress_clear(void)
{
	char str_h[32];
	char *arr[1];
	int ret;

	if (progress_h <= 0) {
		_D("already ongoing noti clear");
		return 0;
	}

	snprintf(str_h, sizeof(str_h), "%d", progress_h);
	arr[0] = str_h;

	ret = method_noti(METHOD_PROGRESS_NOTI_OFF, "i", arr);
	if (ret != 0)
		return ret;

	_D("delete progress noti handle : %d", progress_h);
	progress_h = 0;
	return 0;
}

int noti_progress_update(int rate)
{
	char str_h[32];
	char str_r[32];
	char *arr[2];

	if (progress_h <= 0) {
		_E("need to create notification");
		return -ENOENT;
	}

	if (rate < 0 || rate > 100) {
		_E("Invalid parameter");
		return -EINVAL;
	}

	snprintf(str_h, sizeof(str_h), "%d", progress_h);
	arr[0] = str_h;
	snprintf(str_r, sizeof(str_r), "%d", rate);
	arr[1] = str_r;

	return method_noti(METHOD_PROGRESS_NOTI_UPDATE, "ii", arr);
}

int noti_finish_show(int type)
{
	char str_type[32];
	char *arr[1];
	int ret;

	snprintf(str_type, sizeof(str_type), "%s", ode_type_str[type]);
	arr[0] = str_type;

	return method_noti(METHOD_COMPLETE_NOTI_ON, "s", arr);
}

int noti_error_show(int type, int state, int val)
{
	char str_type[32], str_state[32], str_val[32];
	char *arr[3];
	int ret;

	/* If ongoing noti already exists, delete the noti */
	if (error_h > 0)
		noti_error_clear();

	snprintf(str_type, sizeof(str_type), "%s", ode_type_str[type]);
	arr[0] = str_type;
	snprintf(str_state, sizeof(str_state), "%d", state);
	arr[1] = str_state;
	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[2] = str_val;

	ret = method_noti(METHOD_ERROR_NOTI_ON, "sii", arr);
	if (ret < 0)
		return ret;

	error_h = ret;
	_D("insert error noti handle : %d", error_h);
	return 0;
}

int noti_error_clear(void)
{
	char str_h[32];
	char *arr[1];
	int ret;

	if (error_h <= 0) {
		_D("already ongoing noti clear");
		return 0;
	}

	snprintf(str_h, sizeof(str_h), "%d", error_h);
	arr[0] = str_h;

	ret = method_noti(METHOD_ERROR_NOTI_OFF, "i", arr);
	if (ret != 0)
		return ret;

	_D("delete error noti handle : %d", error_h);
	error_h = 0;
	return 0;
}
