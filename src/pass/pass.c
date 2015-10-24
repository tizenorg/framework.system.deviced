/*
 * PASS (Power Aware System Service)
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


#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pass.h"
#include "pass-core.h"
#include "pass-target.h"

#include "core/devices.h"
#include "core/common.h"
#include "core/edbus-handler.h"

#define PASS_DEFAULT_MIN_LEVEL			0
#define PASS_DEFAULT_CPU_THRESHOLD		20
#define PASS_DEFAULT_LEVEL_UP_THRESHOLD		30
#define PASS_DEFAULT_LEVEL_DOWN_THRESHOLD	80

/*
 * Per-target pass policy
 */
static struct pass_policy policy;

/******************************************************
 *                PASS D-Bus interface                *
 ******************************************************/
static DBusMessage *e_dbus_start_cb(E_DBus_Object *obj, DBusMessage *msg)
{
	if (policy.governor)
		policy.governor->update(&policy, PASS_GOV_START);
	return dbus_message_new_method_return(msg);
}

static DBusMessage *e_dbus_stop_cb(E_DBus_Object *obj, DBusMessage *msg)
{
	if (policy.governor)
		policy.governor->update(&policy, PASS_GOV_STOP);
	return dbus_message_new_method_return(msg);
}

static const struct edbus_method edbus_methods[] = {
	{"start",	NULL,	NULL,	e_dbus_start_cb},
	{"stop",	NULL,	NULL,	e_dbus_stop_cb},
};

/******************************************************
 *                PASS interface (Init/Exit)          *
 ******************************************************/

/*
 * pass_init - Initialize PASS(Power Aware System Service)
 *
 * @data: the instance of structre pass_policy
 */
static void pass_init(void *data)
{
	enum pass_target_type target_type;
	int max_freq = 0;
	int max_cpu = 0;
	int ret;
	int i;

	/*
	 * Initialize pass-table by parsing pass.conf
	 */
	ret = get_pass_table(&policy, PASS_CONF_PATH);
	if (ret < 0) {
		_E("cannot parse %s\n", PASS_CONF_PATH);
		return;
	}

	/*
	 * Initialzie D-Bus interface of PASS. User can be able to
	 * turn on/off PASS through D-Bus interface.
	 */
	ret = register_edbus_interface_and_method(DEVICED_PATH_PASS,
			DEVICED_INTERFACE_PASS,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0) {
		_I("cannot initialize PASS D-Bus (%d)", ret);
		return;
	}

	/* Check whether PASS is initialzied state or not */
	if (policy.governor) {
		_I("PASS is already active state");
		return;
	}

	/*
	 * Set default value to global pass_policy instance
	 * if variable isn't initialized.
	 */
	if (!policy.min_level)
		policy.min_level = PASS_DEFAULT_MIN_LEVEL;
	policy.default_min_level = policy.min_level;

	if (!policy.max_level)
		policy.max_level = policy.num_levels - 1;
	policy.default_max_level = policy.max_level;

	if (!policy.init_level)
		policy.init_level = PASS_DEFAULT_MIN_LEVEL;

	if (!policy.pass_cpu_threshold)
		policy.pass_cpu_threshold = PASS_DEFAULT_CPU_THRESHOLD;

	if (!policy.up_threshold)
		policy.up_threshold = PASS_DEFAULT_LEVEL_UP_THRESHOLD;

	if (!policy.down_threshold)
		policy.down_threshold = PASS_DEFAULT_LEVEL_DOWN_THRESHOLD;

	if (!policy.level_up_threshold)
		policy.level_up_threshold = policy.max_level;

	if (!policy.num_pass_cpu_stats)
		policy.num_pass_cpu_stats = PASS_CPU_STATS_DEFAULT;

	for (i = 0; i < policy.num_levels; i++) {
		if (max_freq < policy.pass_table[i].limit_max_freq)
			max_freq = policy.pass_table[i].limit_max_freq;
		if (max_cpu < policy.pass_table[i].limit_max_cpu)
			max_cpu = policy.pass_table[i].limit_max_cpu;
	}
	policy.cpufreq.max_freq = max_freq;
	policy.cpufreq.num_nr_cpus = max_cpu;

	/* Allocate memory according to the number of data and cpu */
	policy.pass_cpu_stats = malloc(
			sizeof(struct pass_cpu_stats) * policy.num_pass_cpu_stats);

	for (i = 0; i < policy.num_pass_cpu_stats; i++) {
		policy.pass_cpu_stats[i].load =
			malloc(sizeof(unsigned int) * policy.cpufreq.num_nr_cpus);
		policy.pass_cpu_stats[i].nr_running =
			malloc(sizeof(unsigned int) * policy.cpufreq.num_nr_cpus);
		policy.pass_cpu_stats[i].runnable_load =
			malloc(sizeof(unsigned int) * policy.cpufreq.num_nr_cpus);
	}

	/* Get the instance of PASS governor */
	policy.governor = pass_get_governor(&policy, policy.gov_type);
	if (!policy.governor) {
		_E("cannot get the instance of PASS governor");
		return;
	}
	policy.governor->gov_timeout	= policy.gov_timeout;

	/* Get the instance of PASS hotplulg */
	policy.hotplug = pass_get_hotplug(&policy, policy.gov_type);
	if (!policy.hotplug) {
		_E("cannot get the instance of PASS hotplug");
	} else {
		policy.hotplug->sequence = malloc(
				sizeof(int) * policy.cpufreq.num_nr_cpus);
		for (i = 0; i < policy.cpufreq.num_nr_cpus; i++)
			policy.hotplug->sequence[i] = i;
	}

	if (policy.governor->init) {
		ret = policy.governor->init(&policy);
		if (ret < 0) {
			_E("cannot initialize PASS governor");
			return;
		}
	} else {
		_E("cannot execute init() of PASS governor");
		return;
	}
}

/*
 * pass_exit - Exit PASS
 *
 * @data: the instance of structre pass_policy
 */
static void pass_exit(void *data)
{
	int ret;

	if (!policy.governor) {
		_E("cannot exit PASS");
		return;
	}

	put_pass_table(&policy);

	if (policy.governor->exit) {
		ret = policy.governor->exit(&policy);
		if (ret < 0) {
			_E("cannot exit PASS governor");
			return;
		}
	} else {
		_E("cannot execute exit() of PASS governor");
		return;
	}

	policy.governor = NULL;
}

static const struct device_ops pass_device_ops = {
	.name     = "pass",
	.init     = pass_init,
	.exit     = pass_exit,
};

DEVICE_OPS_REGISTER(&pass_device_ops)
