/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include "core/common.h"
#include "weaks.h"

/*
 * May be supplied by logging if desired
 * src/display/logging.c
 */
void __WEAK__ pm_history_save(enum pm_log_type log_type, int code) {}
void __WEAK__ pm_history_init(void) {}
void __WEAK__ pm_history_print(FILE *fp, int count) {}

