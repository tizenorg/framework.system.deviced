/*
 *  deviced
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

int config_init(void);
const char *config_get_string(const char *key, const char *def_value, int *error_code);
int config_get_int(const char *key, int def_value, int *error_code);
float config_get_float(const char *key, float def_value, int *error_code);
double config_get_double(const char *key, double def_value, int *error_code);
int config_exit(void);

#endif /* __CONFIG_H__ */
