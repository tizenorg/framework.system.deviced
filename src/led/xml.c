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
#include <libxml/parser.h>

#include "deviced/dd-led.h"
#include "xml.h"
#include "core/log.h"
#include "core/list.h"

#ifndef DATADIR
#define DATADIR "/usr/share/deviced"
#endif

#define LED_XML		DATADIR"/led.xml"
#define LED_STR		"led"

static const char* led_str[] = {
	[LED_OFF] = "off",
	[LED_LOW_BATTERY] = "low battery",
	[LED_CHARGING] = "charging",
	[LED_FULLY_CHARGED] = "fully charged",
	[LED_CHARGING_ERROR] = "charging error",
	[LED_MISSED_NOTI] = "missed noti",
	[LED_VOICE_RECORDING] = "voice recording",
	[LED_POWER_OFF] = "power off",
	[LED_CUSTOM] = "custom",
};

static dd_list *led_head;

static xmlDocPtr xml_open(const char *xml)
{
	xmlDocPtr doc;

	doc = xmlReadFile(xml, NULL, 0);
	if (doc == NULL) {
		_E("xmlReadFile fail");
		return NULL;
	}

	return doc;
}

static void xml_close(xmlDocPtr doc)
{
	xmlFreeDoc(doc);
}

static xmlNodePtr xml_find(xmlDocPtr doc, const xmlChar* expr)
{
	xmlNodePtr root;
	xmlNodePtr cur;
	xmlChar *key;

	assert(doc);
	assert(expr);

	root = xmlDocGetRootElement(doc);
	if (root == NULL) {
		_E("xmlDocGetRootElement fail");
		return NULL;
	}

	for (cur = root->children; cur != NULL; cur = cur->next) {
		if (xmlStrcmp(cur->name, (const xmlChar*)LED_STR))
			continue;

		key = xmlGetProp(cur, (const xmlChar*)"label");
		if (key && !xmlStrcmp(key, expr)) {
			_D("%s", key);
			xmlFree(key);
			return cur;
		}
	}

	return NULL;
}

static struct led_mode *xml_parse(xmlDocPtr doc, xmlNodePtr cur)
{
	xmlNodePtr node;
	xmlNodePtr attr;
	struct led_mode *led;
	xmlChar *key;
	int i = 0;

	assert(doc);
	assert(cur);

	led = (struct led_mode*)malloc(sizeof(struct led_mode));
	if (led == NULL) {
		_E("out of memory");
		return NULL;
	}

	for (node = cur->children; node != NULL; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (!xmlStrcmp(node->name, "priority")) {
			key = xmlNodeListGetString(doc, node->children, 1);
			if (!key)
				continue;
			led->data.priority = atoi(key);
			xmlFree(key);
		} else if (!xmlStrcmp(node->name, "data")) {
			key = xmlGetProp(node, (const xmlChar*)"on");
			if (!key)
				continue;
			led->data.on = atoi(key);
			xmlFree(key);

			key = xmlGetProp(node, (const xmlChar*)"off");
			if (!key)
				continue;
			led->data.off = atoi(key);
			xmlFree(key);

			key = xmlGetProp(node, (const xmlChar*)"color");
			if (!key)
				continue;
			led->data.color = (unsigned int)strtoul(key, NULL, 16);
			xmlFree(key);

			key = xmlGetProp(node, (const xmlChar*)"wave");
			if (!key || xmlStrcmp(key, "on"))
				led->data.wave = false;
			else
				led->data.wave = true;
			xmlFree(key);
		}
	}

	_D("priority : %d, on : %d, off : %d, color : %x, wave : %d",
			led->data.priority, led->data.on, led->data.off,
			led->data.color, led->data.wave);
	return led;
}

int get_led_data(void)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	struct led_mode *led;
	int i;
	int r;

	doc = xml_open(LED_XML);
	if (doc == NULL) {
		_E("xml_open(%s) fail", LED_XML);
		errno = EPERM;
		return -1;
	}

	for (i = 0; i < LED_MODE_MAX; ++i) {
		cur = xml_find(doc, (const xmlChar*)led_str[i]);
		if (cur == NULL) {
			_E("xml_find(%s) fail", led_str[i]);
			break;
		}

		led = xml_parse(doc, cur);
		if (led == NULL) {
			_E("xml_parse fail");
			break;
		}

		led->mode = i;
		led->state = 0;
		DD_LIST_APPEND(led_head, led);
	}

	xml_close(doc);
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

struct led_mode *get_valid_led_data(void)
{
	dd_list *l;
	struct led_mode *node;
	struct led_mode *cur = NULL;
	int pr = 2;

	DD_LIST_FOREACH(led_head, l, node) {
		if (!node->state)
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

	_D("Mode State priority   on  off    color wave");
	DD_LIST_FOREACH(led_head, l, node) {
		_D("%4d %5d %8d %4d %4d %x %4d",
				node->mode, node->state, node->data.priority,
				node->data.on, node->data.off, node->data.color, node->data.wave);
	}
}
