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
#include <sys/time.h>

#include "pass.h"
#include "pass-gov.h"
#include "pass-hotplug.h"

#include "core/device-notifier.h"
#include "core/config-parser.h"

#define PASS_CPU_STATS_MAX_COUNT	20

/*
 * is_enabled - Check the state of PASS governor
 * @policy: instance of pass_policy structure
 *
 * Return true if the state of PASS is PASS_GOV_START
 * Return false if the state of PASS is PASS_GOV_STOP
 */
static bool is_enabled(struct pass_policy *policy)
{
	if (!policy)
		return false;

	if (policy->gov_state != PASS_GOV_START)
		return false;

	return true;
}

/****************************************************************************
 *                             PASS Notifier                                *
 * - DEVICE_NOTIFIER_PMQOS                                                  *
 * - DEVICE_NOTIFIER_BOOTING_DONE                                           *
 ****************************************************************************/
#define PASS_LOCK			"Lock"
#define PASS_UNLOCK			"Unlock"

static int pass_governor_change_level_scope(struct pass_policy *policy,
					    int min_level, int max_level);

/*
 * FIXME: Current notifier of deviced didn't pass separate user_data parameter
 * on callback function. So, PASS must need global pass_policy instance. This
 * code must be modified after changing deviced's notifier feature.
 */
static struct pass_policy *policy_pmqos;

/*
 * is_pmqos_enabled - Check state of whether to support PM_QOS for scenario
 * @policy: instance of pass_policy structure
 *
 * Return true if the state of PM_QOS is supported
 * Return false if the state of PM_QOS isn't supported
 */
static bool is_pmqos_enabled(struct pass_policy *policy)
{
	if (!policy)
		return false;

	if (!policy->scenario.list)
		return false;

	if (policy->scenario.state != PASS_ON)
		return false;

	return true;
}

/*
 * is_scenario_locked - Check locked state of each scenario
 * @policy: instance of pass_scenario structure
 *
 * Return true if scenario is locked and enabled
 * Return false if scenario is unlocked or disabled
 */

static bool is_scenario_locked(struct pass_scenario *scn)
{
	if (!scn)
		return false;

	if (!scn->locked || scn->state != PASS_ON)
		return false;

	return true;
}

static enum pass_state is_pmqos_locked(char *data, char *name)
{
	char *unlock = NULL;

	if (!data)
		return PASS_OFF;

	unlock = strstr(data, PASS_UNLOCK);
	if (!unlock) {
		/* Lock scenario */
		strncpy(name, data, strlen(data) - strlen(PASS_LOCK));
		return PASS_ON;
	} else {
		/* Unlock scenario */
		strncpy(name, data, strlen(data) - strlen(PASS_UNLOCK));
		return PASS_OFF;
	}
}

static int find_scenario_index(struct pass_scenario_policy *scenario,
			       char *name)
{
	int index;

	for (index = 0; index < scenario->num_scenarios; index++)
		if (MATCH(scenario->list[index].name, name))
			return index;

	return -EINVAL;
}

/*
 * pass_notifier_pmqos - Callback function of DEVICE_NOTIFIER_PMQOS notifier
 * @data: the scenario name
 */
