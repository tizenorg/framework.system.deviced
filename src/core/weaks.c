/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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

int __WEAK__ get_flight_mode(bool *mode) {*mode = 0; return 0; }
int __WEAK__ set_flight_mode(bool mode) {return 0; }
int __WEAK__ get_call_state(int *state) {*state = 0; return 0; }
int __WEAK__ get_mobile_hotspot_mode(int *mode) {*mode = 0; return 0; }

