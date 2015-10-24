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


#ifndef __DISPLAY_WEAKS_H__
#define __DISPLAY_WEAKS_H__

#include "core/common.h"
#include "core.h"

/* src/display/hbm.c */
int __WEAK__ hbm_get_state(void);
int __WEAK__ hbm_set_state(int);
int __WEAK__ hbm_set_state_with_timeout(int, int);
void __WEAK__ hbm_check_timeout(void);
void __WEAK__ hbm_turn_on(void);
void __WEAK__ hbm_turn_off(void);

/* src/display/brightness.c */
int __WEAK__ control_brightness_key(int action);

/* src/display/alpm.c */
int __WEAK__ alpm_set_state(int);
int __WEAK__ alpm_get_state(void);
int __WEAK__ set_alpm_screen(char *, pid_t pid);
bool __WEAK__ check_suspend_direct(pid_t pid);
int __WEAK__ get_ambient_mode(void);
void __WEAK__ check_alpm_invalid_state(void);
int __WEAK__ check_alpm_lcdon_ready(void);

/* src/display/mbm.c */
void __WEAK__ mbm_turn_on(void);
void __WEAK__ mbm_turn_off(void);

/* src/display/standby-mode.c */
int __WEAK__ get_standby_state(void);
void __WEAK__ set_standby_state(bool state);
void __WEAK__ set_standby_mode(pid_t pid, int enable);
void __WEAK__ print_standby_mode(FILE *fp);

/* src/display/logging.c */
void __WEAK__ pm_history_save(enum pm_log_type log_type, int code);
void __WEAK__ pm_history_init(void);
void __WEAK__ pm_history_print(FILE *fp, int count);

/* src/display/key-filter.c */
int __WEAK__ get_glove_state(void);
void __WEAK__ switch_glove_key(int val);



#endif