static int pass_notifier_pmqos(void *data)
{
	struct pass_policy *policy = policy_pmqos;
	struct pass_scenario_policy *scenario = &policy->scenario;
	struct pass_scenario *scn = NULL;
	enum pass_state locked = PASS_UNUSED;
	char name[PASS_NAME_LEN] = {""};
	unsigned int cpufreq_min_level = 0;
	unsigned int cpufreq_max_level = 0;
	int count = 0;
	int index = -1;
	int i;

	if (!is_enabled(policy))
		return 0;

	if (!is_pmqos_enabled(policy))
		return 0;

	/*
	 * Parse scenario name(data) whether to include 'Lock' or 'Unlock'
	 * string and divide correct scenario name.
	 */
	locked = is_pmqos_locked(data, name);
	index = find_scenario_index(scenario, name);
	if (index < 0) {
		_W("Unknown scenario (%s)\n", data);
		return -EINVAL;
	}
	scn = &scenario->list[index];

	/* Check the state of each scenario whether to support or not */
	if (scn->state != PASS_ON) {
		_W("cannot support '%s' scenario (support=%d)\n",
				name, scn->state);
		return 0;
	}

	/*
	 * Compare locked state of scenario with
	 * if state is same as existing state
	 */
	if (scn->locked == locked) {
		_E("'%s' scenario is already %s\n",
				name, locked ? "Locked" : "Unlocked");
		return 0;
	}
	scenario->list[index].locked = locked;

	if (locked)
		scenario->list[index].locked_time = get_time_ms();

	/* Change scaling scope according to scenario's level */
	for (i = 0; i < scenario->num_scenarios; i++) {
		struct pass_scenario *scn = &scenario->list[i];

		if (is_scenario_locked(scn)) {
			if (scn->cpufreq_min_level > cpufreq_min_level)
				cpufreq_min_level = scn->cpufreq_min_level;
			if (scn->cpufreq_max_level > cpufreq_max_level)
				cpufreq_max_level = scn->cpufreq_max_level;
			count++;
		}

		/*
		 * TODO: PASS have to control busfreq/gpufreq as same as cpufreq
		 */
	}

	/*
	 * Restore default min/max level if all scenarios hasn't locked state.
	 */
	if (!is_scenario_locked(scn) && !count) {
		cpufreq_min_level = policy->default_min_level;
		cpufreq_max_level = policy->default_max_level;
	}

	if (locked) {
		_I("Lock   '%s' scenario\n", name);
	} else {
		int64_t locked_time =
			get_time_ms() - scenario->list[index].locked_time;
		scenario->list[index].locked_time = 0;

		_I("UnLock '%s' scenario (%lldms)\n", name, locked_time);
	}

	pass_governor_change_level_scope(policy, cpufreq_min_level,
					 cpufreq_max_level);

	/* TODO: PASS have to control busfreq/gpufreq as same as cpufreq. */

	return 0;
}

/*
 * pass_notifier_booting_done - Callback function of DEVICE_NOTIFIER_BOOTING_
 *                              DONE notifier
 * @data: NULL, this parameter isn't used
 */
static int pass_notifier_booting_done(void *data)
{
	if (!policy_pmqos)
		return 0;

	/* Start PASS governor if 'pass_support' in pass.conf is true */
	if (policy_pmqos->state == PASS_ON && policy_pmqos->governor)
		policy_pmqos->governor->update(policy_pmqos, PASS_GOV_START);

	return 0;
}

/*
 * pass_notifier_init - Register notifiers and init
 * @policy: the instance of pass_policy structure
 */
static int pass_notifier_init(struct pass_policy *policy)
{
	/* Register DEVICE_NOTIFIER_PMQOS */
	register_notifier(DEVICE_NOTIFIER_PMQOS, pass_notifier_pmqos);

	/* Register DEVICE_NOTIFIER_BOOTING_DONE */
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE,
			  pass_notifier_booting_done);

	/*
	 * FIXME: Current notifier of deviced didn't pass separate user_data
	 * parameter on callback function. So, PASS must need global pass_policy
	 * instance. This code must be modified after changing deviced's
	 * notifier feature.
	 */
	policy_pmqos = policy;

	return 0;
}

/*
 * pass_notifier_exit - Un-register notifiers and exit
 * @policy: the instance of pass_policy structure
 */
static int pass_notifier_exit(struct pass_policy *policy)
{
	/* Un-register DEVICE_NOTIFIER_PMQOS */
	unregister_notifier(DEVICE_NOTIFIER_PMQOS, pass_notifier_pmqos);

	/* Un-Register DEVICE_NOTIFIER_BOOTING_DONE */
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE,
			    pass_notifier_booting_done);

	return 0;
}

