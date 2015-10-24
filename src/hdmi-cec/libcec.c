/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>
/* drv. header */
#include "cec.h"
#include "libcec.h"
#include "core/log.h"
#include "core/common.h"

#define CEC_DEBUG 1

#define CEC_KEY_RELEASED 0
#define CEC_KEY_PRESSED 1
/**
 * @def CEC_DEVICE_NAME
 * Defines simbolic name of the CEC device.
 */
#define CEC_DEVICE_NAME         "/dev/cec0"

static struct {
	enum CECDeviceType devtype;
	unsigned char laddr;
} laddresses[] = {
	{ CEC_DEVICE_RECODER, 1  },
	{ CEC_DEVICE_RECODER, 2  },
	{ CEC_DEVICE_TUNER,   3  },
	{ CEC_DEVICE_PLAYER,  4  },
	{ CEC_DEVICE_AUDIO,   5  },
	{ CEC_DEVICE_TUNER,   6  },
	{ CEC_DEVICE_TUNER,   7  },
	{ CEC_DEVICE_PLAYER,  8  },
	{ CEC_DEVICE_RECODER, 9  },
	{ CEC_DEVICE_TUNER,   10 },
	{ CEC_DEVICE_PLAYER,  11 },
};

static struct {
	char *key_name;
	unsigned char cec_id;
	unsigned char opcode;
	unsigned int ui_id;
} supportkeys[] = {
	{"Connect",	0x40,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_F6},
	{"Connect",	0x40,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_F6},
	{"Enter",	0x0,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_ENTER},
	{"Enter",	0x0,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_ENTER},
	{"Up",		0x1,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_UP},
	{"Up",		0x1,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_UP},
	{"Down",	0x2,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_DOWN},
	{"Down",	0x2,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_DOWN},
	{"Left",	0x3,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_LEFT},
	{"Left",	0x3,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_LEFT},
	{"Right",	0x4,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_RIGHT},
	{"Right",	0x4,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_RIGHT},
	{"Exit",	0xD,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_MENU},
	{"Exit",	0xD,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_MENU},
	{"Clear",	0x2C,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_BACK},
	{"Clear",	0x2C,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_BACK},
	{"Clear2",	0x91,	CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN,	KEY_BACK},/* samsung key*/
	{"Play",	0x24,	CEC_OPCODE_PLAY,			KEY_PLAY},
	{"Play",	0x44,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_PLAY},
	{"Play",	0x44,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_PLAY},
	{"Stop",	0x3,	CEC_OPCODE_DECK_CONTROL,		KEY_STOP},
	{"Stop",	0x45,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_STOP},
	{"Stop",	0x45,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_STOP},
	{"Pause",	0x25,	CEC_OPCODE_PLAY,			KEY_PAUSECD},
	{"Pause",	0x46,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_PAUSECD},
	{"Pause",	0x46,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_PAUSECD},
	{"Rewind",	0x48,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_REWIND},
	{"Rewind",	0x48,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_REWIND},
	{"FastForward",	0x49,	CEC_OPCODE_USER_CONTROL_PRESSED,	KEY_FASTFORWARD},
	{"FastForward",	0x49,	CEC_OPCODE_USER_CONTROL_RELEASED,	KEY_FASTFORWARD},
	{"TV Standby",	0x36,	CEC_OPCODE_STANDBY,			KEY_HOMEPAGE},
	{"TV Routing Change", 0x80, CEC_OPCODE_ROUTING_CHANGE,		KEY_HOMEPAGE},
};

static int CECSetLogicalAddr(unsigned int laddr);

#ifdef CEC_DEBUG
static inline void CECPrintFrame(unsigned char *buffer, unsigned int size);
#endif

static int fd = -1;
static int key_fd = -1;
/**
 * Open device driver and assign CEC file descriptor.
 *
 * @return  If success to assign CEC file descriptor, return fd; otherwise, return -1.
 */
int CECOpen()
{
	if (fd != -1)
		CECClose();

	fd = open(CEC_DEVICE_NAME, O_RDWR);
	if (fd < 0) {
		_E("Can't open %s!\n", CEC_DEVICE_NAME);
		return -1;
	}

	return fd;
}

