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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pass.h"

/*
 * pass_step_governor - Check cpu state and determine the amount of resource
 *
 * @stats: structure for getting cpu frequency state from kerncel
 *
 * This function check periodically current following state of system
 * and then determine whether pass level up or down according to pass
 * policy with gathered data.
 * - current cpu frequency
 * - the number of nr_running
 * - the number of busy_cpu
 */
int pass_step_governor(struct pass_policy *policy)
{
	struct pass_table *table = policy->pass_table;
	struct pass_cpu_stats *cpu_stats = policy->pass_cpu_stats;
	int up_threshold = policy->up_threshold;
	int down_threshold = policy->down_threshold;
	int num_pass_gov = 0;
	int level = policy->curr_level;
	int count = 0;
	int up_count = 0;
	int up_max_count = 0;
	int down_count = 0;
	bool up_condition;
	bool down_condition;
	int freq;
	int nr_running;
	int busy_cpu;
	int i;
	int j;
	int64_t time;
	static int64_t last_time;

	for (i = 0; i < policy->num_pass_cpu_stats; i++) {
		time = cpu_stats[i].time;
		freq = cpu_stats[i].freq;
		nr_running = cpu_stats[i].nr_runnings;
		busy_cpu = cpu_stats[i].num_busy_cpu;
		up_condition = false;
		down_condition = false;

		if (last_time >= time)
			continue;
		last_time = time;
		num_pass_gov++;

		/* Check level up condition */
		for (j = 0; j < table[level].num_up_cond; j++) {
			/*
			 * If one more conditions are false among following
			 * conditions, don't increment up_count.
			 */
			if (freq >= table[level].up_cond[j].freq
				&& nr_running >= table[level].up_cond[j].nr_running
				&& busy_cpu >= table[level].up_cond[j].busy_cpu)
				up_condition = true;
		}

		if (table[level].num_up_cond && up_condition) {
			up_count++;

			/*
			_I("[Level%d] [up_count : %2d] \
				freq(%d), nr_running(%d), busy_cpu(%d)",
				level, up_count,
				table[level].up_cond[j].freq ? freq : -1,
				table[level].up_cond[j].nr_running ? nr_running : -1,
				table[level].up_cond[j].busy_cpu ? busy_cpu : -1);
			*/
		}

		/* Check level down condition */
		for (j = 0; j < table[level].num_down_cond; j++) {
			/*
			 * If one more conditions are false among following
			 * conditions, don't increment down_count.
			 */
			if (freq <= table[level].down_cond[j].freq
				&& nr_running < table[level].down_cond[j].nr_running
				&& busy_cpu < table[level].down_cond[j].busy_cpu)
				down_condition = true;
		}

		if (table[level].num_down_cond && down_condition) {
			down_count++;

			/*
			_I("[Level%d] [dw_count : %2d] \
				freq(%d), nr_running(%d), busy_cpu(%d)",
				level, down_count,
				table[level].down_cond[j].freq ? freq : -1,
				table[level].down_cond[j].nr_running ? nr_running : -1,
				table[level].down_cond[j].busy_cpu ? busy_cpu : -1);
			*/
		}

		if (level < policy->max_level &&
			freq == table[level].limit_max_freq)
			up_max_count++;
	}

	if (!num_pass_gov)
		return level;

	if (up_count && level < policy->max_level &&
			up_count * 100 >= num_pass_gov * up_threshold) {
		level += 1;
	} else if (down_count && level > policy->min_level &&
			down_count * 100 >= num_pass_gov * down_threshold) {
		level -= 1;
	}

	return level;
}