/*****************************************************************************
 *                            PASS CPU Hotplug                               *
 ****************************************************************************/

/*
 * pass_hotplug_set_online - Change the maximum number of online cpu
 *
 * @policy: the instance of struct pass_policy
 * @max_online: the maximum number of online cpu
 */
static void pass_hotplug_set_online(struct pass_policy *policy,
					unsigned int online)
{
	struct pass_table *table = policy->pass_table;
	struct pass_hotplug *hotplug = policy->hotplug;
	int num_online;
	int i;

	if (!hotplug || online == hotplug->online)
		return;

	if (online > hotplug->max_online)
		online = hotplug->max_online;

	if (online > hotplug->online)
		num_online = online;
	else
		num_online = hotplug->online;

	for (i = 0 ; i < policy->cpufreq.num_nr_cpus; i++) {
		if (i < online)
			set_cpu_online(hotplug->sequence[i], PASS_CPU_UP);
		else
			set_cpu_online(hotplug->sequence[i], PASS_CPU_DOWN);
	}

	/*
	_I("- CPU %4s '%d->%d'Core\n",
		(hotplug->online > online ? "DOWN" : "UP"),
		hotplug->online, online);
	*/

	hotplug->online = online;
}

/*
 * pass_hotplug_stop - Stop PASS hotplug
 *
 * @policy: the instance of struct pass_policy
 */
static void pass_hotplug_stop(struct pass_policy *policy)
{
	struct pass_table *table = policy->pass_table;
	int level = policy->max_level;

	if (!policy->hotplug)
		return;

	policy->hotplug->online = table[level].limit_max_cpu;
	policy->hotplug->max_online = policy->cpufreq.num_nr_cpus;
}

static int pass_hotplug_dummy_governor(struct pass_policy *policy)
{
	int level = policy->curr_level;

	return policy->pass_table[level].limit_max_cpu;
}

/*
 * Define PASS hotplug
 *
 * - Dummy hotplug
 */
static struct pass_hotplug pass_hotplug_dummy = {
	.name		= "pass_hotplug_dummy",
	.governor	= pass_hotplug_dummy_governor,
};

/*
 * pass_get_governor - Return specific hotplug instance according to type
 *
 * @type: the type of PASS hotplug
 */
struct pass_hotplug* pass_get_hotplug(struct pass_policy *policy,
					enum pass_gov_type type)
{
	switch (type) {
	case PASS_GOV_STEP:
	case PASS_GOV_RADIATION:
		return &pass_hotplug_dummy;
	default:
		_E("Unknown hotplug type");
		break;
	};

	return NULL;
}

/*****************************************************************************
 *                              PASS governor                                *
 ****************************************************************************/

/*
 * pass_governor_change_level - Change maximum cpu frequency and number of online cpu
 *
 * @policy: the instance of struct pass_policy
 * @level: the pass level
 */
static int pass_governor_change_level(struct pass_policy *policy, int new_level)
{
	struct pass_table *table = policy->pass_table;
	struct pass_hotplug *hotplug = policy->hotplug;
	int curr_level = policy->curr_level;
	int limit_max_freq;
	int curr_max_freq;
	int curr_min_freq;
	int limit_max_cpu;
	int online;
	int i;

	if (new_level > policy->max_level)
		new_level = policy->max_level;

	if (new_level < policy->min_level)
		new_level = policy->min_level;

	if (new_level == curr_level)
		return 0;

	/*
	 * Get maximum CPU frequency/the maximum number of online CPU
	 * according to PASS level.
	 */
	limit_max_freq = table[new_level].limit_max_freq;
	limit_max_cpu = table[new_level].limit_max_cpu;

	policy->prev_level = policy->curr_level;
	policy->curr_level = new_level;

	/* Turn on/off CPUs according the maximum number of online CPU */
	if (hotplug) {
		if (hotplug->max_online != limit_max_cpu)
			hotplug->max_online = limit_max_cpu;

		if (hotplug->governor)
			online = hotplug->governor(policy);
		else
			online = 1;

		curr_max_freq = get_cpufreq_scaling_max_freq();
		curr_min_freq = get_cpufreq_scaling_min_freq();

		set_cpufreq_scaling_min_freq(curr_max_freq);

		pass_hotplug_set_online(policy, online);

		set_cpufreq_scaling_min_freq(curr_min_freq);
	}

	/* Set maximum CPU frequency */
	set_cpufreq_scaling_max_freq(limit_max_freq);

	_I("Level %4s '%d->%d' : '%d'Hz/'%d'Core\n",
		(curr_level > new_level ? "DOWN" : "UP"),
		curr_level, new_level, limit_max_freq, limit_max_cpu);

	return 0;
};

