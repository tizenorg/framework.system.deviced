/*
 * test
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

#include "test.h"

#define S_ENTER	1
#define S_LEAVE	0

enum apps_enable_type {
	APPS_DISABLE = 0,
	APPS_ENABLE = 1,
};

#define SIGNAL_CHARGE_NOW "ChargeNow"
#define SIGNAL_CHARGER_STATUS "ChargerStatus"
#define SIGNAL_TEMP_GOOD "TempGood"

#define METHOD_LOWBAT_POPUP_DISABLE	"LowBatteryDisable"
#define METHOD_LOWBAT_POPUP_ENABLE	"LowBatteryEnable"

static E_DBus_Signal_Handler *edbus_charge_now_handler;
static E_DBus_Signal_Handler *edbus_charger_status_handler;
static E_DBus_Signal_Handler *edbus_temp_good_handler;
static E_DBus_Connection     *edbus_conn;

static struct power_supply_type {
	char *scenario;
	int status;
	char *capacity;
	char *charge_status;
	char *health;
	char *online;
	char *present;
	char *name;
} power_supply_types[] = {
	{"norm", S_ENTER, "100", "Charging",    "Good", "2", "1", "CHARGE"},
	{"norm", S_ENTER, "100", "Discharging", "Good", "1", "1", "DISCHARGE"},
	{"norm", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"heat1", S_ENTER, "100", "Discharging", "Overheat", "1", "1", NULL},
	{"heat1", S_ENTER, "100", "Not charging", "Overheat", "2", "1", "HEALTH(H) BEFORE CHARGE"},
	{"heat1", S_LEAVE, "100", "Discharging", "Overheat", "1", "1", "DISCHARGE"},
	{"heat1", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"heat2", S_ENTER, "100", "Charging",    "Good", "2", "1", NULL},
	{"heat2", S_ENTER, "100", "Not charging", "Overheat", "2", "1", "HEALTH(H) AFTER CHARGE"},
	{"heat2", S_LEAVE, "100", "Discharging", "Overheat", "1", "1", "DISCHARGE"},
	{"heat2", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"cold1", S_ENTER, "100", "Discharging", "Cold", "1", "1", NULL},
	{"cold1", S_ENTER, "100", "Not charging", "Cold", "2", "1", "HEALTH(L) BEFORE CHARGE"},
	{"cold1", S_LEAVE, "100", "Discharging", "Cold", "1", "1", "DISCHARGE"},
	{"cold1", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"cold2", S_ENTER, "100", "Charging",    "Good", "2", "1", NULL},
	{"cold2", S_ENTER, "100", "Not charging", "Cold", "2", "1", "HEALTH(L) AFTER CHARGE"},
	{"cold2", S_LEAVE, "100", "Discharging", "Cold", "1", "1", "DISCHARGE"},
	{"cold2", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"ovp",  S_ENTER, "100", "Discharging", "Over voltage", "1", "1", "OVP"},
	{"ovp",  S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"pres1", S_ENTER, "100", "Discharging", "Good", "1", "0", NULL},
	{"pres1", S_ENTER, "100", "Not charging", "Good", "2", "0", "PRESENT BEFORE CHARGE"},
	{"pres1", S_LEAVE, "100", "Discharging", "Good", "1", "0", "DISCHARGE"},
	{"pres1", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"pres2", S_ENTER, "100", "Charging",    "Good", "2", "1", NULL},
	{"pres2", S_ENTER, "100", "Not charging", "Good", "2", "0", "PRESENT AFTER CHARGE"},
	{"pres2", S_LEAVE, "100", "Discharging", "Good", "1", "0", "DISCHARGE"},
	{"pres2", S_LEAVE, "100", "Discharging", "Good", "1", "1", NULL}, /* init */

	{"bat15", S_ENTER, "15", "Discharging", "Good", "1", "1", "LOWBAT 15%"}, /* lowbat 15% */
	{"bat15", S_LEAVE, "15", "Charging",    "Good", "2", "1", "LOWBAT 15%"},
	{"bat5", S_ENTER, "5",	"Discharging", "Good", "1", "1", "LOWBAT 5%"},  /* lowbat 5% */
	{"bat5", S_LEAVE, "5",	"Charging",    "Good", "2", "1", "LOWBAT 5%"},
	{"bat3", S_ENTER, "3",	"Discharging", "Good", "1", "1", "LOWBAT 3%"},  /* lowbat 3% */
	{"bat3", S_LEAVE, "3",	"Charging",    "Good", "2", "1", "LOWBAT 3%"},
	{"bat1", S_ENTER, "1",	"Discharging", "Good", "1", "1", "LOWBAT 1%"},  /* lowbat 1% */
	{"bat1", S_LEAVE, "1",	"Charging",    "Good", "2", "1", "LOWBAT 1%"},

	{"ta",   S_ENTER, "100", "Charging",    "Good", "2", "1", "CHARGE"},   /* charging */
	{"ta",   S_LEAVE, "100", "Discharging", "Good", "1", "1", "DISCHARGE"},/* discharging */

	{"full", S_ENTER, "100", "Full",        "Good", "2", "1", "CHARGE"},   /* full */
	{"full", S_LEAVE, "100", "Discharging", "Good", "1", "1", "DISCHARGE"},/* discharging */

	{"capa", S_ENTER, "100", "Discharging", "Good", "1", "1", "CAPACITY"},/* discharging */
	{"capa", S_LEAVE, "100", "Charging",    "Good", "2", "1", "CAPACITY"},/* charging */
};

