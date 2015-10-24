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


#include <linux/input.h>
#include <pthread.h>
#include <poll.h>
#include <time.h>

#include "core/common.h"
#include "core/list.h"
#include "core/udev.h"
#include "core/log.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "libcec.h"

#define CED_PATH		"/devices/soc/*.cec"

struct cec_dev {
	pthread_t cec_thread;
	int		mCecFd;
	int		mCecPaddr;
	int		mCecLaddr;
	int		mCecTvOnOff;
	int		mCecRoutPaddr;
};

static struct cec_dev *cec_device;

void send_CEConeTouchPlay(struct cec_dev *pdev)
{
	int size = 0;
	unsigned char buffer[4];
	int laddr = pdev->mCecLaddr;
	int paddr = pdev->mCecPaddr;
	struct timespec time = { 0, 500 * NANO_SECOND_MULTIPLIER };

	buffer[0] = 0x40;
	buffer[1] = CEC_OPCODE_TEXT_VIEW_ON;
	size = 2;
	_I("Tx : [CEC_OPCODE_TEXT_VIEW_ON]");

	if (CECSendMessage(buffer, size) != size)
		_E("CECSendMessage() failed!!!");
	nanosleep(&time, NULL);

	buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
	buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
	buffer[2] = (paddr >> 8) & 0xFF;
	buffer[3] = paddr & 0xFF;
	size = 4;
	_I("Tx : [CEC_OPCODE_ACTIVE_SOURCE]");

	if (CECSendMessage(buffer, size) != size)
		_E("CECSendMessage() failed!!!");
	nanosleep(&time, NULL);

	buffer[0] = 0x40;
	buffer[1] = CEC_OPCODE_IMAGE_VIEW_ON;
	size = 2;
	_I("Tx : [CEC_OPCODE_IMAGE_VIEW_ON]");

	if (CECSendMessage(buffer, size) != size)
		_E("CECSendMessage() failed!!!");
	nanosleep(&time, NULL);

	buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
	buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
	buffer[2] = (paddr >> 8) & 0xFF;
	buffer[3] = paddr & 0xFF;
	size = 4;
	_I("Tx : [CEC_OPCODE_ACTIVE_SOURCE]");

	if (CECSendMessage(buffer, size) != size)
		_E("CECSendMessage() failed!!!");
	return;
}

static void handle_cec(struct cec_dev *pdev)
{
	unsigned char buffer[16];
	int size;
	unsigned char lsrc, ldst, opcode;
	int Psrc;
	int ceconoff = 0;

	size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 1000);

	/* no data available or ctrl-c */
	if (!size) {
		_E("fail");
		return;
	}
	/* "Polling Message" */
	if (size == 1) {
		_E("fail");
		return;
	}

	if (!pdev) {
		_E("there is no cec handle");
		return;
	}
	lsrc = buffer[0] >> 4;

	/* ignore messages with src address == mCecLaddr */
	if (lsrc == pdev->mCecLaddr) {
		_E("fail %x %x", lsrc, pdev->mCecLaddr);
		return;
	}
	opcode = buffer[1];

	if (lsrc != CEC_MSG_BROADCAST && (opcode == CEC_OPCODE_SET_STREAM_PATH)) {
		Psrc = buffer[2] << 8 | buffer[3];
		_I("Psrc = 0x%x, dev->Paddr= 0x%x", Psrc, pdev->mCecPaddr);
		if (pdev->mCecPaddr != Psrc) {
			_I("### ignore message : 0x%x ", opcode);
			return;
		}
	}

	if (opcode == CEC_OPCODE_ROUTING_CHANGE) {
		Psrc = buffer[4] << 8 | buffer[5];
		_I("Psrc = 0x%x, dev->Paddr= 0x%x", Psrc, pdev->mCecPaddr);
		pdev->mCecRoutPaddr = Psrc;
		_I("Opcode : 0x%x Routing addr 0x%x", opcode, pdev->mCecRoutPaddr);
	}

	if (CECIgnoreMessage(opcode, lsrc)) {
		_E("### ignore message coming from address 15 (unregistered)");
		return;
	}

	if (buffer[0] == CEC_MSG_BROADCAST) {
		switch (opcode) {
		case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
		case CEC_OPCODE_GET_CEC_VERSION:
			_I("### ignore broadcast message : 0x%x ", opcode);
			return;
		default:
			break;
		}
	}

	if (!CECCheckMessageSize(opcode, size)) {
		/*
		* For some reason the TV sometimes sends messages that are too long
		* Dropping these causes the connect process to fail, so for now we
		* simply ignore the extra data and process the message as if it had
		* the correct size
		*/
		_I("### invalid message size: %d(opcode: 0x%x) ###", size, opcode);
		return;
	}

	/* check if message broadcasted/directly addressed */
	if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
		_E("### invalid message mode (directly addressed/broadcast) ###");
		return;
	}

	ldst = lsrc;

	/* TODO: macros to extract src and dst logical addresses */
	/* TODO: macros to extract opcode */

	if (opcode == CEC_OPCODE_SET_STREAM_PATH)
		ceconoff = 1;
	size = CECProcessOpcode(buffer, pdev->mCecLaddr, pdev->mCecPaddr,
	pdev->mCecRoutPaddr);

	if (size > 0) {
		if (CECSendMessage(buffer, size) != size)
			_E("CECSendMessage() failed!!!");
	}

	if (ceconoff)
		send_CEConeTouchPlay(pdev);
}

