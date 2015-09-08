/*
 * Touch
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
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

#include <glib.h>
#include <stdio.h>
#include <vconf.h>

#include "touch.h"
#include "touch-controller.h"

#include "core/devices.h"
#include "core/common.h"

void touch_boost_enable(struct touch_control *touch_control,
		enum touch_type type, enum touch_boost_state state)
{
	if (!touch_control)
		return;

	switch (state) {
	case TOUCH_BOOST_ON:
		touch_control->mask |= (1 << type);
		break;
	case TOUCH_BOOST_OFF:
		touch_control->mask &= ~(1 << type);
		break;
	default:
		_E("Unknow touch boost state");
		return;
	}

	/*
	 * Touch should update touch_control->cur_state after updating
	 * touch_control->mask variable because Touch need what some module
	 * turn on/off touch boost feature.
	 */
	if (touch_control->cur_state == state)
		return;

	switch (type) {
	case TOUCH_TYPE_VCONF_SIP:
		set_cpufreq_touch_boost_off(state);
		break;
	default:
		_E("Unknow touch type");
		return;
	}

	touch_control->cur_state = state;

	_I("Touch Boost is %s", state == TOUCH_BOOST_ON ? "ON" : "OFF");
}

/* Define global variable of struct touch_control */
static struct touch_control touch_control;

static int touch_stop(enum device_flags flags)
{
	/* Restore touch boost state as ON state */
	set_cpufreq_touch_boost_off(TOUCH_BOOST_ON);

	touch_controller_stop();

	_I("Stop Touch");

	return 0;
}

static int touch_start(enum device_flags flags)
{
	int i;

	/* Touch Boost is default on state */
	touch_control.cur_state = TOUCH_BOOST_ON;

	for (i = 0; i < TOUCH_TYPE_MAX; i++)
		touch_control.mask |= (1 << i);

	touch_controller_start();

	_I("Start Touch");

	return 0;
}

/*
 * touch_init - Initialize Touch module
 */
static void touch_init(void *data)
{
	touch_controller_init(&touch_control);
}

/*
 * touch_exit - Exit Touch module
 */
static void touch_exit(void *data)
{
	touch_controller_exit();
}

static const struct device_ops touch_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "touch",
	.init     = touch_init,
	.exit     = touch_exit,
	.start    = touch_start,
	.stop     = touch_stop,
};

DEVICE_OPS_REGISTER(&touch_device_ops)
