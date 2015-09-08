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


#ifndef __PASS_PLUGIN__
#define __PASS_PLUGIN__

#include "pass.h"

int get_pass_cpu_stats(struct pass_policy *policy);

int get_pass_table(struct pass_policy *policy, char *pass_conf_path);
void put_pass_table(struct pass_policy *policy);

int get_cpufreq_scaling_max_freq(void);
int set_cpufreq_scaling_max_freq(int);

int get_cpufreq_scaling_min_freq(void);
int set_cpufreq_scaling_min_freq(int);

int get_cpufreq_sampling_rate(void);
int set_cpufreq_sampling_rate(int);

int get_cpufreq_up_threshold(void);
int set_cpufreq_up_threshold(int);

int get_cpu_online(int);
int set_cpu_online(int, int);

int64_t get_time_ms(void);

#endif /* __PASS_PLUGIN__ */
