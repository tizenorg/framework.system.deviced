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


/**
 * @file	core.c
 * @brief	Power manager main loop.
 *
 * This file includes Main loop, the FSM, signal processing.
 */
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vconf-keys.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "device-node.h"
#include "lock-detector.h"
#include "display-ops.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/udev.h"
#include "core/list.h"
#include "core/common.h"
#include "core/edbus-handler.h"
#include "core/config-parser.h"
#include "extcon/extcon.h"
#include "extcon/hall.h"
#include "battery/power-supply.h"
#include "dd-display.h"
#include "weaks.h"

#define PM_STATE_LOG_FILE		"/var/log/pm_state.log"
#define PM_STATE_LOWBAT_LOG_FILE	"/var/log/pm_state_lowbat.log"
#define DISPLAY_CONF_FILE		"/etc/deviced/display.conf"
#define LOG_SAVE_BATTERY_LEVEL		5
/**
 * @addtogroup POWER_MANAGER
 * @{
 */

#define SET_BRIGHTNESS_IN_BOOTLOADER	"/usr/bin/save_blenv SLP_LCD_BRIGHT"
#define LOCK_SCREEN_INPUT_TIMEOUT	10000
#define LOCK_SCREEN_CONTROL_TIMEOUT	5000
#define DD_LCDOFF_INPUT_TIMEOUT		3000
#define ALWAYS_ON_TIMEOUT			3600000

#define GESTURE_STR		"gesture"
#define POWER_KEY_STR		"powerkey"
#define TOUCH_STR		"touch"
#define EVENT_STR		"event"
#define TIMEOUT_STR		"timeout"
#define UNKNOWN_STR		"unknown"

unsigned int pm_status_flag;

static void (*power_saving_func) (int onoff);
static enum device_ops_status status = DEVICE_OPS_STATUS_UNINIT;

int pm_cur_state;
int pm_old_state;
Ecore_Timer *timeout_src_id;
static int pre_suspend_flag;
int system_wakeup_flag;
static unsigned int custom_normal_timeout;
static unsigned int custom_dim_timeout;
static int custom_holdkey_block;
static int custom_change_pid = -1;
static char *custom_change_name;
static Ecore_Timer *lock_timeout_id;
static int lock_screen_timeout = LOCK_SCREEN_INPUT_TIMEOUT;
static struct timeval lcdon_tv;
static int lcd_paneloff_mode;
static int stay_touchscreen_off;
static Eina_List *lcdon_ops;
static bool lcdon_broadcast;
static bool booting_flag;


/* default transition, action fuctions */
static int default_trans(int evt);
static int default_action(int timeout);
static int default_check(int next);

struct state states[S_END] = {
	{S_START, default_trans, default_action, default_check,},
	{S_NORMAL, default_trans, default_action, default_check,},
	{S_LCDDIM, default_trans, default_action, default_check,},
	{S_LCDOFF, default_trans, default_action, default_check,},
	{S_SLEEP, default_trans, default_action, default_check,}
};

static const char state_string[5][10] = {
	"S_START", "S_NORMAL", "S_LCDDIM", "S_LCDOFF", "S_SLEEP"
};

static int trans_table[S_END][EVENT_END] = {
	/* Timeout , Input */
	{S_START, S_START},	/* S_START */
	{S_LCDDIM, S_NORMAL},	/* S_NORMAL */
	{S_LCDOFF, S_NORMAL},	/* S_LCDDIM */
	{S_SLEEP, S_NORMAL},	/* S_LCDOFF */
	{S_LCDOFF, S_NORMAL},	/* S_SLEEP */
};

#define SHIFT_UNLOCK		4
#define MASK_RESET_TIMEOUT	0x8	/* 1000 */
#define MASK_MARGIN_TIMEOUT	(0x1 << 8)
#define SHIFT_CHANGE_STATE	7
#define CHANGE_STATE_BIT	0xF00	/* 1111 0000 0000 */
#define SHIFT_LOCK_FLAG	16
#define SHIFT_CHANGE_TIMEOUT	20
#define CUSTOM_TIMEOUT_BIT	0x1
#define CUSTOM_HOLDKEY_BIT	0x2
#define HOLD_KEY_BLOCK_BIT	0x1
#define STANDBY_MODE_BIT	0x2
#define TIMEOUT_NONE		(-1)

#define S_COVER_TIMEOUT			8000
#define GET_HOLDKEY_BLOCK_STATE(x) ((x >> SHIFT_LOCK_FLAG) & HOLD_KEY_BLOCK_BIT)
#define GET_STANDBY_MODE_STATE(x) ((x >> SHIFT_LOCK_FLAG) & STANDBY_MODE_BIT)
#define MASK32				0xffffffff
#define BOOTING_DONE_WATING_TIME	60000	/* 1 minute */
#define LOCK_TIME_WARNING		60	/* 60 seconds */
#define ALPM_CLOCK_WAITING_TIME		3000	/* 3 seconds */

#define ACTIVE_ACT "active"
#define INACTIVE_ACT "inactive"
#define SIGNAL_LCD_ON "LCDOn"
#define SIGNAL_LCD_OFF "LCDOff"
#define SIGNAL_LCD_OFF_COMPLETED "LCDOffCompleted"

#define LOCK_SCREEN_WATING_TIME		0.3	/* 0.3 second */
#define LONG_PRESS_INTERVAL             2       /* 2 seconds */
#define SAMPLING_INTERVAL		1	/* 1 sec */
#define BRIGHTNESS_CHANGE_STEP		10
#define HBM_LUX_THRESHOLD		32768	/* lux */
#define LCD_ALWAYS_ON			0
#define ACCEL_SENSOR_ON			1
#define CONTINUOUS_SAMPLING		1
#define LCDOFF_TIMEOUT			500	/* milli second */
#define DEFAULT_BRIGHTNESS		60
#define DEFAULT_PHASED_DELAY		35000	/* micro second */

#define DIFF_TIMEVAL_MS(a, b) \
	(((a.tv_sec * 1000000 + a.tv_usec) - \
	(b.tv_sec * 1000000 + b.tv_usec)) \
	/ 1000)

enum signal_type {
	SIGNAL_PRE,
	SIGNAL_POST,
};

struct display_config display_conf = {
	.lock_wait_time		= LOCK_SCREEN_WATING_TIME,
	.longpress_interval	= LONG_PRESS_INTERVAL,
	.lightsensor_interval	= SAMPLING_INTERVAL,
	.lcdoff_timeout		= LCDOFF_TIMEOUT,
	.brightness_change_step	= BRIGHTNESS_CHANGE_STEP,
	.hbm_lux_threshold	= HBM_LUX_THRESHOLD,
	.lcd_always_on		= LCD_ALWAYS_ON,
	.framerate_app		= {0, 0, 0, 0},
	.control_display	= 0,
	.powerkey_doublepress	= 0,
	.alpm_on		= 0,
	.accel_sensor_on	= ACCEL_SENSOR_ON,
	.continuous_sampling	= CONTINUOUS_SAMPLING,
	.default_brightness	= DEFAULT_BRIGHTNESS,
	.lcdon_direct		= 0,
	.phased_delay		= DEFAULT_PHASED_DELAY,
};

struct display_function_info display_info = {
	.update_auto_brightness		= NULL,
	.set_autobrightness_min		= NULL,
	.reset_autobrightness_min	= NULL,
	.face_detection			= NULL,
	.set_brightness_level		= NULL,
};

static struct display_dump_data {
	int *status;
	int *pre_suspend_flag;
} dump_data = {
	.status = (int *)&status,
	.pre_suspend_flag = &pre_suspend_flag,
};

typedef struct _pm_lock_node {
	pid_t pid;
	Ecore_Timer *timeout_id;
	time_t time;
	bool holdkey_block;
	struct _pm_lock_node *next;
} PmLockNode;

static PmLockNode *cond_head[S_END];

static void set_process_active(bool flag, pid_t pid)
{
	char str[6];
	char *arr[2];
	int ret;

	if (pid >= INTERNAL_LOCK_BASE)
		return;

	snprintf(str, sizeof(str), "%d", (int)pid);

	arr[0] = (flag ? ACTIVE_ACT : INACTIVE_ACT);
	arr[1] = str;

	/* Send dbug to resourced */
	ret = broadcast_edbus_signal(RESOURCED_PATH_PROCESS,
	    RESOURCED_INTERFACE_PROCESS, RESOURCED_METHOD_ACTIVE, "si", arr);
	if (ret < 0)
		_E("Fail to send dbus signal to resourced!!");
}

bool check_lock_state(int state)
{
	if (cond_head[state] != NULL)
		return true;

	return false;
}

void change_trans_table(int state, int next)
{
	if (state <= S_START || state >= S_END)
		return;

	if (next <= S_START || next >= S_END)
		return;

	trans_table[state][EVENT_TIMEOUT] = next;
}

static void broadcast_lcd_on(enum device_flags flags)
{
	char *arr[1];

	if (flags & LCD_ON_BY_GESTURE)
		arr[0] = GESTURE_STR;
	else if (flags & LCD_ON_BY_POWER_KEY)
		arr[0] = POWER_KEY_STR;
	else if (flags & LCD_ON_BY_EVENT)
		arr[0] = EVENT_STR;
	else if (flags & LCD_ON_BY_TOUCH)
		arr[0] = TOUCH_STR;
	else
		arr[0] = UNKNOWN_STR;

	_D("lcdstep : broadcast lcdon %s", arr[0]);
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_LCD_ON, "s", arr);
}

