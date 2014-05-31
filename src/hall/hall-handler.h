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


#ifndef __HALL_HANDLER_H__
#define __HALL_HANDLER_H__

#define HALL_IC_CLOSED	0
#define HALL_IC_OPENED	1

#define HALL_IC_SUBSYSTEM "flip"
#define HALL_IC_NAME "hall_ic"
#define HALL_IC_PATH "/devices/virtual/"HALL_IC_SUBSYSTEM"/"HALL_IC_NAME
#define HALL_IC_STATUS "/sys/class/"HALL_IC_SUBSYSTEM"/"HALL_IC_NAME"/cover_status"

#define PREDEF_HALL_IC HALL_IC_NAME

#endif //__HALL_HANDLER_H__
