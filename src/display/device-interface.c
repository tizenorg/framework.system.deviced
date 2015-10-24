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


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "core/edbus-handler.h"
#include "core/log.h"
#include "core/devices.h"
#include "util.h"
#include "device-interface.h"
#include "vconf.h"
#include "core.h"
#include "device-node.h"
#include "weaks.h"

#define TOUCH_ON	1
#define TOUCH_OFF	0

#define LCD_PHASED_MIN_BRIGHTNESS	20
#define LCD_PHASED_MAX_BRIGHTNESS	100
#define LCD_PHASED_CHANGE_STEP		5

#define SIGNAL_CURRENT_BRIGHTNESS	"CurrentBrightness"

static PMSys *pmsys;
struct _backlight_ops backlight_ops;
struct _power_ops power_ops;

#ifdef ENABLE_X_LCD_ONOFF
#include "x-lcd-on.c"
static bool x_dpms_enable;
#endif

static int power_lock_support = -1;
static int current_brightness = PM_MAX_BRIGHTNESS;
static bool custom_status;
static int custom_brightness;
static int force_brightness;

static int _bl_onoff(PMSys *p, int on, enum device_flags flags)
{
	int cmd;

	cmd = DISP_CMD(PROP_DISPLAY_ONOFF, DEFAULT_DISPLAY);
	return device_set_property(DEVICE_TYPE_DISPLAY, cmd, on);
}

static int _bl_brt(PMSys *p, int brightness, int delay)
{
	int ret = -1;
	int cmd;
	int prev;
	struct timespec time = {0,};

	if (delay > 0) {
		time.tv_nsec = delay * NANO_SECOND_MULTIPLIER;
		nanosleep(&time, NULL);
	}

	if (force_brightness > 0 && brightness != p->dim_brt) {
		_I("brightness(%d), force brightness(%d)",
		    brightness, force_brightness);
		brightness = force_brightness;
	}

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &prev);

	if (!ret && (brightness != prev))
		backlight_ops.set_current_brightness(brightness);

	/* Update device brightness */
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brightness);

	_I("set brightness %d, %d", brightness, ret);

	return ret;
}

static int _sys_power_state(PMSys *p, int state)
{
	if (state < POWER_STATE_SUSPEND || state > POWER_STATE_POST_RESUME)
		return 0;
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_STATE, state);
}

static int _sys_power_lock(PMSys *p, int state)
{
	if (state != POWER_LOCK && state != POWER_UNLOCK)
		return -1;
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_LOCK, state);
}

static int _sys_get_power_lock_support(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_LOCK_SUPPORT,
	    &value);

	if (ret < 0)
		return -1;

	if (value < 0)
		return 0;

	return value;
}

static int _sys_get_lcd_power(PMSys *p)
{
	int value = -1;
	int ret = -1;
	int cmd;

	cmd = DISP_CMD(PROP_DISPLAY_ONOFF, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &value);

	if (ret < 0 || value < 0)
		return -1;

	return value;
}

static void _init_bldev(PMSys *p, unsigned int flags)
{
	int ret;

	p->bl_brt = _bl_brt;
	p->bl_onoff = _bl_onoff;
#ifdef ENABLE_X_LCD_ONOFF
	if (flags & FLAG_X_DPMS) {
		p->bl_onoff = pm_x_set_lcd_backlight;
		x_dpms_enable = true;
	}
#endif
}

static void _init_pmsys(PMSys *p)
{
	char *val;

	p->dim_brt = PM_DIM_BRIGHTNESS;
	p->sys_power_state = _sys_power_state;
	p->sys_power_lock = _sys_power_lock;
	p->sys_get_power_lock_support = _sys_get_power_lock_support;
	p->sys_get_lcd_power = _sys_get_lcd_power;
}

static void *_system_suspend_cb(void *data)
{
	int ret;

	_I("enter system suspend");
	if (pmsys && pmsys->sys_power_state)
		ret = pmsys->sys_power_state(pmsys, POWER_STATE_SUSPEND);
	else
		ret = -EFAULT;

	if (ret < 0)
		_E("Failed to system suspend! %d", ret);

	return NULL;
}

