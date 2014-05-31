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

#include <vconf.h>
#include <stdbool.h>
#include "core/log.h"
#include "core/devices.h"
#include "core/launch.h"
#include "usb-client.h"

#define BUF_MAX 256

#define TICKER_TYPE_DEFAULT "usb-client-default"
#define TICKER_TYPE_SSH     "usb-client-ssh"

#ifdef TIZEN_ENGINEER_MODE
const static bool eng_mode = true;
#else
const static bool eng_mode = false;
#endif

struct ticker_data {
	char *name;
	int type;
};

static int debug = 0;

static int write_sysfs(char *path, char *value)
{
	FILE *fp;
	int ret;

	if (!path || !value)
		return -ENOMEM;

	fp = fopen(path, "w");
	if (!fp) {
		_E("FAIL: fopen(%s)", path);
		return -ENOMEM;
	}

	ret = fwrite(value, sizeof(char), strlen(value), fp);
	if (ret < strlen(value)) {
		_E("FAIL: fwrite(%s)", value);
		ret = -ENOMEM;
		goto out;
	}

out:
	if (fclose(fp) != 0)
		_E("FAIL: fclose()");
	return ret;
}

static int set_configurations_to_sysfs(dd_list *list)
{
	dd_list *l;
	struct xmlConfiguration *conf;
	int ret;

	if (!list)
		return -EINVAL;

	DD_LIST_FOREACH(list, l, conf) {
		if (!(conf->value))
			continue;

		ret = write_sysfs(conf->path, conf->value);
		if (ret < 0) {
			_E("FAIL: write_sysfs(%s, %s)", conf->path, conf->value);
			return ret;
		}
	}
	return 0;
}

static void run_operations_for_usb_mode(dd_list *list)
{
	dd_list *l;
	int ret;
	struct xmlOperation *oper;

	if (!list)
		return ;

	 DD_LIST_FOREACH(list, l, oper) {
		 ret = launch_app_cmd(oper->oper);
		 _I("operation: %s(%d)", oper->oper, ret);
	 }
}

void unset_client_mode(int mode, bool change)
{
	int ret;
	dd_list *conf_list;
	dd_list *oper_list;

	if (!change)
		update_current_usb_mode(SET_USB_NONE);

	if (update_usb_state(VCONFKEY_SYSMAN_USB_DISCONNECTED) < 0)
		_E("FAIL: update_usb_state(%d)", VCONFKEY_SYSMAN_USB_DISCONNECTED);

	ret = make_configuration_list(SET_USB_NONE);
	if (ret == 0) {
		ret = get_configurations_list(&conf_list);
		if (ret == 0) {
			ret = set_configurations_to_sysfs(conf_list);
			if (ret < 0)
				_E("FAIL: set_configurations_to_sysfs()");
		}
	}

	ret = make_operation_list(mode, USB_CON_UNSET);
	if (ret == 0) {
		ret = get_operations_list(&oper_list);
		if (ret == 0)
			run_operations_for_usb_mode(oper_list);
	}

	release_operations_list();
}

static void launch_ticker_notification(int cur_mode, int sel_mode)
{
	struct ticker_data ticker;
	const struct device_ops *ticker_ops;

	switch(sel_mode) {
	case SET_USB_DEFAULT:
	case SET_USB_SDB:
	case SET_USB_SDB_DIAG:
		if (cur_mode == SET_USB_DEFAULT
				|| cur_mode == SET_USB_SDB
				|| cur_mode == SET_USB_SDB_DIAG)
			return;

		ticker.name = TICKER_TYPE_DEFAULT;
		ticker.type = 0; /* WITHOUT_QUEUE */
		break;
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_DIAG:
	case SET_USB_RNDIS_SDB:
		if (cur_mode == SET_USB_RNDIS
				|| cur_mode == SET_USB_RNDIS_TETHERING
				|| cur_mode == SET_USB_RNDIS_DIAG
				|| cur_mode == SET_USB_RNDIS_SDB)
			return;

		ticker.name = TICKER_TYPE_SSH;
		ticker.type = 0; /* WITHOUT_QUEUE */
		break;
	case SET_USB_NONE:
	case SET_USB_RNDIS_TETHERING:
	default:
		return;
	}

	ticker_ops = find_device("ticker");

	if (get_cradle_status() > 0)
		return;

	if (ticker_ops && ticker_ops->init)
		ticker_ops->init(&ticker);
	else
		_E("cannot find \"ticker\" ops");
}

static int get_selected_mode_by_debug_mode(int mode)
{
	int ret;

	debug = get_debug_mode();

	switch (mode) {
	case SET_USB_DEFAULT:
		if (debug == 1) /* debug on */
			mode = SET_USB_SDB;
		break;
	case SET_USB_SDB:
	case SET_USB_SDB_DIAG:
		if (debug == 0) /* debug off */
			mode = SET_USB_DEFAULT;
		break;
	default:
		break;
	}

	return mode;
}