/**
 * Close CEC file descriptor.
 *
 * @return  If success to close CEC file descriptor, return 1; otherwise, return 0.
 */
int CECClose()
{
	int res = 1;

	if (fd != -1) {
		if (close(fd) != 0) {
			_E("close() failed!\n");
			res = 0;
		}
		fd = -1;
	}

	return res;
}

/**
 * Allocate logical address.
 *
 * @param paddr   [in] CEC device physical address.
 * @param devtype [in] CEC device type.
 *
 * @return new logical address, or 0 if an error occured.
 */
int CECAllocLogicalAddress(int paddr, enum CECDeviceType devtype)
{
	unsigned char laddr = CEC_LADDR_UNREGISTERED;
	int i = 0;

	_I("physical %x type %d", paddr, devtype);
	if (fd == -1) {
		_E("open device first!\n");
		return 0;
	}

	if (CECSetLogicalAddr(laddr) < 0) {
		_E("CECSetLogicalAddr() failed!\n");
		return 0;
	}

	if (paddr == CEC_NOT_VALID_PHYSICAL_ADDRESS)
		return CEC_LADDR_UNREGISTERED;

	/* send "Polling Message" */
	while (i < sizeof(laddresses) / sizeof(laddresses[0])) {
		if (laddresses[i].devtype == devtype) {
			unsigned char _laddr = laddresses[i].laddr;
			unsigned char message = ((_laddr << 4) | _laddr);
			if (CECSendMessage(&message, 1) != 1) {
				laddr = _laddr;
				_I("find logical address %x %d", laddresses[i].laddr, laddresses[i].devtype);
				break;
			}
		}
		i++;
	}

	if (laddr == CEC_LADDR_UNREGISTERED) {
		_E("All LA addresses in use!!!\n");
		return CEC_LADDR_UNREGISTERED;
	}

	if (CECSetLogicalAddr(laddr) < 0) {
		_E("CECSetLogicalAddr() failed!\n");
		return 0;
	}

	/* broadcast "Report Physical Address" */
	unsigned char buffer[5];
	buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
	buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
	buffer[2] = (paddr >> 8) & 0xFF;
	buffer[3] = paddr & 0xFF;
	buffer[4] = devtype;

	if (CECSendMessage(buffer, 5) != 5) {
		_E("CECSendMessage() failed!\n");
		return 0;
	}

	return laddr;
}

/**
 * Send CEC message.
 *
 * @param *buffer   [in] pointer to buffer address where message located.
 * @param size      [in] message size.
 *
 * @return number of bytes written, or 0 if an error occured.
 */
int CECSendMessage(unsigned char *buffer, int size)
{
	if (fd == -1) {
		_E("open device first!\n");
		return 0;
	}

	if (size > CEC_MAX_FRAME_SIZE) {
		_E("size should not exceed %d\n", CEC_MAX_FRAME_SIZE);
		return 0;
	}

#if CEC_DEBUG
	_I("CECSendMessage() : size(%d)", size);
	CECPrintFrame(buffer, size);
#else
	_I("CEC send : 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X",
	buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7],
	buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15]);
#endif
	return write(fd, buffer, size);
}

/**
 * Receive CEC message.
 *
 * @param *buffer   [in] pointer to buffer address where message will be stored.
 * @param size      [in] buffer size.
 * @param timeout   [in] timeout in microseconds.
 *
 * @return number of bytes received, or 0 if an error occured.
 */
int CECReceiveMessage(unsigned char *buffer, int size, long timeout)
{
	int bytes = 0;
	fd_set rfds;
	struct timeval tv;
	int retval;

	if (fd == -1) {
		_E("open device first!\n");
		return 0;
	}

	tv.tv_sec = 0;
	tv.tv_usec = timeout;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	retval = select(fd + 1, &rfds, NULL, NULL, &tv);

	if (retval == -1) {
		_E("fail");
		return 0;
	} else if (retval) {
		bytes = read(fd, buffer, size);
#if CEC_DEBUG
		_I("CECReceiveMessage() : size(%d)", bytes);
		if (bytes > 0)
			CECPrintFrame(buffer, bytes);
#endif
	}

	return bytes;
}

