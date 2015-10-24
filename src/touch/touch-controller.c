/*
 * Touch controller
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

#include "touch-controller.h"
#include "core/common.h"

static struct touch_control *touch_control;

/******************************************************
 *                Touch VCONF interface               *
 ******************************************************/
static void touch_vconf_pm_sip_status(keynode_t *key, void *data)
{
	int sip_state;

	if (vconf_get_int(VCONFKEY_ISF_INPUT_PANEL_STATE, &sip_state) != 0)
		return;

	switch (sip_state) {
	case VCONFKEY_ISF_INPUT_PANEL_STATE_HIDE:
		/* If SIP is off state, should turn on touch boost feature */
		touch_boost_enable(touch_control, TOUCH_TYPE_VCONF_SIP, TOUCH_BOOST_ON);
		break;
	case VCONFKEY_ISF_INPUT_PANEL_STATE_SHOW:
		/* If SIP is on state, should turn off touch boost feature */
		touch_boost_enable(touch_control, TOUCH_TYPE_VCONF_SIP, TOUCH_BOOST_OFF);
		break;
	default:
		_E("Unknow SIP type");
		return;
	}
}

static struct touch_vconf_block vconf_block[] = {
	{
		.vconf		= VCONFKEY_ISF_INPUT_PANEL_STATE,
		.vconf_function	= touch_vconf_pm_sip_status,
	},
};

static int touch_vconf_stop(void)
{
	int i;
	int ret;
	int size;

	if (!touch_control) {
		_E("cannot stop touch_vconf");
		return -EINVAL;
	}

	/* Unregister callback function of VCONFKEY */
	size = ARRAY_SIZE(vconf_block);
	for (i = 0; i < size; i++) {
		ret = vconf_ignore_key_changed(vconf_block[i].vconf,
					vconf_block[i].vconf_function);
		if (ret < 0)
			_E("cannot unregister 'vconf function' : %d",
					vconf_block[i].vconf);
	}

	_I("Stop Touch of VCONF");

	return 0;
}

static int touch_vconf_start(void)
{
	int i;
	int ret;
	int size;

	if (!touch_control) {
		_E("cannot start touch_vconf");
		return -EINVAL;
	}

	size = ARRAY_SIZE(vconf_block);

	/* Register callback function of VCONFKEY */
	for (i = 0; i < size; i++) {
		ret = vconf_notify_key_changed(vconf_block[i].vconf,
					vconf_block[i].vconf_function,
					(void *)touch_control);
		if (ret < 0)
			_E("cannot register 'vconf function' : %d",
					vconf_block[i].vconf);
	}

	_I("Start Touch of VCONF");

	return 0;
}

/******************************************************
 *             Touch controller interface             *
 ******************************************************/
API void touch_controller_start(void)
{
	touch_vconf_start();

	/* Add to start() of new touch type */
}

API void touch_controller_stop(void)
{
	touch_vconf_stop();

	/* Add to stop() of new touch type */
}

API void touch_controller_init(struct touch_control *tc)
{
	touch_control = tc;
}

API void touch_controller_exit(void)
{
	touch_control = NULL;
}