static void broadcast_lcd_off(int type, enum device_flags flags)
{
	char *arr[1];
	char *signal;

	if (type != SIGNAL_PRE && type != SIGNAL_POST) {
		_E("invalid signal type %d", type);
		return;
	}

	if (type == SIGNAL_PRE)
		signal = SIGNAL_LCD_OFF;
	else
		signal = SIGNAL_LCD_OFF_COMPLETED;

	if (flags & LCD_OFF_BY_POWER_KEY)
		arr[0] = POWER_KEY_STR;
	else if (flags & LCD_OFF_BY_TIMEOUT)
		arr[0] = TIMEOUT_STR;
	else if (flags & LCD_OFF_BY_EVENT)
		arr[0] = EVENT_STR;
	else
		arr[0] = UNKNOWN_STR;

	_D("lcdstep : broadcast %s %s", signal, arr[0]);
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    signal, "s", arr);
}

void broadcast_lcd_off_late(enum device_flags flags)
{
	static enum device_flags late_flags;

	if (flags & LCD_OFF_LATE_MODE)
		broadcast_lcd_off(SIGNAL_POST, late_flags);
	else
		late_flags = flags;
}

static unsigned long get_lcd_on_flags(void)
{
	unsigned long flags = NORMAL_MODE;

	if (lcd_paneloff_mode)
		flags |= LCD_PANEL_OFF_MODE;

	if (stay_touchscreen_off)
		flags |= TOUCH_SCREEN_OFF_MODE;

	if (get_ambient_mode != NULL &&
	    get_ambient_mode() == true) {
		flags |= AMBIENT_MODE;
		flags |= LCD_PHASED_TRANSIT_MODE;
	}
	return flags;
}

void lcd_on_procedure(int state, enum device_flags flag)
{
	Eina_List *l = NULL;
	const struct device_ops *ops = NULL;
	unsigned long flags = get_lcd_on_flags();
	flags |= flag;

	/* send LCDOn dbus signal */
	if (!lcdon_broadcast) {
		broadcast_lcd_on(flags);
		lcdon_broadcast = true;
	}

	/* AMOLED Low Power Mode off */
	if (flags & AMBIENT_MODE) {
		pm_unlock_internal(INTERNAL_LOCK_ALPM, LCD_OFF,
		    PM_SLEEP_MARGIN);

		if (alpm_get_state != NULL &&
		    alpm_get_state() == false &&
		    backlight_ops.get_lcd_power() == PM_LCD_POWER_ON)
			return;
		if (alpm_set_state != NULL &&
		    alpm_set_state(false) < 0)
			_E("Failed to ALPM off");
	}

	if (!(flags & LCD_PHASED_TRANSIT_MODE)) {
		/* Update brightness level */
		if (state == LCD_DIM)
			backlight_ops.dim();
		else if (state == LCD_NORMAL)
			backlight_ops.update();
	}

	if (flags & AMBIENT_MODE) {
		if (state == LCD_DIM)
			set_setting_pmstate(S_LCDDIM);
		else if (state == LCD_NORMAL)
			set_setting_pmstate(S_NORMAL);
	}

	EINA_LIST_FOREACH(lcdon_ops, l, ops)
		ops->start(flags);

	if (CHECK_OPS(keyfilter_ops, backlight_enable))
		keyfilter_ops->backlight_enable(true);
}

static inline unsigned long get_lcd_off_flags(void)
{
	unsigned long flags = NORMAL_MODE;

	if (get_ambient_mode != NULL &&
	    get_ambient_mode() == true)
		flags |= AMBIENT_MODE;

	if (flags & AMBIENT_MODE)
		flags |= LCD_PHASED_TRANSIT_MODE;

	return flags;
}

inline void lcd_off_procedure(enum device_flags flag)
{
	Eina_List *l = NULL;
	const struct device_ops *ops = NULL;
	unsigned long flags = get_lcd_off_flags();
	flags |= flag;

	if (get_standby_state && get_standby_state()) {
		_D("standby mode! lcd off logic is skipped");
		return;
	}
	/* AMOLED Low Power Mode on */
	if (flags & AMBIENT_MODE) {
		if (alpm_get_state != NULL &&
		    alpm_get_state() == true)
			return;
		if (alpm_set_state != NULL &&
		    alpm_set_state(true) < 0)
			_E("Failed to ALPM on!");

		/* hbm node is changed to off always
		 * if alpm state is on. */
		if (hbm_turn_off)
			hbm_turn_off();

		pm_lock_internal(INTERNAL_LOCK_ALPM, LCD_OFF,
		    STAY_CUR_STATE, ALPM_CLOCK_WAITING_TIME);
	}

	/*
	 * LCD OFF Normal procedure
	 * step1. broadcast lcd off (pre) signal with cause
	 * step2. lcd off (xset)
	 * step3. broadcast lcd off (post) signal
	 * step4. update pmstate (Not here)
	 *
	 * LCD OFF Ambient procedure
	 * step1. alpm node on
	 * step2. broadcast lcd off (pre) signal with cause
	 * step3. brightness down
	 * step4. update pmstate
	 * step5. lcd off (xset)
	 * step6. broadcast lcd off (post) signal
	 */
	if (lcdon_broadcast) {
		broadcast_lcd_off(SIGNAL_PRE, flags);
		lcdon_broadcast = false;
	}
	if (CHECK_OPS(keyfilter_ops, backlight_enable))
		keyfilter_ops->backlight_enable(false);

	EINA_LIST_REVERSE_FOREACH(lcdon_ops, l, ops)
		ops->stop(flags);

	if (flags & AMBIENT_MODE)
		/*
		 * In case of ambient mode, Don't broadcast the signal here.
		 * Just Save the flag and send the signal after real xset off.
		 * real send logic is in alpm.c
		 */
		broadcast_lcd_off_late(flags);
	else
		broadcast_lcd_off(SIGNAL_POST, flags);
}

void set_stay_touchscreen_off(int val)
{
	_D("stay touch screen off : %d", val);
	stay_touchscreen_off = val;

	lcd_on_procedure(LCD_NORMAL, LCD_ON_BY_EVENT);
	set_setting_pmstate(LCD_NORMAL);
}

void set_lcd_paneloff_mode(int val)
{
	_D("lcd paneloff mode : %d", val);
	lcd_paneloff_mode = val;

	lcd_on_procedure(LCD_NORMAL, LCD_ON_BY_EVENT);
	set_setting_pmstate(LCD_NORMAL);
}

int low_battery_state(int val)
{
	switch (val) {
	case VCONFKEY_SYSMAN_BAT_POWER_OFF:
	case VCONFKEY_SYSMAN_BAT_CRITICAL_LOW:
	case VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF:
		return true;
	}
	return false;
}

static int refresh_app_cond()
{
	trans_condition = 0;

	if (cond_head[S_LCDDIM] != NULL)
		trans_condition = trans_condition | MASK_DIM;
	if (cond_head[S_LCDOFF] != NULL)
		trans_condition = trans_condition | MASK_OFF;
	if (cond_head[S_SLEEP] != NULL)
		trans_condition = trans_condition | MASK_SLP;

	return 0;
}

static PmLockNode *find_node(enum state_t s_index, pid_t pid)
{
	PmLockNode *t = cond_head[s_index];

	while (t != NULL) {
		if (t->pid == pid)
			break;
		t = t->next;
	}
	return t;
}

static PmLockNode *add_node(enum state_t s_index, pid_t pid, Ecore_Timer *timeout_id,
		bool holdkey_block)
{
	PmLockNode *n;
	time_t now;

	n = (PmLockNode *) malloc(sizeof(PmLockNode));
	if (n == NULL) {
		_E("Not enough memory, add cond. fail");
		return NULL;
	}

	time(&now);
	n->pid = pid;
	n->timeout_id = timeout_id;
	n->time = now;
	n->holdkey_block = holdkey_block;
	n->next = cond_head[s_index];
	cond_head[s_index] = n;

	refresh_app_cond();
	return n;
}

static int del_node(enum state_t s_index, PmLockNode *n)
{
	PmLockNode *t;
	PmLockNode *prev;

	if (n == NULL)
		return 0;

	t = cond_head[s_index];
	prev = NULL;
	while (t != NULL) {
		if (t == n) {
			if (prev != NULL)
				prev->next = t->next;
			else
				cond_head[s_index] = cond_head[s_index]->next;
			/* delete timer */
			if (t->timeout_id)
				ecore_timer_del(t->timeout_id);
			free(t);
			break;
		}
		prev = t;
		t = t->next;
	}
	refresh_app_cond();
	return 0;
}

static void print_node(int next)
{
	PmLockNode *n;
	char buf[30];
	time_t now;
	double diff;

	if (next <= S_START || next >= S_END)
		return;

	time(&now);
	n = cond_head[next];
	while (n != NULL) {
		diff = difftime(now, n->time);
		ctime_r(&n->time, buf);

		if (diff > LOCK_TIME_WARNING)
			_W("over %.0f s, pid: %5d, lock time: %s", diff, n->pid, buf);
		else
			_I("pid: %5d, lock time: %s", n->pid, buf);

		n = n->next;
	}
}

void get_pname(pid_t pid, char *pname)
{
	char buf[PATH_MAX];
	int cmdline, r;

	if (pid >= INTERNAL_LOCK_BASE)
		snprintf(buf, PATH_MAX, "/proc/%d/cmdline", getpid());
	else
		snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);

	cmdline = open(buf, O_RDONLY);
	if (cmdline < 0) {
		pname[0] = '\0';
		_E("%d does not exist now(may be dead without unlock)", pid);
		return;
	}

	r = read(cmdline, pname, PATH_MAX);
	if ((r >= 0) && (r < PATH_MAX))
		pname[r] = '\0';
	else
		pname[0] = '\0';

	close(cmdline);
}

