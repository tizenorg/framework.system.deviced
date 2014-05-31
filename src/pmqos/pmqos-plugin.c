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
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/config-parser.h"
#include "pmqos.h"

static bool is_supported(const char *value)
{
	assert(value);

	if (MATCH(value, "yes"))
		return true;
	return false;
}

static int pmqos_parse_scenario(struct parse_result *result, void *user_data, unsigned int index)
{
	struct pmqos_scenario *scenarios = (struct pmqos_scenario*)user_data;

	assert(result);
	assert(result->section && result->name && result->value);

	/* Parse 'PmqosScenario' section */
	if (MATCH(result->section, "PmqosScenario")) {
		if (MATCH(result->name, "scenario_support"))
			scenarios->support = is_supported(result->value);
		else if (MATCH(result->name, "scenario_num")) {
			scenarios->num = atoi(result->value);
			if (scenarios->num > 0) {
				scenarios->list = malloc(sizeof(struct scenario)*scenarios->num);
				if (!scenarios->list) {
					_E("failed to allocat memory for scenario");
					return -errno;
				}
			}
		}
	}

	/* Do not support pmqos scenario */
	if (!scenarios->support)
		return 0;

	/* Do not have pmqos scenario */
	if (!scenarios->num)
		return 0;

	/* No item to parse */
	if (index > scenarios->num)
		return 0;

	/* Parse 'Scenario' section */
	if (MATCH(result->name, "name"))
		strcpy(scenarios->list[index].name, result->value);
	else if (MATCH(result->name, "support"))
		scenarios->list[index].support = is_supported(result->value);

	return 0;
}

static int pmqos_load_config(struct parse_result *result, void *user_data)
{
	struct pmqos_scenario *scenarios = (struct pmqos_scenario*)user_data;
	char name[NAME_MAX];
	int ret;
	static int index;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	/* Parsing 'PMQOS' section */
	if (MATCH(result->section, "PMQOS"))
		goto out;

	/* Parsing 'Pmqos Scenario' section */
	if (MATCH(result->section, "PmqosScenario")) {
		ret = pmqos_parse_scenario(result, user_data, -1);
		if (ret < 0) {
			_E("failed to parse [PmqosScenario] section : %d", ret);
			return ret;
		}
		goto out;
	}

	/* Parsing 'Scenario' section */
	for (index = 0; index < scenarios->num; ++index) {
		snprintf(name, sizeof(name), "Scenario%d", index);

		if (MATCH(result->section, name)) {
			ret = pmqos_parse_scenario(result, user_data, index);
			if (ret < 0) {
				_E("failed to parse [Scenario%d] section : %d", index, ret);
				return ret;
			}
			goto out;
		}
	}

out:
	return 0;
}

int release_pmqos_table(struct pmqos_scenario *scenarios)
{
	if (!scenarios)
		return -EINVAL;

	if (scenarios->num > 0 && !scenarios->list)
		free(scenarios->list);

	return 0;
}

int get_pmqos_table(const char *path, struct pmqos_scenario *scenarios)
{
	int ret;

	/* get configuration file */
	ret = config_parse(path, pmqos_load_config, scenarios);
	if (ret < 0) {
		_E("failed to load conficuration file(%s) : %d", path, ret);
		release_pmqos_table(scenarios);
		return ret;
	}

	return 0;
}