static bool get_usb_tethering_state(void)
{
	int state;
	int ret;

	if (vconf_get_int(VCONFKEY_MOBILE_HOTSPOT_MODE, &state) == 0
			&& (state & VCONFKEY_MOBILE_HOTSPOT_MODE_USB)) {
		_I("USB tethering is on");
		return true;
	}

	_I("USB tethering is off");
	return false;
}

static bool check_usb_tethering(int sel_mode)
{
	bool state;

	state = get_usb_tethering_state();

	if (state == false)
		return state;

	switch (sel_mode) {
	case SET_USB_RNDIS_TETHERING:
	case SET_USB_RNDIS:
		return false;
	default:
		break;
	}

	if (change_selected_usb_mode(SET_USB_RNDIS_TETHERING) != 0) {
		_E("Failed to set usb selected mode (%d)", SET_USB_RNDIS_TETHERING);
		return false;
	}

	return true;
}

static int turn_on_debug(void)
{
	debug = 1;
	return vconf_set_bool(VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL, debug);
}

static int check_first_eng_mode(int sel_mode)
{
	static bool first = true;

	if (!eng_mode || !first)
		return sel_mode;

	first = false;

	if (sel_mode == SET_USB_DEFAULT) {
		sel_mode = SET_USB_SDB;
		if (turn_on_debug() != 0)
			_E("Failed to turn on debug toggle");
	}

	return sel_mode;
}

static int decide_selected_mode(int sel_mode, int cur_mode)
{
	int mode;

	if (check_usb_tethering(sel_mode))
		return -ECANCELED;

	mode = check_first_eng_mode(sel_mode);

	mode = get_selected_mode_by_debug_mode(mode);

	if (mode == cur_mode) {
		_I("Selected usb mode (%d) is same with current usb mode (%d)", mode, cur_mode);
		return -ECANCELED;
	}

	_I("Selected mode decided is (%d)", mode);

	return mode;
}

void change_client_setting(int options)
{
	int sel_mode;
	int cur_mode;
	bool tethering;
	int ret;
	char *action;
	dd_list *conf_list;
	dd_list *oper_list;

	if (control_status() == DEVICE_OPS_STATUS_STOP) {
		launch_syspopup(USB_RESTRICT);
		return;
	}

	sel_mode = get_selected_usb_mode();
	cur_mode = get_current_usb_mode();

	sel_mode = decide_selected_mode(sel_mode, cur_mode);
	if (sel_mode == -ECANCELED)
		return;
	else if (sel_mode <= 0) {
		_E("Failed to get selected mode");
		return;
	}

	if (options & SET_CONFIGURATION) {
		if (cur_mode != SET_USB_NONE) {
			unset_client_mode(cur_mode, true);
		}

		ret = make_configuration_list(sel_mode);
		if (ret < 0) {
			_E("FAIL: make_configuration_list(%d)", sel_mode);
			goto out_configuration;
		}

		ret = get_configurations_list(&conf_list);
		if (ret < 0) {
			_E("failed to get configuration list");
			goto out_configuration;
		}

		ret = set_configurations_to_sysfs(conf_list);
		if (ret < 0) {
			_E("FAIL: set_configurations_to_sysfs()");
			goto out_configuration;
		}
	}

	if (options & SET_OPERATION) {
		ret = make_operation_list(sel_mode, USB_CON_SET);
		if (ret < 0) {
			_E("FAIL: make_operation_list()");
			goto out_operation;
		}

		ret = get_operations_list(&oper_list);
		if (ret < 0) {
			_E("failed to get operation list");
			goto out_operation;
		}

		if (update_usb_state(VCONFKEY_SYSMAN_USB_AVAILABLE) < 0)
			_E("FAIL: update_usb_state(%d)", VCONFKEY_SYSMAN_USB_AVAILABLE);

		update_current_usb_mode(sel_mode);

		run_operations_for_usb_mode(oper_list);
	}

	if (options & SET_NOTIFICATION) {
		launch_ticker_notification(cur_mode, sel_mode);
	}

	ret = 0;

out_operation:
	release_operations_list();

out_configuration:
	if (make_configuration_list(SET_USB_NONE) < 0)
		_E("Release configurations info error");

	if (ret < 0)
		launch_syspopup(USB_ERROR);

	return;
}

void client_mode_changed(keynode_t* key, void *data)
{
	int ret;

	if (get_wait_configured())
		return;

	change_client_setting(SET_CONFIGURATION | SET_OPERATION | SET_NOTIFICATION);
}

