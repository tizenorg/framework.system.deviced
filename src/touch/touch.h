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
 *
 */

#ifndef __TOUCH__
#define __TOUCH__

#include <Ecore.h>
#include <vconf.h>
#include "touch-util.h"

/* Define touch type */
enum touch_type {
	TOUCH_TYPE_VCONF_SIP = 0,
	TOUCH_TYPE_MAX,
};

/* Define touch boost state */
enum touch_boost_state {
	TOUCH_BOOST_ON = 0,
	TOUCH_BOOST_OFF = 1,
};

/*
 * struct touch_control_block
 */
struct touch_control {
	unsigned int mask;
	enum touch_boost_state cur_state;
};

void touch_boost_enable(struct touch_control *,
		enum touch_type, enum touch_boost_state);
#endif /* __TOUCH__ */
