/*
 * deviced
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/**
 * @file	device-interface.h
 * @brief	backlight, touch, power devices interface module header
 */
#ifndef __DEVICE_INTERFACE_H__
#define __DEVICE_INTERFACE_H__

#include <stdbool.h>
#include "core/devices.h"

#define FLAG_X_DPMS		0x2

#define DEFAULT_DISPLAY 0

#define PM_MAX_BRIGHTNESS       100
#define PM_MIN_BRIGHTNESS       1
#define PM_DEFAULT_BRIGHTNESS	60
#define PM_DIM_BRIGHTNESS	0

#define PM_LCD_POWER_ON		0
#define PM_LCD_POWER_OFF	4

#define PM_LCD_RETRY_CNT	3
#define STATUS_STANDBY		(STATUS_ON + 1)

#define DISP_INDEX_SHIFT	16
#define DISP_CMD(prop, index)	((index << DISP_INDEX_SHIFT) | prop)

/*
 * Event type enumeration
 */
enum {
	EVENT_TIMEOUT = 0,	/*< time out event from timer */
	EVENT_DEVICE = EVENT_TIMEOUT,	/*< wake up by devices except input devices */
	EVENT_INPUT,		/*< input event from noti service */
	EVENT_END,
};

extern int init_sysfs(unsigned int);
extern int exit_sysfs(void);

typedef struct _PMSys PMSys;
struct _PMSys {
	int def_brt;
	int dim_brt;

	int (*sys_power_state) (PMSys *, int);
	int (*sys_power_lock) (PMSys *, int);
	int (*sys_get_power_lock_support) (PMSys *);
	int (*sys_get_lcd_power) (PMSys *);
	int (*bl_onoff) (PMSys *, int, enum device_flags);
	int (*bl_brt) (PMSys *, int, int);
};

struct _backlight_ops {
	int (*off)(enum device_flags);
	int (*dim)(void);
	int (*on)(enum device_flags);
	int (*update)(void);
	int (*standby)(int);
	int (*hbm_off)(void);
	int (*set_default_brt)(int level);
	int (*get_lcd_power)(void);
	int (*set_current_brightness)(int level);
	int (*get_current_brightness)(void);
	int (*set_custom_status)(bool on);
	bool (*get_custom_status)(void);
	int (*save_custom_brightness)(void);
	int (*custom_update)(void);
	int (*set_force_brightness)(int level);
	int (*blink)(int timeout);
};

struct _power_ops {
	int (*suspend)(void);
	int (*pre_suspend)(void);
	int (*post_resume)(void);
	int (*power_lock)(void);
	int (*power_unlock)(void);
	int (*get_power_lock_support)(void);
	int (*check_wakeup_src)(void);
};

extern struct _backlight_ops backlight_ops;
extern struct _power_ops power_ops;

#endif