#if 0
void client_mode_changed(keynode_t* key, void *data)
{
	int sel_mode;
	int cur_mode;
	int ret;
	char *action;
	dd_list *conf_list;
	dd_list *oper_list;

	if (control_status() == DEVICE_OPS_STATUS_STOP) {
		launch_syspopup(USB_RESTRICT);
		return;
	}

	sel_mode = get_selected_usb_mode();
	if (sel_mode <= SET_USB_NONE) {
		_E("Getting selected usb mode error(%d)", sel_mode);
		return;
	}

	if (check_usb_tethering()) {
		_I("Turning on the usb tethering");
		return;
	}

	ret = check_debug_mode(sel_mode);
	if (ret != 0)
		return;

	sel_mode = check_first_eng_mode(sel_mode);

	cur_mode = get_current_usb_mode();
	if (cur_mode < SET_USB_NONE) {
		_E("Getting current usb mode error(%d)", cur_mode);
		return ;
	}

	if (cur_mode == sel_mode) {
		_I("Current usb mode(%d) is same with selected usb mode(%d)", cur_mode, sel_mode);
		return ;
	}

	if (cur_mode != SET_USB_NONE) {
		unset_client_mode(cur_mode);
	}

	ret = make_configuration_list(sel_mode);
	if (ret < 0) {
		_E("FAIL: make_configuration_list(%d)", sel_mode);
		goto out;
	}

	ret = make_operation_list(sel_mode, USB_CON_SET);
	if (ret < 0) {
		_E("FAIL: make_operation_list()");
		goto out;
	}

	ret = get_configurations_list(&conf_list);
	if (ret < 0) {
		_E("failed to get configuration list");
		goto out;
	}

	ret = get_operations_list(&oper_list);
	if (ret < 0) {
		_E("failed to get operation list");
		goto out;
	}

	ret = set_configurations_to_sysfs(conf_list);
	if (ret < 0) {
		_E("FAIL: set_configurations_to_sysfs()");
		goto out;
	}

	update_current_usb_mode(sel_mode);

	run_operations_for_usb_mode(oper_list);

	launch_ticker_notification(cur_mode, sel_mode);

	ret = 0;

out:
	if (ret < 0)
		launch_syspopup(USB_ERROR);

	release_operations_list();
	ret = make_configuration_list(SET_USB_NONE);
	if (ret < 0)
		_E("release configurations info error");
	return;
}
#endif

void debug_mode_changed(keynode_t* key, void *data)
{
	int new_debug;
	int cur_mode;
	int sel_mode;

	if (control_status() == DEVICE_OPS_STATUS_STOP)
		return;

	new_debug = get_debug_mode();
	_I("old debug(%d), new debug(%d)", debug, new_debug);
	if (debug == new_debug)
		return;

	cur_mode = get_current_usb_mode();
	_I("cur_mode(%d)", cur_mode);

	switch (cur_mode) {
	case SET_USB_DEFAULT:
	case SET_USB_SDB:
	case SET_USB_SDB_DIAG:
		if (new_debug == 0)
			sel_mode = SET_USB_DEFAULT;
		else
			sel_mode = SET_USB_SDB;
		break;
	default:
		return ;
	}

	if (change_selected_usb_mode(sel_mode) != 0)
		_E("FAIL: change_selected_usb_mode(%d)", sel_mode);

	return;
}

/* USB tethering */
static int turn_on_usb_tethering(void)
{
	int cur_mode;
	int sel_mode;
	int ret;

	cur_mode = get_current_usb_mode();
	sel_mode = get_selected_usb_mode();

	switch (cur_mode) {
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_TETHERING:
		return 0;
	default:
		break;
	}

	switch (sel_mode) {
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_TETHERING:
		break;
	default:
		sel_mode = SET_USB_RNDIS_TETHERING;
		break;
	}

	ret = change_selected_usb_mode(sel_mode);
	if (ret != 0)
		_E("FAIL: change_selected_usb_mode(%d)", sel_mode);

	return ret;
}

static int turn_off_usb_tethering(void)
{
	int cur_mode;
	int sel_mode;
	int ret;

	cur_mode = get_current_usb_mode();

	switch(cur_mode) {
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_TETHERING:
		sel_mode = get_default_mode();
		ret = change_selected_usb_mode(sel_mode);
		if (ret != 0)
			_E("FAIL: change_selected_usb_mode(%d)", sel_mode);
		return ret;

	default:
		return 0;
	}
}

void tethering_status_changed(keynode_t* key, void *data)
{
	bool usb_tethering;
	int ret;

	if (control_status() == DEVICE_OPS_STATUS_STOP)
		return;

	usb_tethering = get_usb_tethering_state();
	if (usb_tethering)
		ret = turn_on_usb_tethering();
	else
		ret = turn_off_usb_tethering();

	if (ret != 0)
		_E("Failed to change tethering mode");

	return;
}
