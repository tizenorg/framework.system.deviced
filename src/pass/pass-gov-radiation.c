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
 * pass_radiation_governor - Check cpu state and determine the amount of resource
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
int pass_radiation_governor(struct pass_policy *policy)
{
	struct pass_table *table = policy->pass_table;
	struct pass_cpu_stats *cpu_stats = policy->pass_cpu_stats;
	int up_threshold = policy->up_threshold;
	int down_threshold = policy->down_threshold;
	int num_pass_gov = 0;
	int level = policy->curr_level;
	int up_count = 0;
	int down_count = 0;
	int left_count = 0;
	int right_count = 0;
	bool up_condition;
	bool down_condition;
	bool left_condition;
	bool right_condition;
	int freq;
	int nr_running;
	int busy_cpu;
	int i;
	int j;
	int64_t time;
	static int64_t last_time = 0;

	for (i = 0; i < policy->num_pass_cpu_stats; i++) {
		time = cpu_stats[i].time;
		freq = cpu_stats[i].freq;
		nr_running = cpu_stats[i].nr_runnings;
		busy_cpu = cpu_stats[i].num_busy_cpu;
		up_condition = true;
		down_condition = true;
		left_condition = true;
		right_condition = true;

		if (last_time >= time)
			continue;
		last_time = time;
		num_pass_gov++;

		/*
		_I("[Level%d][%d][%lld] %d | %d | %d", level, i, time, freq,
						nr_running, busy_cpu);
		*/

		/* Check level up condition */
		for (j = 0; j < table[level].num_up_cond && up_condition; j++) {
			if (table[level].up_cond[j].freq &&
				!(freq >= table[level].up_cond[j].freq))
				up_condition = false;
		}

		if (table[level].num_up_cond && up_condition) {
			up_count++;

			/*
			_I("[Level%d] [up_count : %2d] \
				freq(%d), nr_running(%d), busy_cpu(%d)",
				level, up_count,
				table[level].up_cond[j].freq ? freq : -1,
			*/
		}

		/* Check level down condition */
		for (j = 0; j < table[level].num_down_cond && down_condition; j++) {
			if (table[level].down_cond[j].freq
				&& !(freq <= table[level].down_cond[j].freq))
				down_condition = false;
		}

		if (table[level].num_down_cond && down_condition) {
			down_count++;

			/*
			_I("[Level%d] [dw_count : %2d] \
				freq(%d), nr_running(%d), busy_cpu(%d)",
				level, down_count,
				table[level].down_cond[j].freq ? freq : -1,
			*/
		}

		/* Check level right condition */
		for (j = 0; j < table[level].num_right_cond && right_condition; j++) {
			/*
			 * If one more conditions are false among following
			 * conditions, don't increment right_count.
			 */
			if (table[level].right_cond[j].nr_running
				&& !(nr_running > table[level].right_cond[j].nr_running))
				right_condition = false;

			if (table[level].right_cond[j].busy_cpu
				&& !(busy_cpu >= table[level].right_cond[j].busy_cpu))
				right_condition = false;
		}

		if (table[level].num_right_cond && right_condition) {
			right_count++;

			/*
			_I("[Level%d] [right_count : %2d] \
				nr_running(%d), busy_cpu(%d)",
				level, right_count,
				table[level].right_cond[j].nr_running ? nr_running : -1,
				table[level].right_cond[j].busy_cpu ? busy_cpu : -1);
			*/
		}

		/* Check level left condition */
		for (j = 0; j < table[level].num_left_cond && left_condition; j++) {
			/*
			 * If one more conditions are false among following
			 * conditions, don't increment left_count.
			 */
			if (table[level].left_cond[j].nr_running
				&& !(nr_running <= table[level].left_cond[j].nr_running))
				left_condition = false;

			if (table[level].left_cond[j].busy_cpu
				&& !(busy_cpu < table[level].left_cond[j].busy_cpu))
				left_condition = false;
		}

		if (table[level].num_left_cond && left_condition) {
			left_count++;

			/*
			_I("[Level%d] [ left_count : %2d] \
				nr_running(%d), busy_cpu(%d)",
				level, left_count,
				table[level].left_cond[j].nr_running ? nr_running : -1,
				table[level].left_cond[j].busy_cpu ? busy_cpu : -1);
			*/
		}
	}

	if (up_count * 100 >= num_pass_gov * up_threshold)
		level += policy->cpufreq.num_nr_cpus;
	else if (down_count * 100 >= num_pass_gov * down_threshold)
		level -= policy->cpufreq.num_nr_cpus;

	if (right_count * 100 >= num_pass_gov * up_threshold)
		level += 1;
	else if (left_count * 100 >= num_pass_gov * down_threshold)
		level -= 1;

	/*
	if (level == policy->prev_level) {
		for (i = num_pass_gov; i < policy->num_pass_cpu_stats; i++) {
			time = cpu_stats[i].time;
			freq = cpu_stats[i].freq;
			nr_running = cpu_stats[i].nr_runnings;
			busy_cpu = cpu_stats[i].num_busy_cpu;

			_I("[Level%d][%d][%lld] %d | %d | %d",
					level, i, time, freq,
					nr_running, busy_cpu);
		}
	}

	if (level != policy->curr_level) {
		_I("\n[Level%d] num_pass_gov: [%2d]", level, num_pass_gov);
		_I("[Level%d] down_count  : %2d (%3d >= %3d)", level, down_count,
				down_count * 100, num_pass_gov * down_threshold);
		_I("[Level%d] up_count    : %2d (%3d >= %3d)", level, up_count,
				up_count * 100, num_pass_gov * up_threshold);
		_I("[Level%d] left_count  : %2d (%3d >= %3d)", level, left_count,
				left_count * 100, num_pass_gov * down_threshold);
		_I("[Level%d] right_count : %2d (%3d >= %3d)", level, right_count,
				right_count * 100, num_pass_gov * up_threshold);
	}
	*/

	return level;
}