static Eina_Bool del_dim_cond(void *data)
{
	PmLockNode *tmp = NULL;
	char pname[PATH_MAX];
	pid_t pid;

	if (!data)
		return EINA_FALSE;

	pid = (pid_t)((intptr_t)data);

	_I("delete prohibit dim condition by timeout (%d)", pid);

	tmp = find_node(S_LCDDIM, pid);
	del_node(S_LCDDIM, tmp);
	get_pname(pid, pname);
	set_unlock_time(pname, S_NORMAL);

	if (!timeout_src_id)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	return EINA_FALSE;
}

static Eina_Bool del_off_cond(void *data)
{
	PmLockNode *tmp = NULL;
	char pname[PATH_MAX];
	pid_t pid;

	if (!data)
		return EINA_FALSE;

	pid = (pid_t)((intptr_t)data);

	_I("delete prohibit off condition by timeout (%d)", pid);

	tmp = find_node(S_LCDOFF, pid);
	del_node(S_LCDOFF, tmp);
	get_pname(pid, pname);
	set_unlock_time(pname, S_LCDDIM);

	if (!timeout_src_id)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	return EINA_FALSE;
}

static Eina_Bool del_sleep_cond(void *data)
{
	PmLockNode *tmp = NULL;
	char pname[PATH_MAX];
	pid_t pid = (pid_t)data;

	if (!data)
		return EINA_FALSE;

	pid = (pid_t)((intptr_t)data);

	_I("delete prohibit sleep condition by timeout (%d)", pid);

	if (pid == INTERNAL_LOCK_ALPM &&
	    check_alpm_invalid_state != NULL)
		check_alpm_invalid_state();

	tmp = find_node(S_SLEEP, pid);
	del_node(S_SLEEP, tmp);
	get_pname(pid, pname);
	set_unlock_time(pname, S_LCDOFF);

	if (!timeout_src_id)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	set_process_active(EINA_FALSE, pid);

	return EINA_FALSE;
}

/* timeout handler  */
Eina_Bool timeout_handler(void *data)
{
	_I("Time out state %s\n", state_string[pm_cur_state]);

	if (timeout_src_id) {
		ecore_timer_del(timeout_src_id);
		timeout_src_id = NULL;
	}

	states[pm_cur_state].trans(EVENT_TIMEOUT);
	return EINA_FALSE;
}

void reset_timeout(int timeout)
{
	if (timeout_src_id != 0) {
		ecore_timer_del(timeout_src_id);
		timeout_src_id = NULL;
	}
	if (trans_table[pm_cur_state][EVENT_TIMEOUT] == pm_cur_state)
		return;
	if (timeout > 0)
		timeout_src_id = ecore_timer_add(MSEC_TO_SEC((double)timeout),
		    (Ecore_Task_Cb)timeout_handler, NULL);
	else if (timeout == 0)
		states[pm_cur_state].trans(EVENT_TIMEOUT);
}

/* get configurations from setting */
static int get_lcd_timeout_from_settings(void)
{
	int i;
	int val = 0;
	int ret = -1;
	char *buf;

	for (i = 0; i < S_END; i++) {
		switch (states[i].state) {
		case S_NORMAL:
			ret = get_run_timeout(&val);
			if (ret != 0)
				val = DEFAULT_NORMAL_TIMEOUT;
			break;
		case S_LCDDIM:
			get_dim_timeout(&val);
			break;
		case S_LCDOFF:
			val = display_conf.lcdoff_timeout;
			break;
		default:
			/* This state doesn't need to set time out. */
			val = 0;
			break;
		}
		if (val > 0)
			states[i].timeout = val;

		_I("%s state : %d ms timeout", state_string[i],
			states[i].timeout);
	}

	return 0;
}

static void update_display_time(void)
{
	int ret, run_timeout, val, hallic_open;

	GET_DEVICE_STATUS("hall_ic", HALL_IC_OPENED, hallic_open);
	/* first priority : s cover */
	if (!hallic_open) {
		states[S_NORMAL].timeout = S_COVER_TIMEOUT;
		_I("S cover closed : timeout is set by normal(%d ms)",
		    S_COVER_TIMEOUT);
		return;
	}

	/* second priority : custom timeout */
	if (custom_normal_timeout > 0) {
		states[S_NORMAL].timeout = custom_normal_timeout;
		states[S_LCDDIM].timeout = custom_dim_timeout;
		_I("CUSTOM : timeout is set by normal(%d ms), dim(%d ms)",
		    custom_normal_timeout, custom_dim_timeout);
		return;
	}

#ifndef EMULATOR
	/* third priority : lock state */
	if ((get_lock_screen_state() == VCONFKEY_IDLE_LOCK) &&
	    !get_lock_screen_bg_state()) {
		if (pm_status_flag & SMARTSTAY_FLAG) {
			/* smart stay is on, timeout is always 5 seconds. */
			states[S_NORMAL].timeout = LOCK_SCREEN_CONTROL_TIMEOUT;
			_I("LOCK : timeout is set, smart stay timeout(%d ms)",
			    LOCK_SCREEN_CONTROL_TIMEOUT);
		} else {
			/* timeout is different according to key or event. */
			states[S_NORMAL].timeout = lock_screen_timeout;
			_I("LOCK : timeout is set by normal(%d ms)",
			    lock_screen_timeout);
		}
		return;
	}
#endif

	/* default setting */
	ret = get_run_timeout(&run_timeout);
	if (ret < 0 || run_timeout < 0) {
		_E("Can not get run timeout. set default %d ms",
		    DEFAULT_NORMAL_TIMEOUT);
		run_timeout = DEFAULT_NORMAL_TIMEOUT;
	}

	/* for sdk
	 * if the run_timeout is zero, it regards AlwaysOn state
	 */
	if (run_timeout == 0 || display_conf.lcd_always_on) {
		trans_table[S_NORMAL][EVENT_TIMEOUT] = S_NORMAL;
		run_timeout = ALWAYS_ON_TIMEOUT;
		_I("LCD Always On");
	} else
		trans_table[S_NORMAL][EVENT_TIMEOUT] = S_LCDDIM;

	states[S_NORMAL].timeout = run_timeout;

	get_dim_timeout(&val);
	states[S_LCDDIM].timeout = val;

	_I("Normal: NORMAL timeout is set by %d ms", states[S_NORMAL].timeout);
	_I("Normal: DIM timeout is set by %d ms", states[S_LCDDIM].timeout);
}

static void update_display_locktime(int time)
{
	lock_screen_timeout = time;
	update_display_time();
}


void set_dim_state(bool on)
{
	_I("dim state is %d", on);
	update_display_time();
	states[pm_cur_state].trans(EVENT_INPUT);
}

static int check_lcdon_waiting_state(void)
{
	int ret, state;

	ret = get_call_state(&state);
	if (ret >= 0 && state != VCONFKEY_CALL_OFF)
		return false;

	if (get_lock_screen_state() == VCONFKEY_IDLE_LOCK)
		return false;

	return true;
}

void lcd_on_direct(enum device_flags flags)
{
	if (power_ops.get_power_lock_support()
	    && pm_cur_state == S_SLEEP)
		power_ops.power_lock();

	if (pre_suspend_flag == true) {
		power_ops.post_resume();
		pre_suspend_flag = false;
	}

	if (display_conf.lcdon_direct || !check_lcdon_waiting_state()) {
		_D("lcd is on directly");
		gettimeofday(&lcdon_tv, NULL);
		lcd_on_procedure(LCD_NORMAL, flags);
	}
	reset_timeout(DD_LCDOFF_INPUT_TIMEOUT);
	update_display_locktime(LOCK_SCREEN_INPUT_TIMEOUT);
}

static inline bool check_lcd_on(void)
{
	if (lock_timeout_id != NULL)
		return false;

	if (backlight_ops.get_lcd_power() != PM_LCD_POWER_ON)
		return true;

	if (alpm_get_state != NULL && alpm_get_state() == true)
		return true;

	return false;
}

int custom_lcdon(int timeout)
{
	struct state *st;

	if (timeout <= 0)
		return -EINVAL;

	if (check_lcd_on() == true)
		lcd_on_direct(LCD_ON_BY_GESTURE);

	_I("custom lcd on %d ms", timeout);
	if (set_custom_lcdon_timeout(timeout) == true)
		update_display_time();

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = S_NORMAL;
	st = &states[pm_cur_state];

	/* enter action */
	if (st->action)
		st->action(st->timeout);

	return 0;
}

static int proc_change_state(unsigned int cond, pid_t pid)
{
	int next_state = 0;
	struct state *st;
	int i;
	char pname[PATH_MAX];

	for (i = S_NORMAL; i < S_END; i++) {
		if ((cond >> (SHIFT_CHANGE_STATE + i)) & 0x1) {
			next_state = i;
			break;
		}
	}

	get_pname(pid, pname);

	_I("Change State to %s (%d)", state_string[next_state], pid);

	if (next_state == S_NORMAL) {
		if (check_lcd_on() == true)
			lcd_on_direct(LCD_ON_BY_EVENT);
	} else if (next_state == S_LCDOFF) {
		if (backlight_ops.get_lcd_power() != PM_LCD_POWER_OFF)
			lcd_off_procedure(LCD_OFF_BY_EVENT);
	}

	if (next_state == S_LCDOFF)
		if (set_custom_lcdon_timeout(0) == true)
			update_display_time();

	switch (next_state) {
	case S_NORMAL:
		if (booting_flag)
			update_display_locktime(LOCK_SCREEN_CONTROL_TIMEOUT);
		/* fall through */
	case S_LCDDIM:
		/* fall through */
	case S_LCDOFF:
		/* state transition */
		pm_old_state = pm_cur_state;
		pm_cur_state = next_state;
		st = &states[pm_cur_state];

		/* pm state is updated to dim because of standby mode */
		if (get_standby_state && get_standby_state() &&
		    (pm_cur_state == S_LCDOFF))
			set_setting_pmstate(S_LCDDIM);

		/* enter action */
		if (st->action)
			st->action(st->timeout);
		break;
	case S_SLEEP:
		_I("Dangerous requests.");
		/* at first LCD_OFF and then goto sleep */
		/* state transition */
		pm_old_state = pm_cur_state;
		pm_cur_state = S_LCDOFF;
		st = &states[pm_cur_state];
		if (st->action)
			st->action(TIMEOUT_NONE);
		delete_condition(S_SLEEP);
		pm_old_state = pm_cur_state;
		pm_cur_state = S_SLEEP;
		st = &states[pm_cur_state];
		if (st->action)
			st->action(TIMEOUT_NONE);
		break;

	default:
		return -1;
	}

	return 0;
}

