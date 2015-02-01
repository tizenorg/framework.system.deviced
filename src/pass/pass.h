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


#ifndef __PASS__
#define __PASS__

#include <Ecore.h>
#include "pass-util.h"

/******************************************************
 *                   PASS Governors                   *
 ******************************************************/
#define PASS_NAME_LEN		128
#define PASS_LEVEL_COND_MAX	3
#define PASS_CPU_STATS_DEFAULT	20
#define PASS_MIN_GOV_TIMEOUT	0.2

#define PASS_CONF_PATH		"/etc/deviced/pass.conf"

struct pass_policy;

/*
 * PASS state
 */
enum pass_state {
	PASS_UNUSED = -1,
	PASS_OFF = 0,
	PASS_ON = 1,
};

/*
 * PASS Governor type
 */
enum pass_gov_type {
	PASS_GOV_STEP,
	PASS_GOV_RADIATION,

	PASS_GOV_END,
};

enum pass_gov_state {
	PASS_GOV_NONE = 0,
	PASS_GOV_START,
	PASS_GOV_STOP,
};

/*
 * PASS cpu state
 */
enum pass_cpu_state {
	PASS_CPU_DOWN = 0,
	PASS_CPU_UP,
};

/*
 * struct pass_governor
 */
struct pass_governor {
	char name[PASS_NAME_LEN];
	int (*init)(struct pass_policy *);
	int (*exit)(struct pass_policy *);
	int (*update)(struct pass_policy *, enum pass_gov_state state);
	int (*governor)(struct pass_policy *);
	Ecore_Timer *gov_timer_id;
	double gov_timeout;
};

/******************************************************
 *                   PASS basic data                  *
 ******************************************************/

/*
 * struct pass_cpu_stats
 *
 * @time: current time
 * @freq: current cpu frequency
 * @freq: new cpu frequency
 * @nr_runnings: the average number of running threads
 * @nr_running[]: current number of running threads of each core
 * @runnable_load[]: the runnable load of each core
 * @busy_cpu: the number of busy cpu
 * @avg_load: the average load of all CPUs
 * @avg_runnable_load: the average runnable load of all CPUs
 * @avg_thread_load: the Per-thread load
 * @avg_thread_runnable_load: the Per-thread runnable load
 */
struct pass_cpu_stats {
	int64_t time;
	unsigned int freq;
	unsigned int freq_new;
	unsigned int nr_runnings;

	unsigned int *load;
	unsigned int *nr_running;
	unsigned int *runnable_load;

	unsigned int num_busy_cpu;
	unsigned int avg_load;
	unsigned int avg_runnable_load;
	unsigned int avg_thread_load;
	unsigned int avg_thread_runnable_load;
};

/******************************************************
 *                   PASS interface             *
 ******************************************************/
struct pass_level_condition {
	int freq;
	int nr_running;
	int busy_cpu;
	int avg_load;
};

struct pass_table {
	/* Constraints condition for powersaving */
	int limit_max_freq;
	int limit_max_cpu;

	/* Governor timer's timeout for each pass level */
	double gov_timeout;

	/* Condition to determine up/down of pass level */
	struct pass_level_condition comm_cond;
	struct pass_level_condition up_cond[PASS_LEVEL_COND_MAX];
	struct pass_level_condition down_cond[PASS_LEVEL_COND_MAX];
	struct pass_level_condition left_cond[PASS_LEVEL_COND_MAX];
	struct pass_level_condition right_cond[PASS_LEVEL_COND_MAX];
	int num_up_cond;
	int num_down_cond;
	int num_left_cond;
	int num_right_cond;
};

/*
 * struct pass_hotplug - store information of cpu hotplug
 * @max_online: the possible maximum number of online cpu
 * @online: the current number of online cpu
 * @sequence: the sequence to turn on/off cpu
 */
struct pass_hotplug {
	char name[PASS_NAME_LEN];
	unsigned int max_online;
	unsigned int online;
	unsigned int *sequence;
	int (*governor)(struct pass_policy *);
};


struct pass_cpufreq_policy {
	unsigned int max_freq;
	unsigned int num_nr_cpus;
	unsigned int sampling_rate;
	unsigned int up_threshold;
};

struct pass_busfreq_policy {
	/* TODO */
};

struct pass_gpufreq_policy {
	/* TODO */
};

struct pass_scenario {
	char name[PASS_NAME_LEN];
	enum pass_state state;
	enum pass_state locked;
	int64_t locked_time;

	unsigned int cpufreq_min_level;
	unsigned int cpufreq_max_level;

	unsigned int busfreq_min_level;
	unsigned int busfreq_max_level;

	unsigned int gpufreq_min_level;
	unsigned int gpufreq_max_level;
};

struct pass_scenario_policy {
	enum pass_state state;
	unsigned int num_scenarios;

	struct pass_scenario *list;
};

/*
 * struct pass_policy
 */
struct pass_policy {
	enum pass_state state;
	enum pass_gov_type gov_type;
	enum pass_gov_state gov_state;
	unsigned int pass_cpu_threshold;
	unsigned int up_threshold;
	unsigned int down_threshold;

	unsigned int init_level;
	unsigned int prev_level;
	unsigned int curr_level;
	unsigned int min_level;
	unsigned int max_level;

	unsigned int default_min_level;
	unsigned int default_max_level;

	unsigned int num_levels;
	unsigned int level_up_threshold;

	struct pass_cpufreq_policy cpufreq;
	struct pass_busfreq_policy busfreq;
	struct pass_gpufreq_policy gpufreq;
	struct pass_scenario_policy scenario;

	struct pass_table *pass_table;
	struct pass_cpu_stats *pass_cpu_stats;
	int num_pass_cpu_stats;

	struct pass_governor *governor;
	double gov_timeout;
	struct pass_hotplug *hotplug;
};
#endif /* __pass__ */
