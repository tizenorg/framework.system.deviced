/*
 * deviced
 *
 * Copyright (c) 2012 - 2014 Samsung Electronics Co., Ltd.
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

#include "usb-client.h"

enum usbclient_mode {
	USBCLIENT_MODE_NONE          = 0x00,
	USBCLIENT_MODE_MTP           = 0x01,
	USBCLIENT_MODE_ACM           = 0x02,
	USBCLIENT_MODE_SDB           = 0x04,
	USBCLIENT_MODE_RNDIS         = 0x08,
	USBCLIENT_MODE_DIAG          = 0x10,
	USBCLIENT_MODE_CONN_GADGET   = 0x20,
};

unsigned int get_current_usb_gadget_info(int mode)
{
	unsigned int gadgets = USBCLIENT_MODE_NONE;

	switch (mode) {
	case SET_USB_DEFAULT:
		gadgets |= USBCLIENT_MODE_MTP;
		gadgets |= USBCLIENT_MODE_ACM;
		gadgets |= USBCLIENT_MODE_CONN_GADGET;
		break;
	case SET_USB_SDB:
		gadgets |= USBCLIENT_MODE_MTP;
		gadgets |= USBCLIENT_MODE_ACM;
		gadgets |= USBCLIENT_MODE_SDB;
		gadgets |= USBCLIENT_MODE_CONN_GADGET;
		break;
	case SET_USB_SDB_DIAG:
		gadgets |= USBCLIENT_MODE_MTP;
		gadgets |= USBCLIENT_MODE_ACM;
		gadgets |= USBCLIENT_MODE_SDB;
		gadgets |= USBCLIENT_MODE_DIAG;
		break;
	case SET_USB_RNDIS:
	case SET_USB_RNDIS_TETHERING:
		gadgets |= USBCLIENT_MODE_RNDIS;
		break;
	case SET_USB_RNDIS_DIAG:
		gadgets |= USBCLIENT_MODE_RNDIS;
		gadgets |= USBCLIENT_MODE_DIAG;
		break;
	case SET_USB_RNDIS_SDB:
		gadgets |= USBCLIENT_MODE_RNDIS;
		gadgets |= USBCLIENT_MODE_SDB;
		break;
	case SET_USB_DIAG_SDB:
		gadgets |= USBCLIENT_MODE_DIAG;
		gadgets |= USBCLIENT_MODE_SDB;
		break;
	default:
		break;
	}

	return gadgets;
}

int media_device_state(void)
{
	int mode = get_current_usb_mode();
	int gadgets = get_current_usb_gadget_info(mode);

	if (gadgets & USBCLIENT_MODE_MTP)
		return MEDIA_DEVICE_ON;

	return MEDIA_DEVICE_OFF;
}
