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


#ifndef __EXTCON_H__
#define __EXTCON_H__

#include "core/common.h"

/**
 * Extcon cable name is shared with kernel extcon class.
 * So do not change below strings.
 */
#define EXTCON_CABLE_USB              "USB"
#define EXTCON_CABLE_USB_HOST         "USB-Host"
#define EXTCON_CABLE_TA               "TA"
#define EXTCON_CABLE_HDMI             "HDMI"
#define EXTCON_CABLE_DOCK             "Dock"
#define EXTCON_CABLE_MIC_IN           "Microphone"
#define EXTCON_CABLE_HEADPHONE_OUT    "Headphone"
/* Mobile specific:
 * Not standard extcon cable string, for kernel v3.0 */
#define EXTCON_CABLE_EARKEY           "earkey"
#define EXTCON_CABLE_HDCP             "hdcp"
#define EXTCON_CABLE_HDMI_AUDIO       "ch_hdmi_audio"
#define EXTCON_CABLE_KEYBOARD         "keyboard"

struct extcon_ops {
	const char *name;
	int status;
	void (*init)(void *data);
	void (*exit)(void *data);
	int (*update)(int status);
};

#define EXTCON_OPS_REGISTER(dev) \
static void __CONSTRUCTOR__ dev##_init(void) \
{ \
	add_extcon(&dev); \
} \
static void __DESTRUCTOR__ dev##_exit(void) \
{ \
	remove_extcon(&dev); \
}

void add_extcon(struct extcon_ops *dev);
void remove_extcon(struct extcon_ops *dev);

int extcon_get_status(const char *name);

enum cb_device {
	DEVICE_START	= 0,
	DEVICE_HDMI,
	DEVICE_HDCP,
	DEVICE_HDCP_HDMI,
	DEVICE_HDMI_AUDIO,
	DEVICE_MAX,
};

#define CONNECTED(val) ((val) != 0)

int register_extcon_cb(int device, int (*cb)(void * data));

#endif /* __EXTCON_H__ */