/* update transition condition for application requrements */
static int proc_condition(PMMsg *data)
{
	PmLockNode *tmp = NULL;
	unsigned int val = data->cond;
	pid_t pid = data->pid;
	Ecore_Timer *cond_timeout_id = NULL;
	bool holdkey_block = 0;
	int val_timeout;

	if (val == 0)
		return 0;
	/* for debug */
	char pname[PATH_MAX];
	time_t now;

	get_pname(pid, pname);
	val_timeout = val >> SHIFT_CHANGE_TIMEOUT;
	if (val_timeout & (CUSTOM_TIMEOUT_BIT | CUSTOM_HOLDKEY_BIT)) {
		if (data->timeout == 0 && data->timeout2 == 0) {
			_I("LCD timeout changed : default setting");
			get_lcd_timeout_from_settings();
#ifndef EMULATOR
			if (get_lock_screen_state() == VCONFKEY_IDLE_LOCK) {
				_I("LOCK state : setting lock timeout!");
				states[S_NORMAL].timeout = lock_screen_timeout;
			}
#endif
			custom_normal_timeout = custom_dim_timeout = 0;
			if (!(val_timeout & CUSTOM_HOLDKEY_BIT))
				custom_change_pid = -1;
		} else {
			_I("LCD timeout changed : normal(%d s), dim(%d s)",
				data->timeout, data->timeout2);
			custom_normal_timeout = SEC_TO_MSEC(data->timeout);
			states[S_NORMAL].timeout = custom_normal_timeout;
			custom_dim_timeout = SEC_TO_MSEC(data->timeout2);
			states[S_LCDDIM].timeout = custom_dim_timeout;
			custom_change_pid = pid;
		}

		if (val_timeout & CUSTOM_HOLDKEY_BIT) {
			custom_holdkey_block = true;
			custom_change_pid = pid;
			_I("hold key disabled !");
		} else {
			custom_holdkey_block = false;
			_I("hold key enabled !");
		}
	}

	if (val & MASK_DIM) {
		if (data->timeout > 0) {
			cond_timeout_id =
			    ecore_timer_add(MSEC_TO_SEC(data->timeout),
				    (Ecore_Task_Cb)del_dim_cond, (void *)((intptr_t)pid));
		}
		holdkey_block = GET_HOLDKEY_BLOCK_STATE(val);
		tmp = find_node(S_LCDDIM, pid);
		if (tmp == NULL) {
			add_node(S_LCDDIM, pid, cond_timeout_id, holdkey_block);
		} else {
			if (data->timeout > 0) {
				time(&now);
				tmp->time = now;
			}
			if (tmp->timeout_id) {
				ecore_timer_del(tmp->timeout_id);
				tmp->timeout_id = cond_timeout_id;
			}
			tmp->holdkey_block = holdkey_block;
		}
		/* for debug */
		_SD("[%s] locked by pid %d - process %s holdkeyblock %d\n",
		    "S_NORMAL", pid, pname, holdkey_block);
		set_lock_time(pname, S_NORMAL);
	}
	if (val & MASK_OFF) {
		if (data->timeout > 0) {
			cond_timeout_id =
			    ecore_timer_add(MSEC_TO_SEC(data->timeout),
				    (Ecore_Task_Cb)del_off_cond, (void *)((intptr_t)pid));
		}
		holdkey_block = GET_HOLDKEY_BLOCK_STATE(val);
		tmp = find_node(S_LCDOFF, pid);
		if (tmp == NULL) {
			add_node(S_LCDOFF, pid, cond_timeout_id, holdkey_block);
		} else {
			if (data->timeout > 0) {
				time(&now);
				tmp->time = now;
			}
			if (tmp->timeout_id) {
				ecore_timer_del(tmp->timeout_id);
				tmp->timeout_id = cond_timeout_id;
			}
			tmp->holdkey_block = holdkey_block;
		}
		/* for debug */
		_SD("[%s] locked by pid %d - process %s holdkeyblock %d\n",
		    "S_LCDDIM", pid, pname, holdkey_block);
		set_lock_time(pname, S_LCDDIM);
	}
	if (val & MASK_SLP) {
		/*
		 * pm-state must be changed to LCDOFF,
		 * to guarantee of LCDOFF-lock
		 * when system resumes from suspend state.
		 */
		if (pm_cur_state == S_SLEEP)
			proc_change_state(S_LCDOFF <<
			    (SHIFT_CHANGE_STATE + S_LCDOFF), getpid());
		if (data->timeout > 0) {
			cond_timeout_id =
			    ecore_timer_add(MSEC_TO_SEC(data->timeout),
				    (Ecore_Task_Cb)del_sleep_cond, (void *)((intptr_t)pid));
		}
		if (GET_STANDBY_MODE_STATE(val) && set_standby_mode)
			set_standby_mode(pid, true);
		tmp = find_node(S_SLEEP, pid);
		if (tmp == NULL) {
			add_node(S_SLEEP, pid, cond_timeout_id, 0);
		} else {
			if (data->timeout > 0) {
				time(&now);
				tmp->time = now;
			}
			if (tmp->timeout_id) {
				ecore_timer_del(tmp->timeout_id);
				tmp->timeout_id = cond_timeout_id;
			}
			tmp->holdkey_block = 0;
		}
		set_process_active(EINA_TRUE, pid);

		/* for debug */
		_SD("[%s] locked by pid %d - process %s\n", "S_LCDOFF", pid,
			pname);
		set_lock_time(pname, S_LCDOFF);
	}

	/* UNLOCK(GRANT) condition processing */
	val = val >> SHIFT_UNLOCK;
	if (val & MASK_DIM) {
		tmp = find_node(S_LCDDIM, pid);
		del_node(S_LCDDIM, tmp);
		_SD("[%s] unlocked by pid %d - process %s\n", "S_NORMAL",
			pid, pname);
		set_unlock_time(pname, S_NORMAL);
	}
	if (val & MASK_OFF) {
		tmp = find_node(S_LCDOFF, pid);
		del_node(S_LCDOFF, tmp);
		_SD("[%s] unlocked by pid %d - process %s\n", "S_LCDDIM",
			pid, pname);
		set_unlock_time(pname, S_LCDDIM);
	}
	if (val & MASK_SLP) {
		tmp = find_node(S_SLEEP, pid);
		del_node(S_SLEEP, tmp);
		if (get_standby_state && get_standby_state())
			set_standby_mode(pid, false);
		set_process_active(EINA_FALSE, pid);

		_SD("[%s] unlocked by pid %d - process %s\n", "S_LCDOFF",
			pid, pname);
		set_unlock_time(pname, S_LCDOFF);

		if (check_suspend_direct != NULL && check_suspend_direct(pid))
			return 0;
	}
	val = val >> 8;
	if (val != 0) {
		if ((val & 0x1)) {
			reset_timeout(states[pm_cur_state].timeout);
			_I("reset timeout (%d ms)",
			    states[pm_cur_state].timeout);
		}
	} else {
		/* guard time for suspend */
		if (pm_cur_state == S_LCDOFF) {
			reset_timeout(states[S_LCDOFF].timeout);
			_I("margin timeout (%d ms)",
			    states[S_LCDOFF].timeout);
		}
	}

	if (timeout_src_id == 0)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	return 0;
}

/* If some changed, return 1 */
int check_processes(enum state_t prohibit_state)
{
	PmLockNode *t = cond_head[prohibit_state];
	PmLockNode *tmp = NULL;
	int ret = 0;

	while (t != NULL) {
		if (t->pid < INTERNAL_LOCK_BASE && kill(t->pid, 0) == -1) {
			_E("%d process doesn't exist, delete the state %d ",
				t->pid, prohibit_state);
			if (t->pid == custom_change_pid) {
				get_lcd_timeout_from_settings();
				custom_normal_timeout = custom_dim_timeout = 0;
				custom_change_pid = -1;
			}
			if (get_standby_state && get_standby_state())
				set_standby_mode(t->pid, false);
			tmp = t;
			ret = 1;
		}
		t = t->next;

		if (tmp != NULL) {
			del_node(prohibit_state, tmp);
			tmp = NULL;
		}
	}

	return ret;
}

int check_holdkey_block(enum state_t state)
{
	PmLockNode *t = cond_head[state];
	int ret = 0;

	_I("check holdkey block : state of %s", state_string[state]);

	if (custom_holdkey_block == true) {
		_I("custom hold key blocked by pid(%d)",
			custom_change_pid);
		return 1;
	}

	while (t != NULL) {
		if (t->holdkey_block == true) {
			ret = 1;
			_I("Hold key blocked by pid(%d)!", t->pid);
			break;
		}
		t = t->next;
	}

	return ret;
}

