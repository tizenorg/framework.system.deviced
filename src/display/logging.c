/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include "weaks.h"
#include "util.h"

#define MAX_LOG_COUNT 250

struct pm_history {
	time_t time;
	enum pm_log_type log_type;
	int keycode;
};

static const int max_history_count = MAX_LOG_COUNT;
static struct pm_history pm_history_log[MAX_LOG_COUNT] = {0,};
static int history_count;

static const char history_string[PM_LOG_MAX][15] = {
	"PRESS", "LONG PRESS", "RELEASE", "LCD ON", "LCD ON FAIL",
	"LCD DIM", "LCD DIM FAIL", "LCD OFF", "LCD OFF FAIL", "SLEEP"
};

void pm_history_init(void)
{
	memset(pm_history_log, 0x0, sizeof(pm_history_log));
	history_count = 0;
}

void pm_history_save(enum pm_log_type log_type, int code)
{
	time_t now;

	time(&now);
	pm_history_log[history_count].time = now;
	pm_history_log[history_count].log_type = log_type;
	pm_history_log[history_count].keycode = code;
	history_count++;

	if (history_count >= max_history_count)
		history_count = 0;
}

void pm_history_print(FILE *fp, int count)
{
	int start_index, index, i;
	char time_buf[30];

	if (count <= 0 || count > max_history_count)
		return;

	start_index = (history_count - count + max_history_count)
		    % max_history_count;

	for (i = 0; i < count; i++) {
		index = (start_index + i) % max_history_count;

		if (pm_history_log[index].time == 0)
			continue;

		if (pm_history_log[index].log_type < PM_LOG_MIN ||
		    pm_history_log[index].log_type >= PM_LOG_MAX)
			continue;
		ctime_r(&pm_history_log[index].time, time_buf);

		LOG_DUMP(fp, "[%3d] %15s %3d %s", index,
		    history_string[pm_history_log[index].log_type],
		    pm_history_log[index].keycode, time_buf);
	}
}