static int system_suspend(void)
{
	pthread_t pth;
	int ret;

	ret = pthread_create(&pth, 0, _system_suspend_cb, (void *)NULL);
	if (ret < 0) {
		_E("pthread creation failed!, suspend directly!");
		_system_suspend_cb((void *)NULL);
	} else {
		pthread_join(pth, NULL);
	}

	return 0;
}

static int system_pre_suspend(void)
{
	_I("enter system pre suspend");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_PRE_SUSPEND);

	return 0;
}

static int system_post_resume(void)
{
	_I("enter system post resume");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_POST_RESUME);

	return 0;
}

static int system_power_lock(void)
{
	_I("system power lock");
	if (pmsys && pmsys->sys_power_lock)
		return pmsys->sys_power_lock(pmsys, POWER_LOCK);

	return 0;
}

static int system_power_unlock(void)
{
	_I("system power unlock");
	if (pmsys && pmsys->sys_power_lock)
		return pmsys->sys_power_lock(pmsys, POWER_UNLOCK);

	return 0;
}

static int system_get_power_lock_support(void)
{
	int value = -1;

	if (power_lock_support == -1) {
		if (pmsys && pmsys->sys_get_power_lock_support) {
			value = pmsys->sys_get_power_lock_support(pmsys);
			if (value == 1) {
					_I("system power lock : support");
					power_lock_support = 1;
			} else {
				_E("system power lock : not support");
				power_lock_support = 0;
			}
		} else {
			_E("system power lock : read fail");
			power_lock_support = 0;
		}
	}

	return power_lock_support;
}

static int get_lcd_power(void)
{
	if (pmsys && pmsys->sys_get_lcd_power)
		return pmsys->sys_get_lcd_power(pmsys);

	return -1;
}

static int backlight_hbm_off(void)
{
	int ret, state;
	static bool support = true;

	if (!support)
		return -EIO;

	ret = device_get_property(DEVICE_TYPE_DISPLAY,
	    PROP_DISPLAY_HBM_CONTROL, &state);
	if (ret < 0) {
		support = false;
		return ret;
	}

	if (state) {
		ret = device_set_property(DEVICE_TYPE_DISPLAY,
		    PROP_DISPLAY_HBM_CONTROL, 0);
		if (ret < 0)
			return ret;
		_D("hbm is off!");
	}
	return 0;
}
void change_brightness(int start, int end, int step)
{
	int diff, val;
	int ret = -1;
	int cmd;
	int prev;

	if ((pm_status_flag & PWRSV_FLAG) &&
	    !(pm_status_flag & BRTCH_FLAG))
		return;

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &prev);

	if (prev == end)
		return;

	_D("start %d end %d step %d", start, end, step);

	diff = end - start;

	if (abs(diff) < step)
		val = (diff > 0 ? 1 : -1);
	else
		val = (int)ceil(diff / step);

	while (start != end) {
		if (val == 0) break;

		start += val;
		if ((val > 0 && start > end) ||
		    (val < 0 && start < end))
			start = end;

		pmsys->bl_brt(pmsys, start, display_conf.phased_delay);
	}
}

static int backlight_on(enum device_flags flags)
{
	int ret = -1;
	int i;

	_D("LCD on %x", flags);

	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
		ret = pmsys->bl_onoff(pmsys, STATUS_ON, flags);
		if (get_lcd_power() == PM_LCD_POWER_ON) {
			pm_history_save(PM_LOG_LCD_ON, pm_cur_state);
			break;
		} else {
			pm_history_save(PM_LOG_LCD_ON_FAIL, pm_cur_state);
#ifdef ENABLE_X_LCD_ONOFF
			_E("Failed to LCD on, through xset");
#else
			_E("Failed to LCD on, through OAL");
#endif
			ret = -1;
		}
	}

	if (flags & LCD_PHASED_TRANSIT_MODE) {
		if (pmsys->def_brt > LCD_PHASED_MIN_BRIGHTNESS)
			change_brightness(LCD_PHASED_MIN_BRIGHTNESS,
			    pmsys->def_brt, LCD_PHASED_CHANGE_STEP);
	}

	return ret;
}

