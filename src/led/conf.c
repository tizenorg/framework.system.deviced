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
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "deviced/dd-led.h"
#include "core/log.h"
#include "core/list.h"
#include "core/config-parser.h"
#include "conf.h"

#define LED_CONF	"/etc/deviced/led.conf"
#define LED_STR		"led"

static const char* led_str[] = {
	[LED_OFF] = "Off",
	[LED_LOW_BATTERY] = "Low battery",
	[LED_CHARGING] = "Charging",
	[LED_FULLY_CHARGED] = "Fully charged",
	[LED_CHARGING_ERROR] = "Charging error",
	[LED_MISSED_NOTI] = "Missed noti",
	[LED_VOICE_RECORDING] = "Voice recording",
	[LED_REMOTE_CONTROLLER] = "Remote controller",
	[LED_AIR_WAKEUP] = "Air Wakeup",
	[LED_POWER_OFF] = "Power off",
	[LED_CUSTOM] = "Custom",
};

static dd_list *led_head;

static int get_selected_lcd_mode(const char *value)
{
	int lcd = 0;

	assert(value);

	if (strstr(value, "on"))
		lcd |= DURING_LCD_ON;
	if (strstr(value, "off"))
		lcd |= DURING_LCD_OFF;

	return lcd;
}

static int parse_scenario(struct parse_result *result, void *user_data)
{
	dd_list *head = (dd_list*)led_head;
	struct led_mode *led;
	int index;

	for (index = 0; index < LED_MODE_MAX; ++index) {
		if (MATCH(result->section, led_str[index]))
			break;
	}

	if (index == LED_MODE_MAX) {
		_E("No matched valid scenario : %s", result->section);
		return -EINVAL;
	}

	/* search for matched led_data */
	led = find_led_data(index);
	/* If there is no matched data */
	if (!led) {
		/* allocate led_data memory */
		led = (struct led_mode*)calloc(1, sizeof(struct led_mode));
		if (!led) {
			_E("out of memory : %s", strerror(errno));
			return -errno;
		}

		/* set default value */
		led->mode = index;
		led->state = 0;
		led->data.wave = false;
		led->data.lcd = DURING_LCD_OFF;
		led->data.duration = -1;

		/* Add to list */
		DD_LIST_APPEND(led_head, led);
	}

	/* parse the result data */
	if (MATCH(result->name, "priority"))
		led->data.priority = atoi(result->value);
	else if (MATCH(result->name, "on"))
		led->data.on = atoi(result->value);
	else if (MATCH(result->name, "off"))
		led->data.off = atoi(result->value);
	else if (MATCH(result->name, "color"))
		led->data.color = (unsigned int)strtoul(result->value, NULL, 16);
	else if (MATCH(result->name, "wave"))
		led->data.wave = (!strcmp(result->value, "on") ? true : false);
	else if (MATCH(result->name, "lcd"))
		led->data.lcd = get_selected_lcd_mode(result->value);
	else if (MATCH(result->name, "duration"))
		led->data.duration = atoi(result->value);

	return 0;
}

static int led_load_config(struct parse_result *result, void *user_data)
{
	int ret;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	/* Parsing 'LED' section */
	if (MATCH(result->section, "LED"))
		return 0;

	/* Parsing 'Scenario' section */
	ret = parse_scenario(result, user_data);
	if (ret < 0) {
		_E("failed to parse %s section", result->section);
		return ret;
	}

	return 0;
}

int get_led_data(void)
{
	int ret;

	/* get configuration file */
	ret = config_parse(LED_CONF, led_load_config, led_head);
	if (ret < 0) {
		_E("failed to load configuration file(%s) : %d", LED_CONF, ret);
		release_led_data();
		return ret;
	}

	/* for debug */
	print_all_data();

	return 0;
}

void release_led_data(void)
{
	dd_list *l;
	struct led_mode *node;

	DD_LIST_FOREACH(led_head, l, node) {
		_D("node name : %d", node->mode);
		DD_LIST_REMOVE(led_head, node);
		free(node);
	}
}

struct led_mode *find_led_data(int mode)
{
	dd_list *l;
	struct led_mode *node;

	DD_LIST_FOREACH(led_head, l, node) {
		if (node->mode == mode)
			return node;
	}

	return NULL;
}

struct led_mode *get_valid_led_data(int lcd_state)
{
	dd_list *l;
	struct led_mode *node;
	struct led_mode *cur = NULL;
	int pr = 3;

	DD_LIST_FOREACH(led_head, l, node) {
		if (!node->state)
			continue;

		if (!(node->data.lcd & lcd_state))
			continue;

		if (node->data.priority <= pr) {
			pr = node->data.priority;
			cur = node;
		}
	}

	return cur;
}

void print_all_data(void)
{
	dd_list *l;
	struct led_mode *node;

	_D("Mode State priority   on  off    color lcd wave");
	DD_LIST_FOREACH(led_head, l, node) {
		_D("%4d %5d %8d %4d %4d %x %4d %4d",
				node->mode, node->state, node->data.priority,
				node->data.on, node->data.off, node->data.color,
				node->data.lcd, node->data.wave);
	}
}
