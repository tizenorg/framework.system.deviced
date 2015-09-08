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

#ifndef __PASS_CORE__
#define __PASS_CORE__
/*
 * pass_get_governor - Return specific governor instance according to type
 *
 * @policy: the instance of struct pass_policy
 * @type: the type of PASS governor
 */
struct pass_governor* pass_get_governor(struct pass_policy *policy,
						enum pass_gov_type type);

/*
 * pass_get_hotplug - Return specific hotplug instance according to type
 *
 * @policy: the instance of struct pass_policy
 * @type: the type of PASS governor
 */
struct pass_hotplug* pass_get_hotplug(struct pass_policy *policy,
						enum pass_gov_type type);

#endif	/* __PASS_CORE__ */