static int backlight_off(enum device_flags flags)
{
	int ret = -1;
	int i;
	struct timespec time = {0, 30 * NANO_SECOND_MULTIPLIER};

	_D("lcdstep : LCD off %x", flags);

	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	if (flags & LCD_PHASED_TRANSIT_MODE) {
		if (pmsys->def_brt > LCD_PHASED_MIN_BRIGHTNESS)
			change_brightness(pmsys->def_brt,
			    LCD_PHASED_MIN_BRIGHTNESS, LCD_PHASED_CHANGE_STEP);
	}

	if (flags & AMBIENT_MODE)
		return 0;

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
#ifdef ENABLE_X_LCD_ONOFF
		if (x_dpms_enable == false)
#endif
			nanosleep(&time, NULL);
		_D("lcdstep : xset off %x", flags);
		ret = pmsys->bl_onoff(pmsys, STATUS_OFF, flags);
		if (get_lcd_power() == PM_LCD_POWER_OFF) {
			pm_history_save(PM_LOG_LCD_OFF, pm_cur_state);
			break;
		} else {
			pm_history_save(PM_LOG_LCD_OFF_FAIL, pm_cur_state);
#ifdef ENABLE_X_LCD_ONOFF
			_E("Failed to LCD off, through xset");
#else
			_E("Failed to LCD off, through OAL");
#endif
			ret = -1;
		}
	}
	return ret;
}

static int backlight_dim(void)
{
	int ret = -EIO;
	if (pmsys && pmsys->bl_brt)
		ret = pmsys->bl_brt(pmsys, pmsys->dim_brt, 0);

	pm_history_save(ret == 0 ? PM_LOG_LCD_DIM :
	    PM_LOG_LCD_DIM_FAIL, pm_cur_state);

	return ret;
}

static int set_current_brightness(int level)
{
	char *param[1];
	char buf[4];

	current_brightness = level;

	snprintf(buf, 4, "%d", current_brightness);
	param[0] = buf;

	/* broadcast current brightness */
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_CURRENT_BRIGHTNESS, "i", param);

	return 0;
}

static int get_current_brightness(void)
{
	return current_brightness;
}

static int set_custom_status(bool on)
{
	custom_status = on;
	return 0;
}

static bool get_custom_status(void)
{
	return custom_status;
}

static int save_custom_brightness(void)
{
	int cmd, ret, brightness;

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brightness);

	custom_brightness = brightness;

	return ret;
}

static int custom_backlight_update(void)
{
	int ret = 0;

	if (custom_brightness < PM_MIN_BRIGHTNESS ||
	    custom_brightness > PM_MAX_BRIGHTNESS)
		return -EINVAL;

	if ((pm_status_flag & PWRSV_FLAG) && !(pm_status_flag & BRTCH_FLAG)) {
		ret = backlight_dim();
	} else if (pmsys && pmsys->bl_brt) {
		_I("custom brightness restored! %d", custom_brightness);
		ret = pmsys->bl_brt(pmsys, custom_brightness, 0);
	}

	return ret;
}

static int set_force_brightness(int level)
{
	if (level < 0 ||  level > PM_MAX_BRIGHTNESS)
		return -EINVAL;

	force_brightness = level;

	return 0;
}

static int backlight_update(void)
{
	int ret = 0;

	if (hbm_get_state != NULL && hbm_get_state() == true) {
		_I("HBM is on, backlight is not updated!");
		return 0;
	}

	if (get_custom_status()) {
		_I("custom brightness mode! brt no updated");
		return 0;
	}
	if ((pm_status_flag & PWRSV_FLAG) && !(pm_status_flag & BRTCH_FLAG))
		ret = backlight_dim();
	else if (pmsys && pmsys->bl_brt)
		ret = pmsys->bl_brt(pmsys, pmsys->def_brt, 0);

	return ret;
}