/**
 * Set CEC logical address.
 *
 * @return 1 if success, otherwise, return 0.
 */
int CECSetLogicalAddr(unsigned int laddr)
{
	if (ioctl(fd, CEC_IOC_SETLADDR, &laddr)) {
		_E("ioctl(CEC_IOC_SETLA) failed!\n");
		return 0;
	}

	return 1;
}

int init_input_key_fd(void)
{
	struct uinput_user_dev uidev;
	int key_idx = sizeof(supportkeys)/sizeof(supportkeys[0]);

	if (key_fd < 0) {
		key_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
		if (key_fd < 0) {
			_E("open failed");
			return -1;
		}
		memset(&uidev, 0, sizeof(uidev));
		snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "cec-input");
		uidev.id.bustype = BUS_USB;
		uidev.id.version = 1;
		uidev.id.vendor = 1;
		uidev.id.product = 1;
		ioctl(key_fd, UI_SET_EVBIT, EV_KEY);

		while (--key_idx >= 0) {
			if (CEC_OPCODE_USER_CONTROL_PRESSED == supportkeys[key_idx].opcode ||
			    CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN == supportkeys[key_idx].opcode) {
				ioctl(key_fd, UI_SET_KEYBIT, supportkeys[key_idx].ui_id);
				_I("register key %s key(%d)",
					supportkeys[key_idx].key_name,
					supportkeys[key_idx].ui_id);
			}
		}
		write(key_fd, &uidev, sizeof(uidev));
		ioctl(key_fd, UI_DEV_CREATE);
	}
	return 0;
}

static int input_key_event(unsigned char opcode, unsigned int key)
{
	struct input_event ev;
	int ret;

	if (opcode != CEC_OPCODE_USER_CONTROL_PRESSED &&
	    opcode != CEC_OPCODE_USER_CONTROL_RELEASED &&
	    opcode != CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN &&
	    opcode != CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP) {
		_E("unregister key event opcode(0x%X) key(%d)", opcode, key);
		return -1;
	}
	memset(&ev, 0, sizeof(ev));
	ev.type = EV_KEY;
	ev.code = key;
	if (opcode == CEC_OPCODE_USER_CONTROL_PRESSED ||
	    opcode == CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN)
		ev.value = CEC_KEY_PRESSED;
	else
		ev.value = CEC_KEY_RELEASED;
	ret = write(key_fd, &ev, sizeof(ev));
	return ret;
}

/**
 * report cec key.
 *
 * @return 1 if success, otherwise, return 0.
 */
int CECReportKey(unsigned int key)
{
	int key_idx = sizeof(supportkeys)/sizeof(supportkeys[0]);

	while (--key_idx >= 0) {
		if (key == supportkeys[key_idx].ui_id) {
			_I("%s %d", supportkeys[key_idx].key_name, key);
			break;
		}
	}
#if 0
	if (ioctl(fd, CEC_IOC_HANDLEKEY, &key)) {
		_E("ioctl(CEC_IOC_HANDLEKEY) failed!");
		return 0;
	}
#endif
	return 1;
}

#if CEC_DEBUG
/**
 * Print CEC frame.
 */
void CECPrintFrame(unsigned char *buffer, unsigned int size)
{
	if (size > 0) {
		int i;
		_I("fsize: %d ", size);
		_I("frame: ");
		for (i = 0; i < size; i++)
			_I("0x%02x ", buffer[i]);

		_I("\n");
	}
}
#endif

/**
 * Check CEC message.
 *
 * @param opcode   [in] pointer to buffer address where message will be stored.
 * @param lsrc     [in] buffer size.
 *
 * @return 1 if message should be ignored, otherwise, return 0.
 */
int CECIgnoreMessage(unsigned char opcode, unsigned char lsrc)
{
	int retval = 0;

	/* if a message coming from address 15 (unregistered) */
	if (lsrc == CEC_LADDR_UNREGISTERED) {
		switch (opcode) {
		case CEC_OPCODE_DECK_CONTROL:
		case CEC_OPCODE_PLAY:
			retval = 1;
		default:
			break;
		}
	}

	return retval;
}

