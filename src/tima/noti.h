/*
 * TIMA(TZ based Integrity Measurement Architecture)
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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

#ifndef __TIMA_NOTI_H
#define __TIMA_NOTI_H

int tima_lkm_prevention_noti_on(void);
int tima_lkm_prevention_noti_off(int noti_id);
int tima_pkm_detection_noti_on(void);
int tima_pkm_detection_noti_off(int noti_id);

#endif
