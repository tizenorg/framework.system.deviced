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


#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "pass.h"
#include "pass-plugin.h"

#include "core/config-parser.h"

#define BUFF_MAX			255

/* Linux standard sysfs node */
#define CPUFREQ_SCALING_MAX_FREQ	"/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define CPUFREQ_SCALING_MIN_FREQ	"/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq"
#define CPUFREQ_SAMPLING_RATE		"/sys/devices/system/cpu/cpufreq/ondemand/sampling_rate"
#define CPUFREQ_UP_THRESHOLD		"/sys/devices/system/cpu/cpufreq/ondemand/up_threshold"

#define PROC_DT_COMPATIBLE             "/proc/device-tree/compatible"

/* PASS specific sysfs node */
#define PASS_CPU_STATS			"/sys/kernel/debug/cpufreq/cpu0/load_table"

int get_pass_cpu_stats(struct pass_policy *policy)
{
	struct pass_cpu_stats *stats = policy->pass_cpu_stats;
	char path[PATH_MAX] = PASS_CPU_STATS;
	char str[BUFF_MAX];
	FILE *fp_stats = NULL;
	int i, j;
	int ret = 1;

	if (!stats) {
		_E("invalid parameter of structure pass_cpu_stats");
		return -EINVAL;
	}

	fp_stats = fopen(path, "r");
	if (fp_stats == NULL)
		return -EIO;

	fgets(str, BUFF_MAX, fp_stats);
	for (i = 0; i < policy->num_pass_cpu_stats; i++) {
		ret = fscanf(fp_stats, "%lld %d %d %d",
			&stats[i].time,
			&stats[i].freq,
			&stats[i].freq_new,
			&stats[i].nr_runnings);

		for (j = 0; j < policy->cpufreq.num_nr_cpus; j++)
			ret = fscanf(fp_stats, "%d", &stats[i].load[j]);
	}
	fclose(fp_stats);

	return 0;
}

/*
 * Helper function
 * - Read from sysfs entry
 * - Write to sysfs entry
 */
static int sys_read_buf(char *file, char *buf)
{
	int fd;
	int r;
	int ret = 0;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -ENOENT;

	r = read(fd, buf, BUFF_MAX);
	if ((r >= 0) && (r < BUFF_MAX))
		buf[r] = '\0';
	else
		ret = -EIO;

	close(fd);

	return ret;
}

