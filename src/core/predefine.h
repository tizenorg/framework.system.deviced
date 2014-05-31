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


#ifndef __PREDEFINE_H__
#define __PREDEFINE_H__

int call_predefine_action(int argc, char **argv);
int is_factory_mode(void);
void predefine_pm_change_state(unsigned int s_bits);
int predefine_get_pid(const char *execpath);
#endif /* __PREDEFINE_H__ */