int delete_condition(enum state_t state)
{
	PmLockNode *t = cond_head[state];
	int ret = 0;
	PmLockNode *tmp = NULL;
	pid_t pid;
	char pname[PATH_MAX];

	_I("delete condition : state of %s", state_string[state]);

	while (t != NULL) {
		if (t->timeout_id > 0) {
			ecore_timer_del(t->timeout_id);
			t->timeout_id = NULL;
		}
		tmp = t;
		t = t->next;
		pid = tmp->pid;
		if (state == S_SLEEP)
			set_process_active(EINA_FALSE, pid);
		_I("delete node of pid(%d)", pid);
		del_node(state, tmp);
		get_pname(pid, pname);
		set_unlock_time(pname, state-1);
	}

	return 0;
}

void print_info(FILE *fp)
{
	int s_index = 0;
	int i = 1, ret;
	char pname[PATH_MAX];

	LOG_DUMP(fp, "\n==========================================="
	    "===========================\n");
	LOG_DUMP(fp, "Timeout Info: Run[%dms] Dim[%dms] Off[%dms]\n",
	    states[S_NORMAL].timeout,
	    states[S_LCDDIM].timeout, states[S_LCDOFF].timeout);

	LOG_DUMP(fp, "Tran. Locked : %s %s %s\n",
	    (trans_condition & MASK_DIM) ? state_string[S_NORMAL] : "-",
	    (trans_condition & MASK_OFF) ? state_string[S_LCDDIM] : "-",
	    (trans_condition & MASK_SLP) ? state_string[S_LCDOFF] : "-");

	LOG_DUMP(fp, "Current State: %s\n", state_string[pm_cur_state]);
	LOG_DUMP(fp, "Current Lock Conditions:\n");

	for (s_index = S_NORMAL; s_index < S_END; s_index++) {
		PmLockNode *t;
		char time_buf[30];
		t = cond_head[s_index];

		while (t != NULL) {
			get_pname((pid_t)t->pid, pname);
			ctime_r(&t->time, time_buf);
			LOG_DUMP(fp, " %d: [%s] locked by pid %d %s %s",
			    i++, state_string[s_index - 1], t->pid, pname, time_buf);
			t = t->next;
		}
	}

	if (get_standby_state && get_standby_state())
		print_standby_mode(fp);

	print_lock_info_list(fp);

	pm_history_print(fp, 250);
}

/*
 * If path is NULL, Use default path.
 */
void save_display_log(char *path)
{
	FILE *fp;
	time_t now_time;
	char time_buf[30];
	char *fname;
	const struct device_ops *ops = NULL;

	ops = find_device("display");
	if (check_default(ops))
		return;

	_D("internal state is saved! %s", path);

	time(&now_time);
	ctime_r(&now_time, time_buf);

	fname = (!path ? PM_STATE_LOG_FILE : path);

	fp = fopen(fname, "w+");
	if (fp) {
		ops->dump(fp, 0, ops->dump_data);
		fclose(fp);
	}
}

/* SIGHUP signal handler
 * For debug... print info to syslog
 */
static void sig_hup(int signo)
{
	save_display_log(NULL);
}

static void sig_usr(int signo)
{
	pm_status_flag |= VCALL_FLAG;
}

int check_lcdoff_direct(void)
{
	int lock, hdmi = 0, cradle = DOCK_NONE, hallic_open;

	if (pm_old_state != S_NORMAL)
		return false;

	if (pm_cur_state != S_LCDDIM)
		return false;

	/*
	 * goto lcd dim state when battery health is bad
	 * and abnormal popup shows
	 */
	if ((pm_status_flag & DIMSTAY_FLAG))
		return false;

	/*
	 * LCD on -> off directly in ambient mode
	 */
	if (get_ambient_mode != NULL &&
	    get_ambient_mode() == true &&
	    check_lock_state(S_LCDOFF) == false) {
		return true;
	}

	if (get_standby_state && get_standby_state())
		return false;

	lock = get_lock_screen_state();
	GET_DEVICE_STATUS("hall_ic", HALL_IC_OPENED, hallic_open);

	if (lock != VCONFKEY_IDLE_LOCK && hallic_open)
		return false;

	hdmi = extcon_get_status(EXTCON_CABLE_HDMI);
	/* if hdmi is connected */
	if (hdmi)
		return false;

	cradle = extcon_get_status(EXTCON_CABLE_DOCK);
	/* if cradle is sound dock */
	if (cradle == DOCK_SOUND)
		return false;

	_D("Goto LCDOFF direct(%d,%d,%d)", lock, hdmi, cradle);

	return true;
}

int check_lcdoff_lock_state(void)
{
	if (cond_head[S_SLEEP] != NULL)
		return true;

	return false;
}

/*
 * default transition function
 *   1. call check
 *   2. transition
 *   3. call enter action function
 */
static int default_trans(int evt)
{
	struct state *st = &states[pm_cur_state];
	int next_state;
	int hallic_open;

	next_state = (enum state_t)trans_table[pm_cur_state][evt];

	/* check conditions */
	while (st->check && !st->check(next_state)) {
		/* There is a condition. */
		if (get_standby_state && get_standby_state()) {
			_D("standby mode, goto next_state %s",
			    state_string[next_state]);
			break;
		}
		_I("%s -> %s : check fail", state_string[pm_cur_state],
		       state_string[next_state]);
		if (!check_processes(next_state)) {
			/* this is valid condition - the application that sent the condition is running now. */
			return -1;
		}
	}

	GET_DEVICE_STATUS("hall_ic", HALL_IC_OPENED, hallic_open);

	/* smart stay */
	if (display_info.face_detection &&
	    (pm_status_flag & SMARTSTAY_FLAG) && hallic_open) {
		if (display_info.face_detection(evt, pm_cur_state, next_state))
			return 0;
	}

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = next_state;
	st = &states[pm_cur_state];

	/* enter action */
	if (st->action) {
		if (pm_cur_state == S_NORMAL || pm_cur_state == S_LCDOFF)
			if (set_custom_lcdon_timeout(0) == true)
				update_display_time();

		if (check_lcdoff_direct() == true) {
			/* enter next state directly */
			states[pm_cur_state].trans(EVENT_TIMEOUT);
		} else {
			st->action(st->timeout);
		}
	}

	return 0;
}

static Eina_Bool lcd_on_expired(void *data)
{
	int lock_state, ret;

	if (lock_timeout_id)
		lock_timeout_id = NULL;

	/* check state of lock */
	ret = vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);

	if (ret > 0 && lock_state == VCONFKEY_IDLE_LOCK)
		return EINA_FALSE;

	/* lock screen is not launched yet, but lcd is on */
	if (check_lcd_on() == true)
		lcd_on_procedure(LCD_NORMAL, NORMAL_MODE);

	return EINA_FALSE;
}

static inline void stop_lock_timer(void)
{
	if (lock_timeout_id) {
		ecore_timer_del(lock_timeout_id);
		lock_timeout_id = NULL;
	}
}

static void check_lock_screen(void)
{
	int lock_setting, lock_state, app_state, ret;

	stop_lock_timer();

	ret = get_call_state(&app_state);
	if (ret >= 0 && app_state != VCONFKEY_CALL_OFF)
		goto lcd_on;

	/* check setting of lock screen is enabled. */
	ret = vconf_get_int(VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT,
	    &lock_setting);

	if (ret < 0 || lock_setting == SETTING_SCREEN_LOCK_TYPE_NONE)
		goto lcd_on;

	/* check state of lock */
	ret = vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);

	if (ret < 0 || lock_state == VCONFKEY_IDLE_LOCK)
		goto lcd_on;

	/* Use time to check lock is done. */
	lock_timeout_id = ecore_timer_add(display_conf.lock_wait_time,
	    (Ecore_Task_Cb)lcd_on_expired, (void *)NULL);

	return;

lcd_on:
	if (check_lcd_on() == true)
		lcd_on_procedure(LCD_NORMAL, NORMAL_MODE);
}

