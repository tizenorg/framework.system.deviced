/*
 * TIMA Notification
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
#include <assert.h>
#include <E_DBus.h>

#include "core/log.h"
#include "core/common.h"

#define SYSTEM_POPUP_BUS_NAME	"org.tizen.system.popup"
#define SYSTEM_POPUP_DBUS_PATH	"/Org/Tizen/System/Popup"

#define SYSTEM_POPUP_PATH_TIMA	SYSTEM_POPUP_DBUS_PATH"/Tima"
#define SYSTEM_POPUP_IFACE_TIMA	SYSTEM_POPUP_BUS_NAME".Tima"

#define METHOD_PKM_NOTI_ON	"PKMDetectionNotiOn"
#define METHOD_PKM_NOTI_OFF	"PKMDetectionNotiOff"
#define METHOD_LKM_NOTI_ON	"LKMPreventionNotiOn"
#define METHOD_LKM_NOTI_OFF	"LKMPreventionNotiOff"

#define RETRY_MAX		10
#define DBUS_REPLY_TIMEOUT	(120 * 1000)

static int append_variant(DBusMessageIter *iter, const char *sig, char *param[])
{
	char *ch;
	int i, int_type;

	if (!sig || !param)
		return 0;

	for (ch = (char*)sig, i = 0; *ch != '\0'; ++i, ++ch) {
		switch (*ch) {
		case 's':
			dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &param[i]);
			break;
		case 'i':
			int_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &int_type);
			break;
		default:
			_E("ERROR: %s %c", sig, *ch);
			return -EINVAL;
		}
	}
	return 0;
}

DBusMessage *call_dbus_method(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn;
	DBusMessage *msg = NULL;
	DBusMessageIter iter;
	DBusMessage *ret;
	DBusError err;
	int r;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		ret = NULL;
		goto out;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		ret = NULL;
		goto out;
	}

	dbus_message_iter_init_append(msg, &iter);
	r = append_variant(&iter, sig, param);
	if (r < 0) {
		ret = NULL;
		goto out;
	}

	/*This function is for synchronous dbus method call */
	dbus_error_init(&err);
	ret = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	dbus_error_free(&err);

out:
	dbus_message_unref(msg);

	return ret;
}

int request_to_launch_by_dbus(char *bus, char *path, char *iface,
		char *method, char *ptype, char *param[])
{
	DBusMessage *msg;
	DBusError err;
	int i, r, ret_val;

	i = 0;
	do {
		msg = call_dbus_method(bus, path, iface, method, ptype, param);
		if (msg)
			break;
		i++;
	} while (i < RETRY_MAX);
	if (!msg) {
		_E("fail to call dbus method");
		return -ECONNREFUSED;
	}

	dbus_error_init(&err);

	r = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!r) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	return ret_val;
}

/* This function returns notification ID (ID >= 0) */
int tima_lkm_prevention_noti_on(void)
{
	return request_to_launch_by_dbus(SYSTEM_POPUP_BUS_NAME,
			SYSTEM_POPUP_PATH_TIMA,	SYSTEM_POPUP_IFACE_TIMA,
			METHOD_LKM_NOTI_ON, NULL, NULL);
}

/* This function needs the notification ID to remove
 * This function return 0 if success */
int tima_lkm_prevention_noti_off(int noti_id)
{
	char *param[1];
	char str[8];
	snprintf(str, sizeof(str), "%d", noti_id);
	param[0] = str;

	return request_to_launch_by_dbus(SYSTEM_POPUP_BUS_NAME,
			SYSTEM_POPUP_PATH_TIMA,	SYSTEM_POPUP_IFACE_TIMA,
			METHOD_LKM_NOTI_OFF, "i", param);
}

/* This function returns notification ID (ID >= 0) */
int tima_pkm_detection_noti_on(void)
{
	return request_to_launch_by_dbus(SYSTEM_POPUP_BUS_NAME,
			SYSTEM_POPUP_PATH_TIMA,	SYSTEM_POPUP_IFACE_TIMA,
			METHOD_PKM_NOTI_ON, NULL, NULL);
}

/* This function needs the notification ID to remove
 * This function return 0 if success */
int tima_pkm_detection_noti_off(int noti_id)
{
	char *param[1];
	char str[8];
	snprintf(str, sizeof(str), "%d", noti_id);
	param[0] = str;

	return request_to_launch_by_dbus(SYSTEM_POPUP_BUS_NAME,
			SYSTEM_POPUP_PATH_TIMA,	SYSTEM_POPUP_IFACE_TIMA,
			METHOD_PKM_NOTI_OFF, "i", param);
}