static void *hwc_cec_thread(void *data)
{
	struct pollfd fds;
	struct cec_dev *pdev =
		(struct cec_dev *)data;

	fds.fd = pdev->mCecFd;
	fds.events = POLLIN;

	while (true) {
		int err;
		fds.fd = pdev->mCecFd;
		if (fds.fd > 0) {
			err = poll(&fds, 1, -1);
			if (err > 0) {
				if (fds.revents & POLLIN)
					handle_cec(pdev);
			} else if (err == -1) {
				if (errno == EINTR)
					break;
				_E("error in cec thread: %d", errno);
			}
		}
	}
	return NULL;
}

static int check_cec(void)
{
	static int cec = -1;

	if (cec != -1)
		goto out;

	if (access("/dev/cec0", F_OK) == 0)
		cec = 1;
	cec = 0;
out:
	return cec;
}

static void start_cec(struct cec_dev *pdev)
{
	unsigned char buffer[CEC_MAX_FRAME_SIZE];
	int size;

	if (!pdev) {
		_E("there is no cec handle");
		return;
	}
	pdev->mCecPaddr = 0x100B;/* CEC_NOT_VALID_PHYSICAL_ADDRESS */

	/* get TV physical address */
	pdev->mCecLaddr = CECAllocLogicalAddress(pdev->mCecPaddr, CEC_DEVICE_PLAYER);
	/* Request power state from TV */
	buffer[0] = (pdev->mCecLaddr << 4);
	buffer[1] = CEC_OPCODE_GIVE_DEVICE_POWER_STATUS;
	size = 2;
	if (CECSendMessage(buffer, size) != size)
		_E("CECSendMessage(%#x) failed!!!", buffer[0]);
	/* Wakeup Homekey at first connection */
	CECReportKey(KEY_POWER);
}

static void open_cec(struct cec_dev *pdev)
{
	int ret;

	if (pdev) {
		_I("already initialized");
		return;
	}
	pdev = (struct cec_dev *)malloc(sizeof(*pdev));
	if (!pdev) {
		_E("there is no cec handle");
		return;
	}
	memset(pdev, 0, sizeof(*pdev));
	pdev->mCecFd = CECOpen();
	_I("cec is %d", pdev->mCecFd);
	if (pdev->mCecFd > 0) {
		ret = pthread_create(&pdev->cec_thread, NULL, hwc_cec_thread, pdev);
		if (ret) {
			_E("failed to start cec thread: %d", ret);
			pdev->mCecFd = -1;
			CECClose();
			free(pdev);
			pdev = NULL;
		} else {
			_I("run thread");
			start_cec(pdev);
			_I("start cec");
			init_input_key_fd();
		}

	} else {
		pdev->mCecFd = -1;
		free(pdev);
		pdev = NULL;
	}
}

static void close_cec(struct cec_dev *pdev)
{
	if (!pdev) {
		_E("there is no cec handle");
		return;
	}
	CECClose();
	_I("cec is %d", pdev->mCecFd);
	pthread_kill(pdev->cec_thread, SIGTERM);
	pthread_join(pdev->cec_thread, NULL);
	pdev->mCecFd = -1;
	free(pdev);
	pdev = NULL;
}

static void cec_uevent_changed(struct udev_device *dev)
{
	const char *state = NULL;
	const char *devpath = NULL;
	const char *value = NULL;

	int ret;
	static int cradle;

	devpath = udev_device_get_property_value(dev, "DEVNAME");
	value = udev_device_get_property_value(dev, "HDMICEC");
	_I("%s %s", devpath, value);
}

static const struct uevent_handler cec_uevent_handler = {
	.subsystem   = "misc",
	.uevent_func = cec_uevent_changed,
};

static DBusMessage *dbus_get_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "GetState"  ,  NULL, "i" ,  dbus_get_state },
};

static int cec_init_booting_done(void *data)
{
	int ret;

	ret = register_udev_uevent_control(&cec_uevent_handler);
	if (ret < 0)
		_E("FAIL: reg_uevent_control()");

	ret = register_edbus_interface_and_method(DEVICED_PATH_HDMICEC,
			DEVICED_INTERFACE_HDMICEC,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	return 0;
}

static void hdmi_cec_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, cec_init_booting_done);
	_I("open cec");
	open_cec(cec_device);
}

static void hdmi_cec_exit(void *data)
{
	unregister_udev_uevent_control(&cec_uevent_handler);
}

static int hdmi_cec_execute(void *data)
{
	int state = (int)data;
/*
	if (check_cec() == 0) {
		_E("there is no cec");
		return 0;
	}
*/
	if (state) {
		_I("start cec");
		start_cec(cec_device);
	} else
		close_cec(cec_device);
	return 0;
}

static const struct device_ops cec_device_ops = {
	.name     = "hdmi-cec",
	.init     = hdmi_cec_init,
	.exit     = hdmi_cec_exit,
	.execute = hdmi_cec_execute,
};

DEVICE_OPS_REGISTER(&cec_device_ops)