static int backlight_standby(int force)
{
	int ret = -1;
	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	if ((get_lcd_power() == PM_LCD_POWER_ON) || force) {
		_I("LCD standby");
		ret = pmsys->bl_onoff(pmsys, STATUS_STANDBY, 0);
	}

	return ret;
}

static int set_default_brt(int level)
{
	if (!pmsys)
		return -EFAULT;

	if (level < PM_MIN_BRIGHTNESS || level > PM_MAX_BRIGHTNESS)
		level = PM_DEFAULT_BRIGHTNESS;
	pmsys->def_brt = level;

	return 0;
}

static Eina_Bool blink_cb(void *data)
{
	static bool flag;

	pmsys->bl_brt(pmsys, flag ? PM_MAX_BRIGHTNESS : PM_MIN_BRIGHTNESS, 0);

	flag = !flag;

	return EINA_TRUE;
}

static int blink(int timeout)
{
	static Ecore_Timer *timer;

	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}

	if (timeout < 0)
		return -EINVAL;

	if (timeout == 0) {
		backlight_update();
		return 0;
	}

	timer = ecore_timer_add(MSEC_TO_SEC((double)timeout), blink_cb, NULL);

	return 0;
}

static int check_wakeup_src(void)
{
	/*  TODO if nedded.
	 * return wackeup source. user input or device interrupts? (EVENT_DEVICE or EVENT_INPUT)
	 */
	return EVENT_DEVICE;
}

void _init_ops(void)
{
	backlight_ops.off = backlight_off;
	backlight_ops.dim = backlight_dim;
	backlight_ops.on = backlight_on;
	backlight_ops.update = backlight_update;
	backlight_ops.standby = backlight_standby;
	backlight_ops.hbm_off = backlight_hbm_off;
	backlight_ops.set_default_brt = set_default_brt;
	backlight_ops.get_lcd_power = get_lcd_power;
	backlight_ops.set_current_brightness = set_current_brightness;
	backlight_ops.get_current_brightness = get_current_brightness;
	backlight_ops.set_custom_status = set_custom_status;
	backlight_ops.get_custom_status = get_custom_status;
	backlight_ops.save_custom_brightness = save_custom_brightness;
	backlight_ops.custom_update = custom_backlight_update;
	backlight_ops.set_force_brightness = set_force_brightness;
	backlight_ops.blink = blink;

	power_ops.suspend = system_suspend;
	power_ops.pre_suspend = system_pre_suspend;
	power_ops.post_resume = system_post_resume;
	power_ops.power_lock = system_power_lock;
	power_ops.power_unlock = system_power_unlock;
	power_ops.get_power_lock_support = system_get_power_lock_support;
	power_ops.check_wakeup_src = check_wakeup_src;
}

int init_sysfs(unsigned int flags)
{
	int ret;

	pmsys = (PMSys *) malloc(sizeof(PMSys));
	if (pmsys == NULL) {
		_E("Not enough memory to alloc PM Sys");
		return -1;
	}

	memset(pmsys, 0x0, sizeof(PMSys));

	_init_pmsys(pmsys);
	_init_bldev(pmsys, flags);

	if (pmsys->bl_onoff == NULL || pmsys->sys_power_state == NULL) {
		_E("We have no managable resource to reduce the power consumption");
		return -1;
	}

	_init_ops();

	return 0;
}

int exit_sysfs(void)
{
	int fd;
	const struct device_ops *ops = NULL;

	fd = open("/tmp/sem.pixmap_1", O_RDONLY);
	if (fd == -1) {
		_E("X server disable");
		backlight_on(NORMAL_MODE);
	}

	backlight_update();

	ops = find_device("touchscreen");
	if (!check_default(ops))
		ops->start(NORMAL_MODE);

	ops = find_device("touchkey");
	if (!check_default(ops))
		ops->start(NORMAL_MODE);

	free(pmsys);
	pmsys = NULL;
	if (fd != -1)
		close(fd);

	return 0;
}