static void unregister_edbus_signal_handler(void)
{
	e_dbus_signal_handler_del(edbus_conn, edbus_charge_now_handler);
	e_dbus_signal_handler_del(edbus_conn, edbus_charger_status_handler);
	e_dbus_signal_handler_del(edbus_conn, edbus_temp_good_handler);
	e_dbus_connection_close(edbus_conn);
	e_dbus_shutdown();
}

static void power_supply_changed(void *data, DBusMessage *msg)
{
	DBusError err;
	int val;
	int r;

	_I("edbus signal Received");

	r = dbus_message_is_signal(msg, DEVICED_INTERFACE_BATTERY, SIGNAL_CHARGE_NOW);
	if (!r) {
		_E("dbus_message_is_signal error");
		return;
	}

	_I("%s - %s", DEVICED_INTERFACE_BATTERY, SIGNAL_CHARGE_NOW);

	dbus_error_init(&err);
	r = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!r) {
		_E("dbus_message_get_args error");
		return;
	}
	_I("receive data : %d", val);
}

static int register_charge_now_handler(void)
{
	int ret;

	e_dbus_init();

	edbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!(edbus_conn)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_out;
	}

	edbus_charge_now_handler = e_dbus_signal_handler_add(edbus_conn, NULL, DEVICED_PATH_BATTERY,
			DEVICED_INTERFACE_BATTERY, SIGNAL_CHARGE_NOW, power_supply_changed, NULL);
	if (!(edbus_charge_now_handler)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_connection_out;
	}
	return 0;

edbus_handler_connection_out:
	e_dbus_connection_close(edbus_conn);
edbus_handler_out:
	e_dbus_shutdown();
	return ret;
}

static void charger_status_changed(void *data, DBusMessage *msg)
{
	DBusError err;
	int val;
	int r;

	_I("edbus signal Received");

	r = dbus_message_is_signal(msg, DEVICED_INTERFACE_BATTERY, SIGNAL_CHARGER_STATUS);
	if (!r) {
		_E("dbus_message_is_signal error");
		return;
	}

	_I("%s - %s", DEVICED_INTERFACE_BATTERY, SIGNAL_CHARGER_STATUS);

	dbus_error_init(&err);
	r = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!r) {
		_E("dbus_message_get_args error");
		return;
	}
	_I("receive data : %d", val);
}

static int register_charger_status_handler(void)
{
	int ret;

	e_dbus_init();

	edbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!(edbus_conn)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_out;
	}

	edbus_charger_status_handler = e_dbus_signal_handler_add(edbus_conn, NULL, DEVICED_PATH_BATTERY,
			DEVICED_INTERFACE_BATTERY, SIGNAL_CHARGER_STATUS, charger_status_changed, NULL);
	if (!(edbus_charger_status_handler)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_connection_out;
	}
	return 0;

edbus_handler_connection_out:
	e_dbus_connection_close(edbus_conn);
edbus_handler_out:
	e_dbus_shutdown();
	return ret;
}

static void temp_status_changed(void *data, DBusMessage *msg)
{
	DBusError err;
	int val;
	int r;

	_I("edbus signal Received");

	r = dbus_message_is_signal(msg, DEVICED_INTERFACE_BATTERY, SIGNAL_TEMP_GOOD);
	if (!r) {
		_E("dbus_message_is_signal error");
		return;
	}

	_I("%s - %s", DEVICED_INTERFACE_BATTERY, SIGNAL_TEMP_GOOD);
}

static int register_temp_good_handler(void)
{
	int ret;

	e_dbus_init();

	edbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!(edbus_conn)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_out;
	}

	edbus_temp_good_handler = e_dbus_signal_handler_add(edbus_conn, NULL, DEVICED_PATH_BATTERY,
			DEVICED_INTERFACE_BATTERY, SIGNAL_TEMP_GOOD, temp_status_changed, NULL);
	if (!(edbus_temp_good_handler)) {
		ret = -ECONNREFUSED;
		goto edbus_handler_connection_out;
	}
	return 0;

edbus_handler_connection_out:
	e_dbus_connection_close(edbus_conn);
edbus_handler_out:
	e_dbus_shutdown();
	return ret;
}
static void power_supply_signal(void)
{
	_I("test");
	register_charge_now_handler();
	register_charger_status_handler();
	register_temp_good_handler();
	ecore_main_loop_begin();
}