/* default enter action function */
static int default_action(int timeout)
{
	int ret;
	int wakeup_count = -1;
	char buf[NAME_MAX];
	char *pkgname = NULL;
	int i = 0;
	int lock_state = -1;
	int app_state = -1;
	time_t now;
	double diff;
	static time_t last_update_time;
	static int last_timeout;
	struct timeval now_tv;

	if (status != DEVICE_OPS_STATUS_START) {
		_E("display is not started!");
		return -EINVAL;
	}

	if (pm_cur_state != S_SLEEP) {
		if (pm_cur_state == S_NORMAL &&
		    lcdon_tv.tv_sec != 0) {
			gettimeofday(&now_tv, NULL);
			timeout -= DIFF_TIMEVAL_MS(now_tv, lcdon_tv);
			lcdon_tv.tv_sec = 0;
		}
		/* set timer with current state timeout */
		reset_timeout(timeout);

		if (pm_cur_state == S_NORMAL) {
			time(&last_update_time);
			last_timeout = timeout;
		} else {
			_I("timout set: %s state %d ms",
			    state_string[pm_cur_state], timeout);
		}
	}

	if (pm_cur_state != pm_old_state && pm_cur_state != S_SLEEP) {
		if (power_ops.get_power_lock_support())
			power_ops.power_lock();
		if (pm_cur_state != S_LCDOFF &&
		    (get_ambient_mode == NULL ||
		    get_ambient_mode() == false))
			set_setting_pmstate(pm_cur_state);
		device_notify(DEVICE_NOTIFIER_LCD, &pm_cur_state);
	}

	if (pm_old_state == S_NORMAL && pm_cur_state != S_NORMAL) {
		time(&now);
		diff = difftime(now, last_update_time);
		_I("S_NORMAL is changed to %s [%d ms, %.0f s]",
		    state_string[pm_cur_state], last_timeout, diff);
	}

	switch (pm_cur_state) {
	case S_NORMAL:
		/*
		 * normal state : backlight on and restore
		 * the previous brightness
		 */
		if (pm_old_state == S_LCDOFF || pm_old_state == S_SLEEP) {
			if (pre_suspend_flag == true) {
				power_ops.post_resume();
				pre_suspend_flag = false;
			}
			check_lock_screen();
		} else if (pm_old_state == S_LCDDIM)
			backlight_ops.update();

		if (check_lcd_on() == true)
			lcd_on_procedure(LCD_NORMAL, NORMAL_MODE);
		if (set_standby_state)
			set_standby_state(false);
		break;

	case S_LCDDIM:
		if (pm_old_state == S_NORMAL &&
		    backlight_ops.get_custom_status())
			backlight_ops.save_custom_brightness();
		/* lcd dim state : dim the brightness */
		backlight_ops.dim();

		if (pm_old_state == S_LCDOFF || pm_old_state == S_SLEEP)
			lcd_on_procedure(LCD_DIM, NORMAL_MODE);
		if (set_standby_state)
			set_standby_state(false);
		break;

	case S_LCDOFF:
		if (pm_old_state != S_SLEEP && pm_old_state != S_LCDOFF) {
			stop_lock_timer();
			/* lcd off state : turn off the backlight */
			if (backlight_ops.get_lcd_power() != PM_LCD_POWER_OFF)
				lcd_off_procedure(LCD_OFF_BY_TIMEOUT);
			set_setting_pmstate(pm_cur_state);

			if (pre_suspend_flag == false) {
				pre_suspend_flag = true;
				power_ops.pre_suspend();
			}
		}

		if (backlight_ops.get_lcd_power() != PM_LCD_POWER_OFF
		    || lcd_paneloff_mode)
			lcd_off_procedure(LCD_OFF_BY_TIMEOUT);
		break;

	case S_SLEEP:
		if (pm_old_state != S_SLEEP && pm_old_state != S_LCDOFF)
			stop_lock_timer();

		if (backlight_ops.get_lcd_power() != PM_LCD_POWER_OFF)
			lcd_off_procedure(LCD_OFF_BY_TIMEOUT);

		if (!power_ops.get_power_lock_support()) {
			/* sleep state : set system mode to SUSPEND */
			if (device_get_property(DEVICE_TYPE_POWER,
			    PROP_POWER_WAKEUP_COUNT, &wakeup_count) < 0)
				_E("wakeup count read error");

			if (wakeup_count < 0) {
				_I("Wakup Event! Can not enter suspend mode.");
				goto go_lcd_off;
			}

			if (device_set_property(DEVICE_TYPE_POWER,
			    PROP_POWER_WAKEUP_COUNT, wakeup_count) < 0) {
				_E("wakeup count write error");
				goto go_lcd_off;
			}
		}
		goto go_suspend;
	}

	return 0;

go_suspend:
	pm_history_save(PM_LOG_SLEEP, pm_cur_state);
	if (power_ops.get_power_lock_support()) {
		if (power_ops.power_unlock() < 0)
			_E("power unlock state error!");
	} else {
		power_ops.suspend();
		_I("system wakeup!!");
		system_wakeup_flag = true;
		/* Resume !! */
		if (power_ops.check_wakeup_src() == EVENT_DEVICE)
			/* system waked up by devices */
			states[pm_cur_state].trans(EVENT_DEVICE);
		else
			/* system waked up by user input */
			states[pm_cur_state].trans(EVENT_INPUT);
	}
	return 0;

go_lcd_off:
	if (!power_ops.get_power_lock_support()) {
		/* Resume !! */
		states[pm_cur_state].trans(EVENT_DEVICE);
	}
	return 0;
}

/*
 * default check function
 *   return
 *    0 : can't transit, others : transitable
 */
static int default_check(int next)
{
	int trans_cond = trans_condition & MASK_BIT;
	int lock_state = -1;
	int app_state = -1;

	vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
	if (lock_state == VCONFKEY_IDLE_LOCK && next != S_SLEEP) {
		while (0) {
			get_call_state(&app_state);
			if (app_state != VCONFKEY_CALL_OFF)
				break;
			vconf_get_bool(VCONFKEY_ALARM_RINGING, &app_state);
			if (app_state == EINA_TRUE)
				break;
			_I("default_check:LOCK STATE, it's transitable");
			return 1;
		}
	}

	switch (next) {
	case S_LCDDIM:
		trans_cond = trans_cond & MASK_DIM;
		break;
	case S_LCDOFF:
		trans_cond = trans_cond & MASK_OFF;
		break;
	case S_SLEEP:
		trans_cond = trans_cond & MASK_SLP;
		break;
	default:		/* S_NORMAL is exceptional */
		trans_cond = 0;
		break;
	}

	if (trans_cond != 0) {
		print_node(next);
		return 0;
	}

	return 1;		/* transitable */
}

static void default_saving_mode(int onoff)
{
	if (onoff) {
		pm_status_flag |= PWRSV_FLAG;
		/* off hbm state, it's power saving mode */
		if (hbm_get_state != NULL &&
		    hbm_get_state() == true)
			hbm_set_state_with_timeout(false, 0);
	} else {
		pm_status_flag &= ~PWRSV_FLAG;
	}
	if (pm_cur_state == S_NORMAL)
		backlight_ops.update();
}

static int poll_callback(int condition, PMMsg *data)
{
	static time_t last_t;
	time_t now;

	if (status != DEVICE_OPS_STATUS_START) {
		_E("display logic is not started!");
		return -ECANCELED;
	}

	if (condition == INPUT_POLL_EVENT) {
		if (pm_cur_state == S_LCDOFF || pm_cur_state == S_SLEEP)
			_I("Power key input");
		time(&now);
		if (last_t != now ||
		    pm_cur_state == S_LCDOFF ||
		    pm_cur_state == S_SLEEP) {
			states[pm_cur_state].trans(EVENT_INPUT);
			last_t = now;
		}
	} else if (condition == PM_CONTROL_EVENT) {
		if (data->cond & MASK_BIT
			|| ((data->cond >> SHIFT_UNLOCK) & MASK_BIT)
			|| (data->cond >> SHIFT_CHANGE_TIMEOUT))
			proc_condition(data);

		if (data->cond & CHANGE_STATE_BIT)
			proc_change_state(data->cond, data->pid);
	}

	return 0;
}

static int update_setting(int key_idx, int val)
{
	char buf[PATH_MAX];
	int ret = -1;
	int run_timeout = -1;

	switch (key_idx) {
	case SETTING_TO_NORMAL:
		update_display_time();
		states[pm_cur_state].trans(EVENT_INPUT);
		break;
	case SETTING_LOW_BATT:
		if (low_battery_state(val)) {
			if (!(pm_status_flag & CHRGR_FLAG))
				power_saving_func(true);
			pm_status_flag |= LOWBT_FLAG;
		} else {
			if (pm_status_flag & PWRSV_FLAG)
				power_saving_func(false);
			pm_status_flag &= ~LOWBT_FLAG;
			pm_status_flag &= ~BRTCH_FLAG;
		}
		break;
	case SETTING_CHARGING:
		if (val) {
			if (pm_status_flag & LOWBT_FLAG) {
				power_saving_func(false);
				pm_status_flag &= ~LOWBT_FLAG;
			}
			pm_status_flag |= CHRGR_FLAG;
		} else {
			int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
			vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW,
				&bat_state);
			if (low_battery_state(bat_state)) {
				power_saving_func(true);
				pm_status_flag |= LOWBT_FLAG;
			}
			pm_status_flag &= ~CHRGR_FLAG;
		}
		break;
	case SETTING_BRT_LEVEL:
		if (pm_status_flag & PWRSV_FLAG) {
			pm_status_flag |= BRTCH_FLAG;
			_I("brightness changed in low battery,escape dim state");
		}
		pm_status_flag &= ~EXTBRT_FLAG;
		backlight_ops.set_default_brt(val);
		device_notify(DEVICE_NOTIFIER_SETTING_BRT_CHANGED, &val);
		break;
	case SETTING_LOCK_SCREEN:
		set_lock_screen_state(val);
		if (val == VCONFKEY_IDLE_UNLOCK) {
			if (CHECK_OPS(keyfilter_ops, backlight_enable))
				keyfilter_ops->backlight_enable(false);
		}

		/* LCD on if lock screen show before waiting time */
		if (pm_cur_state == S_NORMAL &&
		    val == VCONFKEY_IDLE_LOCK &&
		    backlight_ops.get_lcd_power() != PM_LCD_POWER_ON)
			lcd_on_procedure(LCD_NORMAL, LCD_ON_BY_EVENT);
		stop_lock_timer();
		update_display_time();
		if (pm_cur_state == S_NORMAL)
			states[pm_cur_state].trans(EVENT_INPUT);
		break;
	case SETTING_LOCK_SCREEN_BG:
		set_lock_screen_bg_state(val);
		update_display_time();
		if (pm_cur_state == S_NORMAL)
			states[pm_cur_state].trans(EVENT_INPUT);
		break;
	case SETTING_POWEROFF:
		switch (val) {
		case VCONFKEY_SYSMAN_POWER_OFF_NONE:
		case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
			pm_status_flag &= ~PWROFF_FLAG;
			break;
		case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
		case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
			pm_status_flag |= PWROFF_FLAG;
			break;
		}
		break;

	default:
		return -1;
	}
	return 0;
}