/*
 * pass_governor_change_level_scope - Change the scope of cpufreq scaling
 *
 * @policy: the instance of struct pass_policy
 * @min_level: the minimum level of cpufreq scaling
 * @max_level: the maximum level of cpufreq scaling
 */
static int pass_governor_change_level_scope(struct pass_policy *policy,
					    int min_level, int max_level)
{
	if (!policy)
		return -EINVAL;

	if (min_level > max_level) {
		_E("min_level(%d) have to be smaller than max_level(%d)\n",
						min_level, max_level);
		return -EINVAL;
	}

	if (min_level == policy->min_level
		&& max_level == policy->max_level)
		return 0;

	/* Change minimum/maximum level of cpufreq scaling */
	policy->min_level = min_level;
	policy->max_level = max_level;

	pass_governor_change_level(policy, policy->curr_level);

	return 0;
}

/*
 * pass_calculate_busy_cpu - Count a number of busy cpu among NR_CPUS by using
 *			     runnable_avg_sum/runnable_avg_period
 *
 * @policy: the instance of struct pass_policy
 */
static void pass_calculate_busy_cpu(struct pass_policy *policy)
{
	struct pass_cpu_stats *stats = policy->pass_cpu_stats;
	struct pass_table *table = policy->pass_table;
	unsigned int level = policy->curr_level;
	unsigned int cpu_threshold = 0;
	unsigned int busy_cpu;
	unsigned int cur_freq;
	unsigned int load;
	unsigned int sum_load;
	unsigned int sum_runnable_load;
	unsigned int nr_runnings;
	int limit_max_cpu;
	int i;
	int j;

	limit_max_cpu = table[level].limit_max_cpu;

	for (i = 0; i < policy->num_pass_cpu_stats; i++) {
		cur_freq = stats[i].freq;
		nr_runnings = stats[i].nr_runnings;

		stats[i].num_busy_cpu = 0;
		stats[i].avg_load = 0;
		stats[i].avg_runnable_load = 0;
		stats[i].avg_thread_load = 0;
		stats[i].avg_thread_runnable_load = 0;

		busy_cpu = 0;
		sum_load = 0;
		sum_runnable_load = 0;

		/* Calculate the number of busy cpu */
		for (j = 0; j < policy->cpufreq.num_nr_cpus; j++) {
			load = stats[i].load[j];
			sum_load += stats[i].load[j];
			sum_runnable_load += stats[i].runnable_load[j];
			if (!load)
				continue;

			cpu_threshold =
				(unsigned int)(cur_freq * load)
						/ policy->cpufreq.max_freq;
			if (load == 100
				|| cpu_threshold >= policy->pass_cpu_threshold)
				busy_cpu++;
		}

		stats[i].num_busy_cpu = busy_cpu;
		stats[i].avg_load = sum_load / limit_max_cpu;
		stats[i].avg_runnable_load = sum_runnable_load / limit_max_cpu;
		if (nr_runnings) {
			stats[i].avg_thread_load
				= (sum_load * 100) / nr_runnings;
			stats[i].avg_thread_runnable_load
				= (sum_runnable_load * 100) / nr_runnings;
		}
	}
}

