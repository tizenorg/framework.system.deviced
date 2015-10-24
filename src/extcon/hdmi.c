/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <device-node.h>
#include "core/log.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "display/core.h"
#include "apps/apps.h"
#include "extcon.h"

#define METHOD_GET_HDMI		"GetHDMI"
#define METHOD_GET_HDCP		"GetHDCP"
#define METHOD_GET_HDMI_AUDIO	"GetHDMIAudio"
#define SIGNAL_HDMI_STATE	"ChangedHDMI"
#define SIGNAL_HDCP_STATE	"ChangedHDCP"
#define SIGNAL_HDMI_AUDIO_STATE	"ChangedHDMIAudio"

/* Ticker notification */
#define HDMI_CONNECTED    "HdmiConnected"
#define HDMI_DISCONNECTED "HdmiDisconnected"

#define HDCP_HDMI_VALUE(HDCP, HDMI)	((HDCP << 1) | HDMI)
#define HDMI_NOT_SUPPORTED	(-1)

static int hdmi_hdcp_status;

static struct extcon_ops hdmi_extcon_ops;
static struct extcon_ops hdcp_extcon_ops;
static struct extcon_ops hdmi_audio_extcon_ops;

static void hdmi_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast hdmi status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_STATE, "i", arr);
}

static int hdmi_update(int status)
{
	int val;
	int ret;
	static const struct device_ops *hdmi_cec_ops;

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_HDMI_SUPPORT, &val) == 0) {
		if (val != 1) {
			_I("target is not support HDMI");
			vconf_set_int(VCONFKEY_SYSMAN_HDMI, HDMI_NOT_SUPPORTED);
			hdmi_send_broadcast(HDMI_NOT_SUPPORTED);
			return -ENODEV;
		}
	}

	/* Mobile specific:
	 * Platform event does not contain a changed value.
	 * In this case, extcon_update forwards a negative value. */
	if (status < 0) {
		ret = device_get_property(DEVICE_TYPE_EXTCON,
				PROP_EXTCON_HDMI_ONLINE, &status);
		if (ret != 0) {
			_E("failed to get status");
			return -ENODEV;
		}

		if (hdmi_extcon_ops.status == status)
			return 0;

		hdmi_extcon_ops.status = status;
	}

	_I("jack - hdmi changed %d", status);
	pm_change_internal(getpid(), LCD_NORMAL);
	vconf_set_int(VCONFKEY_SYSMAN_HDMI, status);
	hdmi_send_broadcast(status);

	if (status == 1) {
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_OFF, STAY_CUR_STATE, 0);
		/* dim lock is released when user pressed the power key. */
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, STAY_CUR_STATE, 0);
		_I("hdmi is connected! dim lock is on.");
		launch_message_post(HDMI_CONNECTED);
	} else {
		launch_message_post(HDMI_DISCONNECTED);
		pm_unlock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, PM_SLEEP_MARGIN);
		_I("hdmi is disconnected! dim lock is off.");
		pm_unlock_internal(INTERNAL_LOCK_HDMI, LCD_OFF, PM_SLEEP_MARGIN);
	}

	FIND_DEVICE_INT(hdmi_cec_ops, "hdmi-cec");
	hdmi_cec_ops->execute(&status);

	return 0;
}

static void hdcp_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast hdcp status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDCP_STATE, "i", arr);
}

static int hdcp_update(int status)
{
	hdcp_send_broadcast(status);

	/* update hdmi_hdcp status */
	hdmi_hdcp_status = HDCP_HDMI_VALUE(status, hdmi_extcon_ops.status);
	hdmi_send_broadcast(hdmi_hdcp_status);
	return 0;
}

static void hdmi_audio_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast hdmi audio status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_AUDIO_STATE, "i", arr);
}

static int hdmi_audio_update(int status)
{
	hdmi_audio_send_broadcast(status);
	return 0;
}

static DBusMessage *dbus_hdcp_hdmi_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, hdmi_hdcp_status);
}

static DBusMessage *dbus_hdcp_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, hdcp_extcon_ops.status);
}

static DBusMessage *dbus_hdmi_audio_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, hdmi_audio_extcon_ops.status);
}

static DBusMessage *dbus_hdmi_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, hdmi_extcon_ops.status);
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_GET_HDCP,       NULL, "i", dbus_hdcp_handler },
	{ METHOD_GET_HDMI_AUDIO, NULL, "i", dbus_hdmi_audio_handler },
	{ METHOD_GET_HDMI,       NULL, "i", dbus_hdcp_hdmi_handler },
};

static int display_changed(void *data)
{
	enum state_t state;
	int hdmi;

	if (!data)
		return 0;

	state = *(int *)data;
	if (state != S_NORMAL)
		return 0;

	hdmi = hdmi_extcon_ops.status;
	if (hdmi) {
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, STAY_CUR_STATE, 0);
		_I("hdmi is connected! dim lock is on.");
	}
	return 0;
}

static void hdmi_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static void hdmi_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);
}

static struct extcon_ops hdmi_extcon_ops = {
	.name   = EXTCON_CABLE_HDMI,
	.init   = hdmi_init,
	.exit   = hdmi_exit,
	.update = hdmi_update,
};

EXTCON_OPS_REGISTER(hdmi_extcon_ops)

static struct extcon_ops hdcp_extcon_ops = {
	.name = EXTCON_CABLE_HDCP,
	.update = hdcp_update,
};

EXTCON_OPS_REGISTER(hdcp_extcon_ops)

static struct extcon_ops hdmi_audio_extcon_ops = {
	.name = EXTCON_CABLE_HDMI_AUDIO,
	.update = hdmi_audio_update,
};

EXTCON_OPS_REGISTER(hdmi_audio_extcon_ops)