static void check_seed_status(void)
{
	int ret = -1;
	int tmp = 0;
	int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
	int max_brt = 0;
	int brt = 0;
	int lock_state = -1;

	/* Charging check */
	if ((get_charging_status(&tmp) == 0) && (tmp > 0))
		pm_status_flag |= CHRGR_FLAG;

	ret = get_setting_brightness(&tmp);
	if (ret != 0 || (tmp < PM_MIN_BRIGHTNESS || tmp > PM_MAX_BRIGHTNESS)) {
		_I("fail to read vconf value for brightness");
		brt = PM_DEFAULT_BRIGHTNESS;
		if (tmp < PM_MIN_BRIGHTNESS || tmp > PM_MAX_BRIGHTNESS)
			vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt);
		tmp = brt;
	}
	_I("Set brightness from Setting App. %d", tmp);
	backlight_ops.set_default_brt(tmp);

	vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state);
	if (low_battery_state(bat_state)) {
		if (!(pm_status_flag & CHRGR_FLAG)) {
			power_saving_func(true);
			pm_status_flag |= LOWBT_FLAG;
		}
	}

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &tmp);
	if (ret == 0 && tmp == SETTING_BRIGHTNESS_AUTOMATIC_OFF)
		backlight_ops.update();
	else if (ret == 0 && tmp == SETTING_BRIGHTNESS_AUTOMATIC_ON)
		backlight_ops.set_default_brt(PM_MAX_BRIGHTNESS);

	lcd_on_procedure(LCD_NORMAL, LCD_ON_BY_EVENT);

	/* lock screen check */
	ret = vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
	set_lock_screen_state(lock_state);
#ifndef EMULATOR
	if (lock_state == VCONFKEY_IDLE_LOCK) {
		states[S_NORMAL].timeout = lock_screen_timeout;
		_I("LCD NORMAL timeout is set by %d ms for lock screen",
		    lock_screen_timeout);
	}
#endif

	return;
}

static void init_lcd_operation(void)
{
	const struct device_ops *ops = NULL;

	ops = find_device("display");
	if (!check_default(ops))
		EINA_LIST_APPEND(lcdon_ops, ops);

	ops = find_device("touchscreen");
	if (!check_default(ops))
		EINA_LIST_APPEND(lcdon_ops, ops);

	ops = find_device("touchkey");
	if (!check_default(ops))
		EINA_LIST_APPEND(lcdon_ops, ops);

	ops = find_device("rotary");
	if (!check_default(ops))
		EINA_LIST_APPEND(lcdon_ops, ops);
}

static void exit_lcd_operation(void)
{
	Eina_List *l = NULL;
	Eina_List *l_next = NULL;
	const struct device_ops *ops = NULL;

	EINA_LIST_FOREACH_SAFE(lcdon_ops, l, l_next, ops)
		EINA_LIST_REMOVE_LIST(lcdon_ops, l);
}

enum {
	INIT_SETTING = 0,
	INIT_INTERFACE,
	INIT_POLL,
	INIT_FIFO,
	INIT_DBUS,
	INIT_END
};

static const char *errMSG[INIT_END] = {
	[INIT_SETTING] = "setting init error",
	[INIT_INTERFACE] = "lowlevel interface(sysfs or others) init error",
	[INIT_POLL] = "input devices poll init error",
	[INIT_FIFO] = "FIFO poll init error",
	[INIT_DBUS] = "d-bus init error",
};

static void esd_action()
{
	_I("ESD on");

	if (pm_cur_state == S_NORMAL) {
		backlight_ops.off(NORMAL_MODE);
		backlight_ops.on(NORMAL_MODE);
	} else if (pm_cur_state == S_LCDDIM) {
		backlight_ops.off(NORMAL_MODE);
		backlight_ops.dim();
	} else if (alpm_get_state != NULL &&
	    alpm_get_state() == true) {
		proc_change_state(S_NORMAL <<
		    (SHIFT_CHANGE_STATE + S_NORMAL), getpid());
		backlight_ops.off(NORMAL_MODE);
		backlight_ops.on(NORMAL_MODE);
	}
}

int set_lcd_timeout(int on, int dim, int holdkey_block, char *name)
{
	if (on == 0 && dim == 0) {
		_I("LCD timeout changed : default setting");
		custom_normal_timeout = custom_dim_timeout = 0;
	} else if (on < 0 || dim < 0) {
		_E("fail to set value (%d,%d)", on, dim);
		return -EINVAL;
	} else {
		_I("LCD timeout changed : on(%ds), dim(%ds)", on, dim);
		custom_normal_timeout = SEC_TO_MSEC(on);
		custom_dim_timeout = SEC_TO_MSEC(dim);
	}
	/* Apply new backlight time */
	update_display_time();
	if (pm_cur_state == S_NORMAL)
		states[pm_cur_state].trans(EVENT_INPUT);

	if (holdkey_block) {
		custom_holdkey_block = true;
		_I("hold key disabled !");
	} else {
		custom_holdkey_block = false;
		_I("hold key enabled !");
	}

	if (custom_change_name) {
		free(custom_change_name);
		custom_change_name = 0;
	}

	if (custom_normal_timeout == 0 &&
	    custom_dim_timeout == 0 &&
	    !holdkey_block)
		return 0;

	custom_change_name = strndup(name, strlen(name));
	if (!custom_change_name) {
		_E("Malloc falied!");
		custom_normal_timeout = custom_dim_timeout = 0;
		custom_holdkey_block = false;
		return -ENOMEM;
	}

	return 0;
}

void reset_lcd_timeout(const char *sender, void *data)
{
	if (!sender)
		return;

	if (!custom_change_name)
		return;

	if (strcmp(sender, custom_change_name))
		return;

	_I("reset lcd timeout %s: set default timeout", sender);

	free(custom_change_name);
	custom_change_name = 0;
	custom_normal_timeout = custom_dim_timeout = 0;
	custom_holdkey_block = false;

	update_display_time();
	if (pm_cur_state == S_NORMAL)
		states[pm_cur_state].trans(EVENT_INPUT);
}

static int hall_ic_open(void *data)
{
	int open;

	if (!data)
		return -EINVAL;

	open = *(int *)data;

	update_display_time();

	if (pm_cur_state == S_NORMAL || pm_cur_state == S_LCDDIM)
		states[pm_cur_state].trans(EVENT_INPUT);
	else if (pm_cur_state == S_SLEEP && open)
		proc_change_state(S_LCDOFF <<
		    (SHIFT_CHANGE_STATE + S_LCDOFF), getpid());

	if (display_info.update_auto_brightness)
		display_info.update_auto_brightness(false);

	return 0;
}

static int booting_done(void *data)
{
	if (booting_flag)
		return 0;

	booting_flag = true;

	return 0;
}

static int battery_present_changed(void *data)
{
	int present = (int)data;

	if (present == PRESENT_NORMAL) {
		_D("battery present good");
		pm_status_flag &= ~DIMSTAY_FLAG;

	} else if (present == PRESENT_ABNORMAL) {
		_D("battery present bad");
		pm_status_flag |= DIMSTAY_FLAG;
	}

	return 0;
}

static int battery_health_changed(void *data)
{
	int health = (int)data;

	if (health == HEALTH_GOOD) {
		_D("battery health good");
		pm_status_flag &= ~DIMSTAY_FLAG;

	} else if (health == HEALTH_BAD) {
		_D("battery health bad");
		pm_status_flag |= DIMSTAY_FLAG;
	}

	return 0;
}

static int battery_critical_popup(void *data)
{
	int level = (int)data;

	if (!eng_mode())
		return 0;

	if (pm_status_flag & CHRGR_FLAG)
		return 0;

	if (level == LOG_SAVE_BATTERY_LEVEL)
		save_display_log(PM_STATE_LOWBAT_LOG_FILE);

	return 0;
}

static int process_background(void *data)
{
	pid_t pid;
	PmLockNode *node = NULL;

	pid = *(pid_t *)data;

	node = find_node(S_LCDDIM, pid);
	if (node) {
		del_node(S_LCDDIM, node);
		_I("%d pid is background, then unlock LCD_NORMAL", pid);
	}

	return 0;
}