/*
 * pass_governor_stop - Stop PASS governor through D-Bus
 *
 * @policy: the instance of struct pass_policy
 */
static void pass_governor_stop(struct pass_policy *policy)
{
	if (!policy->governor) {
		_E("cannot stop PASS governor");
		return;
	}

	if (policy->gov_state == PASS_GOV_STOP) {
		_E("PASS governor is already inactive state");
		return;
	}

	/* Restore maximum cpu freq/the number of online cpu */
	pass_governor_change_level(policy, policy->num_levels - 1);

	pass_hotplug_stop(policy);

	if (policy->governor->gov_timer_id) {
		ecore_timer_del(policy->governor->gov_timer_id);
		policy->governor->gov_timer_id = NULL;
	}

	/* Set PASS state as PASS_GOV_STOP */
	policy->gov_state = PASS_GOV_STOP;

	_I("Stop PASS governor");
}

/*
 * pass_governor_core_timer - Callback function of core timer for PASS governor
 *
 * @data: the instance of struct pass_policy
 */
static Eina_Bool pass_governor_core_timer(void *data)
{
	struct pass_policy *policy = (struct pass_policy *)data;
	static int count = 0;
	int level;
	int online;
	int ret;

	if (!policy) {
		_E("cannot execute PASS core timer");
		return ECORE_CALLBACK_CANCEL;
	}

	/*
	 * Collect data related to system state
	 * - current cpu frequency
	 * - the number of nr_running
	 * - cpu load (= runnable_avg_sum * 100 / runnable_avg_period)
	 */
	ret = get_pass_cpu_stats(policy);
	if (ret < 0) {
		if (count++ < PASS_CPU_STATS_MAX_COUNT)
			return ECORE_CALLBACK_RENEW;

		count = 0;

		_E("cannot read 'pass_cpu_stats' sysfs entry");
		pass_governor_stop(policy);

		return ECORE_CALLBACK_CANCEL;
	}

	/* Calculate the number of busy cpu */
	pass_calculate_busy_cpu(policy);

	/* Determine the amount of proper resource */
	if (policy->governor->governor) {
		level = policy->governor->governor(policy);

		pass_governor_change_level(policy, level);
	} else {
		_E("cannot execute governor function");
		pass_governor_stop(policy);
		return ECORE_CALLBACK_CANCEL;
	}

	/*
	 * Change the period of govenor timer according to PASS level
	 */
	if (policy->pass_table[level].gov_timeout >= PASS_MIN_GOV_TIMEOUT &&
		(policy->governor->gov_timeout
		 != policy->pass_table[level].gov_timeout)) {

		_I("Change the period of governor timer from %fs to %fs\n",
				policy->governor->gov_timeout,
				policy->pass_table[level].gov_timeout);

		policy->governor->gov_timeout =
			policy->pass_table[level].gov_timeout;
		ecore_timer_interval_set(policy->governor->gov_timer_id,
					policy->governor->gov_timeout);
		ecore_timer_reset(policy->governor->gov_timer_id);
	}

	return ECORE_CALLBACK_RENEW;
}

/*
 * pass_governor_start - Start PASS governor through D-Bus
 *
 * @policy: the instance of struct pass_policy
 */
static void pass_governor_start(struct pass_policy *policy)
{
	if (!policy->governor) {
		_E("cannot start PASS governor");
		return;
	}

	if (is_enabled(policy)) {
		_E("PASS governor is already active state");
		return;
	}

	/* Create the core timer of PASS governor */
	policy->governor->gov_timer_id = ecore_timer_add(
				policy->governor->gov_timeout,
				(Ecore_Task_Cb)pass_governor_core_timer,
				(void *)policy);
	if (!policy->governor->gov_timer_id) {
		_E("cannot add core timer for governor");
		pass_governor_stop(policy);
		return;
	}

	/*
	 * Set default pass level when starting pass
	 * - default pass level according to policy->init_level
	 */
	policy->curr_level = -1;
	if (policy->init_level > policy->max_level)
		policy->init_level = policy->max_level;
	pass_governor_change_level(policy, policy->init_level);

	/* Set PASS state as PASS_GOV_START */
	policy->gov_state = PASS_GOV_START;

	_I("Start PASS governor");
}

