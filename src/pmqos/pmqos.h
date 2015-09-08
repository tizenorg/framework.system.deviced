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


#ifndef __PMQOS_H__
#define __PMQOS_H__

#include <stdbool.h>
#include <limits.h>

struct pmqos_scenario {
	struct scenario {
		char name[NAME_MAX];
		bool support;
	} *list;
	int num;
	bool support;
};

int get_pmqos_table(const char *path, struct pmqos_scenario *scenarios);
int release_pmqos_table(struct pmqos_scenario *scenarios);

#endif /* __PMQOS_H__ */