/**
 * Check CEC message.
 *
 * @param opcode   [in] pointer to buffer address where message will be stored.
 * @param size     [in] message size.
 *
 * @return 0 if message should be ignored, otherwise, return 1.
 */
int CECCheckMessageSize(unsigned char opcode, int size)
{
	int retval = 1;

	switch (opcode) {
	case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
	case CEC_OPCODE_SET_SYSTEM_AUDIO_MODE:
	case CEC_OPCODE_IMAGE_VIEW_ON:
	case CEC_OPCODE_TEXT_VIEW_ON:
		if (size != 2)
			retval = 0;
		break;
	case CEC_OPCODE_PLAY:
	case CEC_OPCODE_DECK_CONTROL:
	case CEC_OPCODE_SET_MENU_LANGUAGE:
	case CEC_OPCODE_ACTIVE_SOURCE:
	case CEC_OPCODE_ROUTING_INFORMATION:
		if (size != 3)
			retval = 0;
		break;
	case CEC_OPCODE_SET_STREAM_PATH:
		if (size != 4)
			retval = 0;
		break;
	case CEC_OPCODE_FEATURE_ABORT:
	case CEC_OPCODE_DEVICE_VENDOR_ID:
	case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:
		if (size != 4)
			retval = 0;
		break;
	case CEC_OPCODE_ROUTING_CHANGE:
		if (size != 6)
			retval = 0;
		break;
	/* CDC - 1.4 */
	case 0xf8:
		if (!(size > 5 && size <= 16))
			retval = 0;
		break;
	default:
		break;
	}

	return retval;
}

/**
 * Check CEC message.
 *
 * @param opcode    [in] pointer to buffer address where message will be stored.
 * @param broadcast [in] broadcast/direct message.
 *
 * @return 0 if message should be ignored, otherwise, return 1.
 */
int CECCheckMessageMode(unsigned char opcode, int broadcast)
{
	int retval = 1;

	switch (opcode) {
	case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
	case CEC_OPCODE_SET_MENU_LANGUAGE:
	case CEC_OPCODE_ACTIVE_SOURCE:
		if (!broadcast)
			retval = 0;
		break;
	case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
	case CEC_OPCODE_DECK_CONTROL:
	case CEC_OPCODE_PLAY:
	case CEC_OPCODE_USER_CONTROL_PRESSED:
	case CEC_OPCODE_FEATURE_ABORT:
	case CEC_OPCODE_ABORT:
		if (broadcast)
			retval = 0;
		break;
	default:
		break;
	}

	return retval;
}

/**
 * handle key message.
 *
 * @param opcode	[in] opcode.
 * @param key		[in] received key
 *
 */
void CECHandleKey(unsigned char opcode, unsigned char key)
{
	int ret;
	int key_idx = sizeof(supportkeys)/sizeof(supportkeys[0]);

	while (--key_idx >= 0) {
		if (opcode == supportkeys[key_idx].opcode &&
		    key == supportkeys[key_idx].cec_id)
			break;
	}

	if (key_idx >= 0) {
		/* key is valid */
		_I("[CEC] %s(opcode 0x%X,key 0x%X(key code %d)",
		supportkeys[key_idx].key_name, (int)opcode, (int)key, supportkeys[key_idx].ui_id);
		ret = input_key_event(supportkeys[key_idx].opcode, supportkeys[key_idx].ui_id);
		if (ret < 0)
			_E("ioctl(CEC_IOC_HANDLEKEY) failed! (fd %d %d: err[%d])", key_fd, ret, errno);
	} else {
		_E("[CEC] 0x%X 0x%X is not supported key\n", opcode, key);
	}
}

/**
 * process CEC OneTouchPlay
 *
 * OneTouchPlay is disabled at normal case
 * Enable : "adb shell setprop persist.hdmi.onetouch_enabled 1"
 * send Text View On
 * send Active source
 */