/*
 * pass_governor_init - Initialize PASS governor
 *
 * @policy: the instance of struct pass_policy
 */
static int pass_governor_init(struct pass_policy *policy)
{
	int max_level;
	int ret;

	if(policy->governor->gov_timeout < 0) {
		_E("invalid timeout value [%d]!",
			policy->governor->gov_timeout);
		pass_governor_stop(policy);
		return -EINVAL;
	}

	/* Set default PASS state */
	policy->gov_state = PASS_GOV_STOP;

	/* Initialize notifier */
	pass_notifier_init(policy);

	_I("Initialize PASS (Power Aware System Service)");

	return 0;
}

/*
 * pass_governor_exit - Exit PASS governor
 */
static int pass_governor_exit(struct pass_policy *policy)
{
	int i;

	/* Exit notifier */
	pass_notifier_exit(policy);

	/*
	 * Stop core timer and
	 * Restore maximum online cpu/cpu frequency
	 */
	pass_governor_stop(policy);

	/* Free allocated memory */
	for (i = 0; i < policy->num_pass_cpu_stats; i++) {
		free(policy->pass_cpu_stats[i].load);
		free(policy->pass_cpu_stats[i].nr_running);
		free(policy->pass_cpu_stats[i].runnable_load);
	}
	free(policy->pass_cpu_stats);

	if (policy->hotplug)
		free(policy->hotplug->sequence);

	/* Set pass_policy structure as default value */
	policy->pass_cpu_threshold = 0;
	policy->up_threshold = 0;
	policy->down_threshold = 0;

	policy->prev_level = 0;
	policy->curr_level = 0;
	policy->min_level = 0;
	policy->max_level = 0;
	policy->level_up_threshold = 0;

	policy->pass_table = NULL;
	policy->num_pass_cpu_stats = 0;

	policy->governor->gov_timeout = 0;

	policy->governor = NULL;

	_I("Exit PASS (Power Aware System Service)");

	return 0;
}

/*
 * pass_governor_update - Restart/Pause PASS governor
 *
 * @cond: the instance of struct pass_policy
 */
static int pass_governor_update(struct pass_policy *policy,
			enum pass_gov_state state)
{
	if (!policy) {
		_E("cannot update PASS governor");
		return -EINVAL;
	}

	switch (state) {
	case PASS_GOV_START:
		pass_governor_start(policy);
		break;
	case PASS_GOV_STOP:
		pass_governor_stop(policy);
		break;
	default:
		_E("Unknown governor state");
		return -EINVAL;
	}

	return 0;
}

/*
 * Define PASS governor
 *
 * - Step governor
 * - Radiation governor
 */
static struct pass_governor pass_gov_step = {
	.name		= "pass_step",
	.init		= pass_governor_init,
	.exit		= pass_governor_exit,
	.update		= pass_governor_update,

	.governor	= pass_step_governor,
};

static struct pass_governor pass_gov_radiation = {
	.name		= "pass_radiation",
	.init		= pass_governor_init,
	.exit		= pass_governor_exit,
	.update		= pass_governor_update,

	.governor	= pass_radiation_governor,
};

/*
 * pass_get_governor - Return specific governor instance according to type
 *
 * @type: the type of PASS governor
 */
struct pass_governor* pass_get_governor(struct pass_policy *policy,
						enum pass_gov_type type)
{
	switch (type) {
	case PASS_GOV_STEP:
		return &pass_gov_step;
	case PASS_GOV_RADIATION:
		return &pass_gov_radiation;
	default:
		_E("Unknown governor type");
		break;
	};

	return NULL;
}
