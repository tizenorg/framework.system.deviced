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
 *
 */

#ifndef __TOUCH_CONTROLLER__
#define __TOUCH_CONTROLLER__

#include "touch.h"
#include "shared/common.h"

/*
 * struct touch_vconf_block
 */
struct touch_vconf_block {
	char *vconf;
	void (*vconf_function)(keynode_t *key, void* data);
};

void touch_controller_init(struct touch_control *);
void touch_controller_exit(void);
void touch_controller_start(void);
void touch_controller_stop(void);

#endif /* __TOUCH_CONTROLLER__ */