static int power_supply(int index)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *param[7];

	param[0] = POWER_SUBSYSTEM;
	param[1] = "5";
	param[2] = power_supply_types[index].capacity;
	param[3] = power_supply_types[index].charge_status;
	param[4] = power_supply_types[index].health;
	param[5] = power_supply_types[index].online;
	param[6] = power_supply_types[index].present;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI,
			DEVICED_INTERFACE_SYSNOTI,
			POWER_SUBSYSTEM, "sisssss", param);
	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			POWER_SUBSYSTEM);
		return -EBADMSG;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (ret == 0) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		val = -EBADMSG;
	}

	if (power_supply_types[index].name != NULL)
		_I("++++++++++[START] %s ++++++++++", power_supply_types[index].name);
	_I("C(%s , %s) P(%s) STATUS(%s) HEALTH(%s)",
		power_supply_types[index].capacity,
		power_supply_types[index].online,
		power_supply_types[index].present,
		power_supply_types[index].charge_status,
		power_supply_types[index].health);
	if (power_supply_types[index].name != NULL)
		_I("++++++++++[END] %s ++++++++++", power_supply_types[index].name);
	if (val < 0)
		_R("[NG] ---- %s", __func__);
	else
		_R("[OK] ---- %s     : C(%s , %s) P(%s) S(%s) H(%s)",
		__func__,
		power_supply_types[index].capacity,
		power_supply_types[index].online,
		power_supply_types[index].present,
		power_supply_types[index].charge_status,
		power_supply_types[index].health);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	sleep(TEST_WAIT_TIME_INTERVAL);
	return val;
}

static void scenario(char *scenario)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(power_supply_types); index++) {
		if (strcmp(scenario, power_supply_types[index].scenario) != 0)
			continue;
		power_supply(index);
	}
}

static void unit(char *unit, int status)
{
	int index;
	int found = 0;
	char *scenario = NULL;

	for (index = 0; index < ARRAY_SIZE(power_supply_types); index++) {
		scenario = power_supply_types[index].scenario;
		if (strcmp(unit, scenario) != 0 ||
		    power_supply_types[index].status != status)
			continue;
		found = 1;
		power_supply(index);
	}

	if (found)
		return;

	index = strlen(unit);
	if (index < 0 || index > 3)
		return;

	index = strtol(unit, NULL, 10);
	if (index < 0 || index > 100)
		return;

	for (index = 0; index < ARRAY_SIZE(power_supply_types); index++) {
		if (strcmp("capa", power_supply_types[index].scenario) != 0 ||
		    power_supply_types[index].status != status)
			continue;
		power_supply_types[index].capacity = unit;
		_D("%s", power_supply_types[index].capacity);
		power_supply(index);
	}
}

static void full(char *unit, char *capacity, int status)
{
	int index;
	for (index = 0; index < ARRAY_SIZE(power_supply_types); index++) {
		if (strcmp(unit, power_supply_types[index].scenario) != 0 ||
		    power_supply_types[index].status != status)
			continue;
		power_supply_types[index].capacity = capacity;
		_D("%s", power_supply_types[index].capacity);
		power_supply(index);
	}
}

static void lowbat_popup(char *status)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *method;

	if (!status)
		return;
	val = atoi(status);
	if (val != APPS_ENABLE && val != APPS_DISABLE)
		return;

	if (val == APPS_ENABLE)
		method = METHOD_LOWBAT_POPUP_ENABLE;
	else
		method = METHOD_LOWBAT_POPUP_DISABLE;
	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		DEVICED_PATH_APPS,
		DEVICED_INTERFACE_APPS,
		method, NULL, NULL);

	if (!msg) {
		_E("fail : %s %s %s %s",
			DEVICED_BUS_NAME, DEVICED_PATH_APPS, DEVICED_INTERFACE_APPS, method);
		return;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (ret == 0) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		val = -EBADMSG;
	}

	if (val < 0)
		_R("[NG] ---- %s      : V(%s %d)", __func__, method, val);
	else
		_R("[OK] ---- %s      : V(%s %d)", __func__, method, val);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	sleep(TEST_WAIT_TIME_INTERVAL);
	return;
}

static void power_supply_init(void *data)
{
	int index;

	_I("start test");
	for (index = 0; index < ARRAY_SIZE(power_supply_types); index++)
		power_supply(index);
}

static void power_supply_exit(void *data)
{
	_I("end test");
}

static int power_supply_unit(int argc, char **argv)
{
	if (argv[1] == NULL)
		return -EINVAL;
	else if (argc < 4)
		return -EAGAIN;
	if (strcmp("wait", argv[2]) == 0)
		power_supply_signal();
	else if (strcmp("popup", argv[2]) == 0)
		lowbat_popup(argv[3]);
	else if (strcmp("scenario", argv[2]) == 0)
		scenario(argv[3]);
	else if (strcmp("enter", argv[3]) == 0)
		unit(argv[2], S_ENTER);
	else if (strcmp("leave", argv[3]) == 0)
		unit(argv[2], S_LEAVE);
	else if (strcmp("enter", argv[4]) == 0)
		full(argv[2], argv[3], S_ENTER);
	else if (strcmp("leave", argv[4]) == 0)
		full(argv[2], argv[3], S_LEAVE);
	return 0;
}

static const struct test_ops power_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "power",
	.init     = power_supply_init,
	.exit    = power_supply_exit,
	.unit    = power_supply_unit,
};

TEST_OPS_REGISTER(&power_test_ops)