void CECOneTouchPlay(unsigned char *buffer, int laddr, int paddr)
{
	int size = 0;
	unsigned char ldst = buffer[0] >> 4;
	unsigned char opcode = buffer[1];
	struct timespec time = {0, 500 * NANO_SECOND_MULTIPLIER};

	buffer[0] = (laddr << 4) | ldst;
	buffer[1] = CEC_OPCODE_TEXT_VIEW_ON;
	size = 2;
	_I("Tx : [CEC_OPCODE_TEXT_VIEW_ON]");

	if (size > 0) {
		if (CECSendMessage(buffer, size) != size)
			_E("CECSendMessage() failed!!!");
	}
	nanosleep(&time, NULL);

	buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
	buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
	buffer[2] = (paddr >> 8) & 0xFF;
	buffer[3] = paddr & 0xFF;
	size = 4;
	_I("Tx : [CEC_OPCODE_ACTIVE_SOURCE]");
	if (size > 0) {
		if (CECSendMessage(buffer, size) != size)
			_E("CECSendMessage() failed!!!");
	}
}

/**
 * process CEC message.
 *
 * @param buffer    [in/out] pointer to buffer address where message will be stored.
 * @param laddr		[in] logical address
 * @param paddr		[in] physical address
 *
 * @return return size of CEC message to send.
 */
