/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <errno.h>
#include <vconf.h>
#include <device-node.h>
#include <bundle.h>
#include <eventsystem.h>

#include "core/log.h"
#include "core/common.h"
#include "core/edbus-handler.h"
#include "display/poll.h"
#include "extcon.h"

#define SIGNAL_EARJACK_STATUS       "Earjack"
#define SIGANL_EARJACK_KEY_STATUS   "EarjackKey"

static struct extcon_ops earjack_extcon_ops;
static struct extcon_ops earkey_extcon_ops;

static void earjack_broadcast(char *sig, int status)
{
	static int old;
	static char sig_old[32];
	char *arr[1];
	char str_status[32];

	if (strcmp(sig_old, sig) == 0 && old == status)
		return;

	_D("%s %d", sig, status);

	old = status;
	snprintf(sig_old, sizeof(sig_old), "%s", sig);
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_EXTCON, DEVICED_INTERFACE_EXTCON,
			sig, "i", arr);
}

static void earjack_send_system_event(int status)
{
	bundle *b;
	const char *str;

	if (status)
		str = EVT_VAL_EARJACK_CONNECTED;
	else
		str = EVT_VAL_EARJACK_DISCONNECTED;

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_EARJACK_STATUS, str);
	eventsystem_send_system_event(SYS_EVENT_EARJACK_STATUS, b);
	bundle_free(b);
}

static DBusMessage *dbus_get_earjack_status(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, earjack_extcon_ops.status);
}

static DBusMessage *dbus_get_earjack_key_status(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, earkey_extcon_ops.status);
}

static const struct edbus_method earjack_edbus_methods[] = {
	{ SIGNAL_EARJACK_STATUS,       NULL, "i", dbus_get_earjack_status },
	/* Add methods here */
};

static const struct edbus_method earkey_edbus_methods[] = {
	{ SIGANL_EARJACK_KEY_STATUS,   NULL, "i", dbus_get_earjack_key_status },
	/* Add methods here */
};

static int earjack_update(int status)
{
	int ret;

	/**
	 * In kernel v3.0, kernel sends a changed event without status.
	 * So whenever earjack is updated,
	 * we should get the status from kernel node.
	 * It's a temporary code and will be removed on next kernel version.
	 */
	if (status < 0) {
		ret = device_get_property(DEVICE_TYPE_EXTCON,
				PROP_EXTCON_EARJACK_ONLINE, &status);
		if (ret < 0) {
			_E("fail to get earjack status : %d", ret);
			return -EPERM;
		}

		if (earjack_extcon_ops.status == status)
			return 0;

		earjack_extcon_ops.status = status;
	}

	_I("jack - earjack changed %d", status);
	vconf_set_int(VCONFKEY_SYSMAN_EARJACK, status);
	earjack_broadcast(SIGNAL_EARJACK_STATUS, status);
	earjack_send_system_event(status);
	if (status)
		pm_change_internal(getpid(), LCD_NORMAL);

	return 0;
}

static void earjack_init(void *data)
{
	int ret;

	/* Mobile specific:
	 * Extcon state node is not linked in the valid node in kernel v3.0.
	 * So it should be updated again */
	earjack_update(-1);

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_EXTCON,
			earjack_edbus_methods, ARRAY_SIZE(earjack_edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static int earkey_update(int status)
{
	int ret;

	/**
	 * In kernel v3.0, kernel sends a changed event without status.
	 * So whenever earkey is updated,
	 * we should get the status from kernel node.
	 * It's a temporary code and will be removed on next kernel version.
	 */
	if (status < 0) {
		ret = device_get_property(DEVICE_TYPE_EXTCON,
				PROP_EXTCON_EARKEY_ONLINE, &status);
		if (ret < 0) {
			_E("fail to get earkey status : %d", ret);
			return -EPERM;
		}

		if (earkey_extcon_ops.status == status)
			return 0;

		earkey_extcon_ops.status = status;
	}

	_I("jack - earkey changed %d", status);
	/* 0: not press 1: press */
	earjack_broadcast(SIGANL_EARJACK_KEY_STATUS, status);

	return 0;
}

static void earkey_init(void *data)
{
	int ret;

	/* Mobile specific:
	 * Extcon state node is not linked in the valid node in kernel v3.0.
	 * So it should be updated again */
	earkey_update(-1);

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_EXTCON,
			earkey_edbus_methods, ARRAY_SIZE(earkey_edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static struct extcon_ops earjack_extcon_ops = {
	.name   = EXTCON_CABLE_HEADPHONE_OUT,
	.init   = earjack_init,
	.update = earjack_update,
};

EXTCON_OPS_REGISTER(earjack_extcon_ops)

/**
 * Mobile specific:
 * In the next version of the Kernel v3.0,
 * earkey use input class instead of extcon.
 */
static struct extcon_ops earkey_extcon_ops = {
	.name   = EXTCON_CABLE_EARKEY,
	.init   = earkey_init,
	.update = earkey_update,
};

EXTCON_OPS_REGISTER(earkey_extcon_ops)