static int sys_write_buf(char *file, char *buf)
{
	int fd;
	int r;
	int ret = 0;

	fd = open(file, O_WRONLY);
	if (fd == -1)
		return -1;

	r = write(fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;

	close(fd);

	return ret;
}

static int sys_get_int(char *fname, int *val)
{
	char buf[BUFF_MAX];

	if (sys_read_buf(fname, buf) == 0) {
		*val = atoi(buf);
		return 0;
	} else {
		*val = -1;
		return -1;
	}
}

static int sys_set_int(char *fname, int val)
{
	char buf[BUFF_MAX];
	int r = -1;
	snprintf(buf, sizeof(buf), "%d", val);

	if (sys_write_buf(fname, buf) == 0)
		r = 0;

	return r;
}

/*
 * Get/Set maximum cpu frequency
 */
#define GET_VALUE(name, path)			\
int get_##name(void)				\
{						\
	int value, ret;				\
						\
	ret =  sys_get_int(path, &value);	\
	if (ret < 0)				\
		return -EAGAIN;			\
						\
	return value;				\
}						\

#define SET_VALUE(name, path)			\
int set_##name(int value)			\
{						\
	return sys_set_int(path, value);	\
}						\

GET_VALUE(cpufreq_scaling_max_freq, CPUFREQ_SCALING_MAX_FREQ);
SET_VALUE(cpufreq_scaling_max_freq, CPUFREQ_SCALING_MAX_FREQ);

GET_VALUE(cpufreq_scaling_min_freq, CPUFREQ_SCALING_MIN_FREQ);
SET_VALUE(cpufreq_scaling_min_freq, CPUFREQ_SCALING_MIN_FREQ);

GET_VALUE(cpufreq_sampling_rate, CPUFREQ_SAMPLING_RATE);
SET_VALUE(cpufreq_sampling_rate, CPUFREQ_SAMPLING_RATE);

GET_VALUE(cpufreq_up_threshold, CPUFREQ_UP_THRESHOLD);
SET_VALUE(cpufreq_up_threshold, CPUFREQ_UP_THRESHOLD);

int get_cpu_online(int cpu)
{
	char path[BUFF_MAX];
	int online;
	int ret;

	ret = sprintf(path, "/sys/devices/system/cpu/cpu%d/online", cpu);
	if (ret < 0)
		return -EINVAL;

	ret = sys_get_int(path, &online);
	if (ret < 0)
		return -EAGAIN;

	return online;
}
int set_cpu_online(int cpu, int online)
{
	char path[BUFF_MAX];
	int ret;

	ret = sprintf(path, "/sys/devices/system/cpu/cpu%d/online", cpu);
	if (ret < 0)
		return -EINVAL;

	ret = sys_set_int(path, online);
	if (ret < 0)
		return -EAGAIN;

	return 0;
}

/***************************************************************************
 *                            Parse pass.conf                              *
 ***************************************************************************/

static enum pass_state is_supported(char *value)
{
	enum pass_state state;

	if (!value)
		return -EINVAL;

	if (MATCH(value, "yes"))
		state = PASS_ON;
	else if (MATCH(value, "no"))
		state = PASS_OFF;
	else
		state = PASS_UNUSED;

	return state;
}

static int pass_parse_scenario(struct parse_result *result, void *user_data,
			       unsigned int index)
{
	struct pass_policy *policy = user_data;
	struct pass_scenario_policy *scenario = &policy->scenario;
	char section_name[BUFF_MAX];
	int i, len;

	if (!policy && !scenario && !result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	/* Parse 'PassScenario' section */
	if (MATCH(result->section, "PassScenario")) {
		if (MATCH(result->name, "pass_scenario_support")) {
			scenario->state = is_supported(result->value);
			if (scenario->state < 0)
				return -EINVAL;

		} else if (MATCH(result->name, "pass_num_scenarios")) {
			scenario->num_scenarios = atoi(result->value);

			if (scenario->num_scenarios > 0 && !scenario->list) {
				scenario->list = malloc(sizeof(struct pass_scenario)
							* scenario->num_scenarios);
				if (!scenario->list) {
					_E("cannot allocate memory for Scenario\n");
					return -EINVAL;
				}
			}
		}
	}

	if (scenario->state != PASS_ON)
		return 0;

	if (!scenario->num_scenarios)
		return 0;

	if (index >= scenario->num_scenarios || index == PASS_UNUSED)
		return 0;

	/* Parse 'Scenario' section */
	if (MATCH(result->name, "name")) {
		len = sizeof(scenario->list[index].name) - 1;
		strncpy(scenario->list[index].name, result->value, len);
		scenario->list[index].name[len] = '\0';
	} else if (MATCH(result->name, "support")) {
		scenario->list[index].state = is_supported(result->value);
		if (scenario->list[index].state < 0)
			return -EINVAL;
	} else if (MATCH(result->name, "cpufreq_min_level")) {
		scenario->list[index].cpufreq_min_level = atoi(result->value);
	} else if (MATCH(result->name, "cpufreq_max_level")) {
		scenario->list[index].cpufreq_max_level = atoi(result->value);
	} else if (MATCH(result->name, "busfreq_min_level")) {
		scenario->list[index].busfreq_min_level = atoi(result->value);
	} else if (MATCH(result->name, "busfreq_max_level")) {
		scenario->list[index].busfreq_max_level = atoi(result->value);
	} else if (MATCH(result->name, "gpufreq_min_level")) {
		scenario->list[index].gpufreq_min_level = atoi(result->value);
	} else if (MATCH(result->name, "gpufreq_max_level")) {
		scenario->list[index].gpufreq_max_level = atoi(result->value);
	}

	return 0;
}

static int  pass_parse_cpufreq_level(struct parse_result *result,
				    void *user_data, int level)
{
	struct pass_policy *policy = user_data;
	char section_name[BUFF_MAX];
	int i, ret;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	if (MATCH(result->name, "limit_max_freq"))
		policy->pass_table[level].limit_max_freq = atoi(result->value);
	else if (MATCH(result->name, "limit_max_cpu"))
		policy->pass_table[level].limit_max_cpu = atoi(result->value);

	else if (MATCH(result->name, "num_down_cond"))
		policy->pass_table[level].num_down_cond = atoi(result->value);
	else if (MATCH(result->name, "num_down_cond_freq"))
		policy->pass_table[level].down_cond[0].freq = atoi(result->value);
	else if (MATCH(result->name, "num_down_cond_nr_running"))
		policy->pass_table[level].down_cond[0].nr_running = atoi(result->value);
	else if (MATCH(result->name, "num_down_cond_busy_cpu"))
		policy->pass_table[level].down_cond[0].busy_cpu = atoi(result->value);

	else if (MATCH(result->name, "num_up_cond"))
		policy->pass_table[level].num_up_cond = atoi(result->value);
	else if (MATCH(result->name, "num_up_cond_freq"))
		policy->pass_table[level].up_cond[0].freq = atoi(result->value);
	else if (MATCH(result->name, "num_up_cond_nr_running"))
		policy->pass_table[level].up_cond[0].nr_running = atoi(result->value);
	else if (MATCH(result->name, "num_up_cond_busy_cpu"))
		policy->pass_table[level].up_cond[0].busy_cpu = atoi(result->value);

	else if (MATCH(result->name, "num_left_cond"))
		policy->pass_table[level].num_left_cond = atoi(result->value);
	else if (MATCH(result->name, "num_left_cond_freq"))
		policy->pass_table[level].left_cond[0].freq = atoi(result->value);
	else if (MATCH(result->name, "num_left_cond_nr_running"))
		policy->pass_table[level].left_cond[0].nr_running = atoi(result->value);
	else if (MATCH(result->name, "num_left_cond_busy_cpu"))
		policy->pass_table[level].left_cond[0].busy_cpu = atoi(result->value);

	else if (MATCH(result->name, "num_right_cond"))
		policy->pass_table[level].num_right_cond = atoi(result->value);
	else if (MATCH(result->name, "num_right_cond_freq"))
		policy->pass_table[level].right_cond[0].freq = atoi(result->value);
	else if (MATCH(result->name, "num_right_cond_nr_running"))
		policy->pass_table[level].right_cond[0].nr_running = atoi(result->value);
	else if (MATCH(result->name, "num_right_cond_busy_cpu"))
		policy->pass_table[level].right_cond[0].busy_cpu = atoi(result->value);

	return 0;
}

static int pass_parse_core(struct parse_result *result, void *user_data)
{
	struct pass_policy *policy = user_data;
	char section_name[BUFF_MAX];
	int i, ret;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	if (MATCH(result->name, "pass_compatible")) {
		char compatible[BUFF_MAX];

		ret = sys_read_buf(PROC_DT_COMPATIBLE, compatible);
		if (ret < 0)
			return -EEXIST;

		if (strcmp(compatible, result->value))
			return -EINVAL;

		_I("Match compatible string : %s\n", compatible);
       } else if (MATCH(result->name, "pass_support"))
		policy->state = atoi(result->value);
	else if (MATCH(result->name, "pass_gov_type"))
		policy->gov_type = atoi(result->value);
	else if (MATCH(result->name, "pass_num_levels"))
		policy->num_levels = atoi(result->value);
	else if (MATCH(result->name, "pass_min_level"))
		policy->min_level = atoi(result->value);
	else if (MATCH(result->name, "pass_max_level"))
		policy->max_level = atoi(result->value);
	else if (MATCH(result->name, "pass_num_cpu_stats"))
		policy->num_pass_cpu_stats = atoi(result->value);
	else if (MATCH(result->name, "pass_cpu_threshold"))
		policy->pass_cpu_threshold = atoi(result->value);
	else if (MATCH(result->name, "pass_up_threshold"))
		policy->up_threshold = atoi(result->value);
	else if (MATCH(result->name, "pass_down_threshold"))
		policy->down_threshold = atoi(result->value);
	else if (MATCH(result->name, "pass_init_level"))
		policy->init_level = atoi(result->value);
	else if (MATCH(result->name, "pass_level_up_threshold"))
		policy->level_up_threshold = atoi(result->value);
	else if (MATCH(result->name, "pass_governor_timeout")) {
		policy->gov_timeout = atof(result->value);

		if (PASS_MIN_GOV_TIMEOUT > policy->gov_timeout)
			policy->gov_timeout = PASS_MIN_GOV_TIMEOUT;
	}

	if (policy->num_levels > 0 && !policy->pass_table) {
		policy->pass_table = malloc(sizeof(struct pass_table)
			       * policy->num_levels);
		if (!policy->pass_table) {
			_E("cannot allocate memory for pass_table\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int pass_load_config(struct parse_result *result, void *user_data)
{
	struct pass_policy *policy = user_data;
	struct pass_scenario_policy *scenario = &policy->scenario;
	char section_name[BUFF_MAX];
	int level = PASS_UNUSED;
	int index = PASS_UNUSED;
	int i, ret;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	/* Parsing 'PASS' section */
	if (MATCH(result->section, "Pass")) {
		ret = pass_parse_core(result, user_data);
		if (ret < 0) {
			_E("cannot parse the core part\n");
			return ret;
		}

		goto out;
	}

	/* Parsing 'CpufreqLevel' section to get pass-table */
	for (level = 0; level < policy->num_levels; level++) {
		ret = sprintf(section_name, "CpufreqLevel%d", level);

		if (MATCH(result->section, section_name)) {
			ret = pass_parse_cpufreq_level(result, user_data, level);
			if (ret < 0) {
				_E("cannot parse 'Cpufreq' section\n");
				return ret;
			}

			goto out;
		}
	}

	/* Parsing 'PassScenario' section */
	if (MATCH(result->section, "PassScenario")) {
		ret = pass_parse_scenario(result, user_data, PASS_UNUSED);
		if (ret < 0) {
			_E("cannot parse 'PassScenario' section\n");
			return ret;
		}

		goto out;
	}

	/* Parsing 'Scenario' section */
	for (index = 0; index < scenario->num_scenarios; index++) {
		ret = sprintf(section_name, "Scenario%d", index);

		if (MATCH(result->section, section_name)) {
			ret = pass_parse_scenario(result, user_data, index);
			if (ret < 0) {
				_E("cannot parse 'Scenario' section\n");
				return ret;
			}

			goto out;
		}
	}

out:
	return 0;
}

int get_pass_table(struct pass_policy *policy, char *pass_conf_path)
{
	int ret;

	policy->state = PASS_UNUSED;
	policy->scenario.state = PASS_UNUSED;

	ret = config_parse(pass_conf_path, pass_load_config, policy);
	if (ret < 0) {
		_E("cannot parse %s\n", pass_conf_path);
		return -EINVAL;
	}

	if (policy->state == PASS_UNUSED)
		return -EINVAL;

	if (policy->scenario.state == PASS_UNUSED)
		_W("%s don't include the list of pass-scenario\n");
	else
		_I("can%s use pass-scenario",
				policy->scenario.state ? "" : "not");

	return 0;
}

void put_pass_table(struct pass_policy *policy)
{
	if(policy->pass_table)
		free(policy->pass_table);

	if(policy->scenario.list)
		free(policy->scenario.list);
}

/*
 * get_time_ms - Return current time (unit: millisecond)
 */
int64_t get_time_ms(void)
{
	struct timeval now;

	gettimeofday(&now, NULL);

	return (int64_t)(now.tv_sec * 1000 + now.tv_usec / 1000);
}