int CECProcessOpcode(unsigned char *buffer, int laddr, int paddr, int raddr)
{
	int size = 0;
	unsigned char ldst = buffer[0] >> 4;
	unsigned char opcode = buffer[1];

	switch (opcode) {
	case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
		/* responce with "Report Physical Address" */
		buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
		buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
		buffer[2] = (paddr >> 8) & 0xFF;
		buffer[3] = paddr & 0xFF;
		buffer[4] = CEC_DEVICE_PLAYER;
		size = 5;
		break;
	case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
		_I("[CEC_OPCODE_REQUEST_ACTIVE_SOURCE 0x%X]", opcode);
		if (raddr != paddr) {
			_I("Not Currently active source  r:0x0%x  p:0x0%x", raddr, paddr);
			break;
		}
		buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
		buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
		buffer[2] = (paddr >> 8) & 0xFF;
		buffer[3] = paddr & 0xFF;
		size = 4;
		_I("Tx : [CEC_OPCODE_ACTIVE_SOURCE 0x%X]", CEC_OPCODE_ACTIVE_SOURCE);
		break;
	case CEC_OPCODE_IMAGE_VIEW_ON:
	case CEC_OPCODE_TEXT_VIEW_ON:
		_I("[CEC_OPCODE_IMAGE_VIEW_ON 0x%X]", opcode);
		buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
		buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
		buffer[2] = (paddr >> 8) & 0xFF;
		buffer[3] = paddr & 0xFF;
		size = 4;
		_I("Tx : [CEC_OPCODE_ACTIVE_SOURCE 0x%X]", CEC_OPCODE_ACTIVE_SOURCE);
		break;
	case CEC_OPCODE_SET_STREAM_PATH:  /* 11.2.2-3 test */
		_I("[CEC_OPCODE_SET_STREAM_PATH 0x%X]", opcode);
		buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
		buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
#if 1
		buffer[2] = (paddr >> 8) & 0xFF;
		buffer[3] = paddr & 0xFF;
#endif
		size = 4;
		_I("Tx : [CEC_OPCODE_ACTIVE_SOURCE 0x%X]", CEC_OPCODE_ACTIVE_SOURCE);
		CECReportKey(KEY_HOMEPAGE);
		break;
	case CEC_OPCODE_MENU_REQUEST:
		_I("[CEC_OPCODE_MENU_REQUEST 0x%X]", opcode);
		buffer[0] = (laddr << 4) | ldst;
		buffer[1] = CEC_OPCODE_MENU_STATUS;
		buffer[2] = 0x0;/*active*/
		size = 3;
		_I("Tx : [CEC_OPCODE_MENU_STATUS 0x%X]", CEC_OPCODE_MENU_STATUS);
		break;
	case CEC_OPCODE_GET_DEVICE_VENDOR_ID:
		_I("[CEC_OPCODE_GET_DEVICE_VENDOR_ID 0x%X]", opcode);
		buffer[0] = (laddr << 4) | ldst;
		buffer[1] = CEC_OPCODE_DEVICE_VENDOR_ID;
		buffer[2] = 0x00;
		buffer[3] = 0x00;
		buffer[4] = 0xF0;
		size = 5;
		_I("Tx : [CEC_OPCODE_GET_DEVICE_VENDOR_ID 0x%X]", CEC_OPCODE_DEVICE_VENDOR_ID);
		break;
	case CEC_OPCODE_VENDOR_COMMAND_WITH_ID:
		_I("[CEC_OPCODE_VENDOR_COMMAND_WITH_ID 0x%X] : %2X%2X%2X", opcode,
		buffer[2], buffer[3], buffer[4]);
		break;
	case CEC_OPCODE_GET_CEC_VERSION:
		buffer[0] = (laddr << 4) | ldst;
		buffer[1] = CEC_OPCODE_CEC_VERSION;
		buffer[2] = 0x05;
		size = 3;
		_I("Tx : [CEC_OPCODE_CEC_VERSION 0x%X] : 0x%X", CEC_OPCODE_CEC_VERSION, buffer[2]);
		break;
	case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
		_I("[CEC_OPCODE_GIVE_DEVICE_POWER_STATUS 0x%X]", opcode);
		buffer[0] = (laddr << 4) | ldst;
		buffer[1] = CEC_OPCODE_REPORT_POWER_STATUS;
		buffer[2] = 0x0;
		size = 3;
		_I("Tx : [CEC_OPCODE_REPORT_POWER_STATUS 0x%X]", CEC_OPCODE_REPORT_POWER_STATUS);
		break;
	case CEC_OPCODE_REPORT_POWER_STATUS:
		_I("[CEC_OPCODE_REPORT_POWER_STATUS 0x%X]", opcode);
		CECOneTouchPlay(buffer, laddr, paddr);
		break;
	case CEC_OPCODE_STANDBY:
		_I("CEC_OPCODE_STANDBY 0x%X", opcode);
		CECHandleKey(opcode, buffer[1]);
		break;
	case CEC_OPCODE_ROUTING_CHANGE:
		_I("CEC_OPCODE_ROUTING_CHANGE 0x%X r:0x0%x  p:0x0%x", opcode, raddr, paddr);
		if (paddr != raddr) {
			CECHandleKey(opcode, buffer[1]);
			_I("CEC_OPCODE_ROUTING_CHANGE Send HomeKey");
		}
		break;
	case CEC_OPCODE_GIVE_OSD_NAME:
		_I("CEC_OPCODE_GIVE_OSD_NAME 0x%X", opcode);
		buffer[0] = (laddr << 4) | ldst;
		buffer[1] = CEC_OPCODE_SET_OSD_NAME;
		buffer[2] = 'T';
		buffer[3] = 'i';
		buffer[4] = 'z';
		buffer[5] = 'e';
		buffer[6] = 'n';
		buffer[7] = 'G';
		buffer[8] = 'a';
		buffer[9] = 't';
		buffer[10] = 'e';
		buffer[11] = 'w';
		buffer[12] = 'a';
		buffer[13] = 'y';
		size = 14;
		_I("Tx : [CEC_OPCODE_SET_OSD_NAME 0x%X]", CEC_OPCODE_SET_OSD_NAME);
		break;
	case CEC_OPCODE_USER_CONTROL_PRESSED:
	case CEC_OPCODE_USER_CONTROL_RELEASED:
	case CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN:
	case CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP:
	case CEC_OPCODE_DECK_CONTROL:
	case CEC_OPCODE_PLAY:
		CECHandleKey(opcode, buffer[2]);
		break;
	case CEC_OPCODE_ABORT:
	case CEC_OPCODE_FEATURE_ABORT:
	default:
		/* send "Feature Abort" */
		buffer[0] = (laddr << 4) | ldst;
		buffer[1] = CEC_OPCODE_FEATURE_ABORT;
		buffer[2] = CEC_OPCODE_ABORT;
		buffer[3] = 0x04; /* "refused" */
		size = 4;
		break;
	}

	return size;
}