static int display_load_config(struct parse_result *result, void *user_data)
{
	struct display_config *c = user_data;

	_D("%s,%s,%s", result->section, result->name, result->value);

	if (!c)
		return -EINVAL;

	if (!MATCH(result->section, "Display"))
		return 0;

	if (MATCH(result->name, "LockScreenWaitingTime")) {
		SET_CONF(c->lock_wait_time, atof(result->value));
		_D("lock wait time is %.3f", c->lock_wait_time);
	} else if (MATCH(result->name, "LongPressInterval")) {
		SET_CONF(c->longpress_interval, atof(result->value));
		_D("long press interval is %.3f", c->longpress_interval);
	} else if (MATCH(result->name, "LightSensorSamplingInterval")) {
		SET_CONF(c->lightsensor_interval, atof(result->value));
		_D("lightsensor interval is %.3f", c->lightsensor_interval);
	} else if (MATCH(result->name, "LCDOffTimeout")) {
		SET_CONF(c->lcdoff_timeout, atoi(result->value));
		_D("lcdoff timeout is %d ms", c->lcdoff_timeout);
	} else if (MATCH(result->name, "BrightnessChangeStep")) {
		SET_CONF(c->brightness_change_step, atoi(result->value));
		_D("brightness change step is %d", c->brightness_change_step);
	} else if (MATCH(result->name, "HBMLuxThreshold")) {
		SET_CONF(c->hbm_lux_threshold, atoi(result->value));
		_D("HBM lux threshold is %d", c->hbm_lux_threshold);
	} else if (MATCH(result->name, "LCDAlwaysOn")) {
		c->lcd_always_on = (MATCH(result->value, "yes") ? 1 : 0);
		_D("LCD always on is %d", c->lcd_always_on);
	} else if (MATCH(result->name, "ChangedFrameRateAllowed")) {
		if (strstr(result->value, "setting")) {
			c->framerate_app[REFRESH_SETTING] = 1;
			_D("framerate app is Setting");
		}
		if (strstr(result->value, "all")) {
			memset(c->framerate_app, 1, sizeof(c->framerate_app));
			_D("framerate app is All");
		}
	} else if (MATCH(result->name, "ControlDisplay")) {
		c->control_display = (MATCH(result->value, "yes") ? 1 : 0);
		_D("ControlDisplay is %d", c->control_display);
	} else if (MATCH(result->name, "PowerKeyDoublePressSupport")) {
		c->powerkey_doublepress = (MATCH(result->value, "yes") ? 1 : 0);
		_D("PowerKeyDoublePressSupport is %d", c->powerkey_doublepress);
	} else if (MATCH(result->name, "UseALPM")) {
		c->alpm_on = (MATCH(result->value, "yes") ? 1 : 0);
		_D("UseALPM is %d", c->alpm_on);
	} else if (MATCH(result->name, "AccelSensorOn")) {
		c->accel_sensor_on = (MATCH(result->value, "yes") ? 1 : 0);
		_D("AccelSensorOn is %d", c->accel_sensor_on);
	} else if (MATCH(result->name, "ContinuousSampling")) {
		c->continuous_sampling = (MATCH(result->value, "yes") ? 1 : 0);
		_D("ContinuousSampling is %d", c->continuous_sampling);
	} else if (MATCH(result->name, "DefaultBrightness")) {
		SET_CONF(c->default_brightness, atoi(result->value));
		_D("DefaultBrightness is %d", c->default_brightness);
	} else if (MATCH(result->name, "LCDOnDirect")) {
		c->lcdon_direct = (MATCH(result->value, "yes") ? 1 : 0);
		_D("LCDOnDirect is %d", c->lcdon_direct);
	} else if (MATCH(result->name, "PhasedDelay")) {
		SET_CONF(c->phased_delay, atoi(result->value));
		_D("PhasedDelay is %d", c->phased_delay);
	}

	return 0;
}

static void lcd_uevent_changed(struct udev_device *dev)
{
	const char *devpath;
	const char *action;

	devpath = udev_device_get_devpath(dev);
	if (!devpath)
		return;

	if (!fnmatch(LCD_ESD_PATH, devpath, 0)) {
		action = udev_device_get_action(dev);
		if (!strcmp(action, UDEV_CHANGE))
			esd_action();
	}
}

static const struct uevent_handler lcd_uevent_ops = {
	.subsystem   = LCD_EVENT_SUBSYSTEM,
	.uevent_func = lcd_uevent_changed,
};

/**
 * Power manager Main
 *
 */
static void display_init(void *data)
{
	int ret, i;
	unsigned int flags = (WITHOUT_STARTNOTI | FLAG_X_DPMS);
	int timeout = 0;

	_I("Start power manager");

	signal(SIGHUP, sig_hup);

	power_saving_func = default_saving_mode;

	/* load configutation */
	ret = config_parse(DISPLAY_CONF_FILE, display_load_config, &display_conf);
	if (ret < 0)
		_W("Failed to load %s, %s Use default value!",
		    DISPLAY_CONF_FILE, ret);

	register_kernel_uevent_control(&lcd_uevent_ops);

	register_notifier(DEVICE_NOTIFIER_HALLIC_OPEN, hall_ic_open);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_BATTERY_HEALTH, battery_health_changed);
	register_notifier(DEVICE_NOTIFIER_BATTERY_PRESENT, battery_present_changed);
	register_notifier(DEVICE_NOTIFIER_BATTERY_CRITICAL_POPUP, battery_critical_popup);
	register_notifier(DEVICE_NOTIFIER_PROCESS_BACKGROUND, process_background);

	for (i = INIT_SETTING; i < INIT_END; i++) {
		switch (i) {
		case INIT_SETTING:
			ret = init_setting(update_setting);
			break;
		case INIT_INTERFACE:
			get_lcd_timeout_from_settings();
			ret = init_sysfs(flags);
			break;
		case INIT_POLL:
			_I("input init");
			init_input(poll_callback);
			break;
		case INIT_DBUS:
			_I("dbus init");
			ret = init_pm_dbus();
			break;
		}
		if (ret != 0) {
			_E("%s", errMSG[i]);
			break;
		}
	}

	if (i == INIT_END) {
		display_ops_init(NULL);
		pm_history_init();
		init_lcd_operation();
		check_seed_status();

		if (display_conf.lcd_always_on) {
			_D("LCD always on!");
			trans_table[S_NORMAL][EVENT_TIMEOUT] = S_NORMAL;
		}

		if (flags & WITHOUT_STARTNOTI) {	/* start without noti */
			_I("Start Power managing without noti");
			pm_cur_state = S_NORMAL;
			set_setting_pmstate(pm_cur_state);

			timeout = states[S_NORMAL].timeout;
			/* check minimun lcd on time */
			if (timeout < DEFAULT_NORMAL_TIMEOUT)
				timeout = DEFAULT_NORMAL_TIMEOUT;

			reset_timeout(timeout);
			status = DEVICE_OPS_STATUS_START;
			/*
			 * Lock lcd off until booting is done.
			 * deviced guarantees all booting script is executing.
			 * Last script of booting unlocks this suspend blocking state.
			 */
			pm_lock_internal(INTERNAL_LOCK_BOOTING, LCD_OFF,
			    STAY_CUR_STATE, BOOTING_DONE_WATING_TIME);
		}
		if (CHECK_OPS(keyfilter_ops, init))
			keyfilter_ops->init();
	}
}

static void display_exit(void *data)
{
	int i = INIT_END;

	status = DEVICE_OPS_STATUS_STOP;

	/* Set current state to S_NORMAL */
	pm_cur_state = S_NORMAL;
	set_setting_pmstate(pm_cur_state);
	/* timeout is not needed */
	reset_timeout(TIMEOUT_NONE);

	if (CHECK_OPS(keyfilter_ops, exit))
		keyfilter_ops->exit();

	unregister_kernel_uevent_control(&lcd_uevent_ops);

	display_ops_exit(NULL);

	for (i = i - 1; i >= INIT_SETTING; i--) {
		switch (i) {
		case INIT_SETTING:
			exit_setting();
			break;
		case INIT_INTERFACE:
			exit_sysfs();
			break;
		case INIT_POLL:
			unregister_notifier(DEVICE_NOTIFIER_HALLIC_OPEN, hall_ic_open);
			unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE,
			    booting_done);
			unregister_notifier(DEVICE_NOTIFIER_BATTERY_HEALTH,
			    battery_health_changed);
			unregister_notifier(DEVICE_NOTIFIER_BATTERY_PRESENT, battery_present_changed);
			unregister_notifier(DEVICE_NOTIFIER_BATTERY_CRITICAL_POPUP,
			    battery_critical_popup);
			unregister_notifier(DEVICE_NOTIFIER_PROCESS_BACKGROUND,
			    process_background);

			exit_input();
			break;
		}
	}

	exit_lcd_operation();
	free_lock_info_list();

	_I("Stop power manager");
}

static int display_start(enum device_flags flags)
{
	/* NORMAL MODE */
	if (flags & NORMAL_MODE) {
		if (flags & LCD_PANEL_OFF_MODE) {
			/* standby on */
			backlight_ops.standby(true);
		} else {
			/* normal lcd on */
			backlight_ops.on(flags);
		}
		return 0;
	}

	/* CORE LOGIC MODE */
	if (!(flags & CORE_LOGIC_MODE))
		return 0;

	if (status == DEVICE_OPS_STATUS_START)
		return -EALREADY;

	display_init(NULL);

	return 0;
}

static int display_stop(enum device_flags flags)
{
	/* NORMAL MODE */
	if (flags & NORMAL_MODE) {
		backlight_ops.off(flags);
		return 0;
	}

	/* CORE LOGIC MODE */
	if (!(flags & CORE_LOGIC_MODE))
		return 0;

	if (status == DEVICE_OPS_STATUS_STOP)
		return -EALREADY;

	display_exit(NULL);

	return 0;
}

static int display_status(void)
{
	return status;
}

static int display_dump(FILE *fp, int mode, void *dump_data)
{
	struct display_dump_data *data = dump_data;
	time_t now_time;
	char time_buf[30];

	time(&now_time);
	ctime_r(&now_time, time_buf);

	LOG_DUMP(fp, "pm_state_log now-time : %d(s) %s\n", (int)now_time, time_buf);
	LOG_DUMP(fp, "pm_status_flag: %x\n", pm_status_flag);
	LOG_DUMP(fp, "screen lock status : %d\n", get_lock_screen_state());

	if (data)
		LOG_DUMP(fp, "status : %d presuspend flag: %d\n",
		    *data->status, *data->pre_suspend_flag);

	print_info(fp);

	return 0;
}

static const struct device_ops display_device_ops = {
	.priority = DEVICE_PRIORITY_HIGH,
	.name     = "display",
	.init     = display_init,
	.exit     = display_exit,
	.start    = display_start,
	.stop     = display_stop,
	.status   = display_status,
	.dump     = display_dump,
	.dump_data = &dump_data,
};

DEVICE_OPS_REGISTER(&display_device_ops)

/**
 * @}
 */
