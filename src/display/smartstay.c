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


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "core/common.h"
#include "core/device-handler.h"

#define SMART_STAY	2
#define SMART_DETECTION_LIB "/usr/lib/libsmart_detection.so"
#define CB_TIMEOUT	3 /* seconds */
#define OCCUPIED_FAIL	-2

typedef void (*detection_cb)(int result, void *data);

static void *detect_handle = NULL;
static int (*get_detection)(detection_cb callback, int type, void* user_data1, void* user_data2);
static bool block_state = EINA_FALSE;
static bool cb_state = EINA_FALSE;
static Ecore_Timer *cb_timeout_id = NULL;

static void init_smart_lib(void)
{
	detect_handle = dlopen(SMART_DETECTION_LIB, RTLD_LAZY);

	if (!detect_handle) {
		_E("dlopen error");
	} else {
		get_detection = (int (*)(detection_cb, int, void *, void *))
		    dlsym(detect_handle, "get_smart_detection");

		if (!get_detection) {
			_E("dl_sym fail");
			dlclose(detect_handle);
			detect_handle = NULL;
		}
	}
}

static Eina_Bool check_cb_state(void *data)
{
	struct state *st;
	int next_state;
	int user_data = (int)(data);

	cb_timeout_id = NULL;

	if (cb_state) {
		_I("smart detection cb is already working!");
		return ECORE_CALLBACK_CANCEL;
	}

	if (block_state) {
		_I("input event occur, smart detection is ignored!");
		block_state = EINA_FALSE;
		return ECORE_CALLBACK_CANCEL;
	}

	if (user_data < S_START || user_data > S_LCDOFF) {
		_E("next state is wrong [%d], set to [%d]",
		    user_data, S_NORMAL);
		user_data = S_NORMAL;
	}

	_I("smart detection cb is failed, goes to [%d]", user_data);

	next_state = user_data;
	pm_old_state = pm_cur_state;
	pm_cur_state = next_state;

	st = &states[pm_cur_state];
	if (st->action)
		st->action(st->timeout);

	if (pm_cur_state == S_LCDOFF)
		update_lcdoff_source(VCONFKEY_PM_LCDOFF_BY_TIMEOUT);

	return ECORE_CALLBACK_CANCEL;
}

static void detection_callback(int degree, void *data)
{
	struct state *st;
	int next_state;
	int user_data = (int)(data);

	cb_state = EINA_TRUE;

	if (block_state) {
		_I("input event occur, face detection is ignored!");
		block_state = EINA_FALSE;
		return;
	}

	if (!get_hallic_open()) {
		_I("hallic is closed! Skip detection logic!");
		return;
	}

	_I("degree = [%d], user_data = [%d]", degree, user_data);

	switch (degree)
	{
	case 270:
	case 90:
	case 180:
	case 0:
		_I("face detection success, goes to [%d]",
		    S_NORMAL);
		pm_old_state = pm_cur_state;
		pm_cur_state = S_NORMAL;
		break;
	case OCCUPIED_FAIL:
		_I("camera is occupied, stay current state");
		break;
	default:
		if (user_data < S_START || user_data > S_LCDOFF) {
			_E("next state is wrong [%d], set to [%d]",
			    user_data, S_NORMAL);
			user_data = S_NORMAL;
		}

		next_state = user_data;

		pm_old_state = pm_cur_state;
		pm_cur_state = next_state;

		if (check_lcdoff_direct() == true)
			pm_cur_state = S_LCDOFF;

		_I("smart detection is failed, goto [%d]state", pm_cur_state);

		break;
	}

	st = &states[pm_cur_state];
	if (st->action)
		st->action(st->timeout);

	if (pm_cur_state == S_LCDOFF)
		update_lcdoff_source(VCONFKEY_PM_LCDOFF_BY_TIMEOUT);
}

static int check_face_detection(int evt, int pm_cur_state, int next_state)
{
	int lock_state = EINA_FALSE;
	int state;

	if (cb_timeout_id) {
		ecore_timer_del(cb_timeout_id);
		cb_timeout_id = NULL;
	}

	if (evt == EVENT_INPUT) {
		block_state = EINA_TRUE;
		return EINA_FALSE;
	}
	block_state = EINA_FALSE;

	if (evt != EVENT_TIMEOUT)
		return EINA_FALSE;

	if (pm_cur_state != S_NORMAL)
		return EINA_FALSE;

	if (!get_detection) {
		init_smart_lib();
		if (!get_detection) {
			_E("%s load failed!", SMART_DETECTION_LIB);
			return EINA_FALSE;
		}
	}

	state = get_detection(detection_callback, SMART_STAY, NULL, (void*)next_state);

	if (state != 0)
		_E("get detection FAIL [%d]", state);
	else
		_I("get detection success");

	cb_state = EINA_FALSE;
	cb_timeout_id = ecore_timer_add(CB_TIMEOUT,
			    (Ecore_Task_Cb)check_cb_state, (void*)next_state);
	return EINA_TRUE;
}

static void smartstay_init(void *data)
{
	display_info.face_detection = check_face_detection;
}

static void smartstay_exit(void *data)
{
	if (detect_handle) {
		dlclose(detect_handle);
		detect_handle = NULL;
		_D("detect handle is closed!");
	}
	get_detection = NULL;
	display_info.face_detection = NULL;
}

static const struct display_ops display_smartstay_ops = {
	.name     = "smartstay",
	.init     = smartstay_init,
	.exit     = smartstay_exit,
};

DISPLAY_OPS_REGISTER(&display_smartstay_ops)

