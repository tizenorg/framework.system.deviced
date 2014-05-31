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

#include <vconf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include "core/log.h"
#include "core/list.h"
#include "usb-client.h"

#define BUF_MAX 256

#define DEFAULT_IPRODUCT_NAME "TIZEN"

#define CONF_PATH                "/usr/share/deviced/usb-configurations"
#define SUPPORTED_CONF_PATH      CONF_PATH"/conf-supported.xml"
#define USB_CONFIGURATIONS_PATH  CONF_PATH"/usb-configurations.xml"

#define USB_CON_SET             "set"
#define USB_CON_UNSET           "unset"

#define NODE_NAME_TEXT          "text"
#define NODE_NAME_COMMENT       "comment"
#define NODE_NAME_CONFIG_NODES  "config-nodes"
#define NODE_NAME_CONFIG_files  "config-files"
#define NODE_NAME_USB_CONFIG    "usb-config"
#define NODE_NAME_CONF_FILE     "conf-file"
#define NODE_NAME_USB_CONFS     "usb-configurations"
#define NODE_NAME_USB_OPERS     "usb-operations"
#define NODE_NAME_DRIVER        "driver"
#define NODE_NAME_USB_DRV       "usb-drv"
#define NODE_NAME_MODE          "mode"
#define NODE_NAME_ACTION        "action"
#define NODE_NAME_IPRODUCT      "iProduct"
#define ATTR_VALUE              "value"
#define ATTR_VERSION            "ver"
#define ATTR_BACKGROUND         "background"

static dd_list *supported_list;  /* Supported Configurations */
static dd_list *oper_list;       /* Operations for USB mode */
static dd_list *conf_list;       /* Configurations for usb mode */
static char *driver;             /* usb driver version (ex. 1.0, 1.1, none, ... ) */

int get_operations_list(dd_list **list)
{
	if (!list)
		return -EINVAL;

	*list = oper_list;
	return 0;
}

int get_configurations_list(dd_list **list)
{
	if (!list)
		return -EINVAL;

	*list = conf_list;
	return 0;
}

#ifdef USB_DEBUG
static void show_supported_confs_list(void)
{
	int i;
	dd_list *l;
	struct xmlSupported *sp;
	void *mode;

	if (!supported_list) {
		_D("Supported Confs list is empty");
		return;
	}

	_D("********************************");
	_D("** Supported Confs list");
	_D("********************************");

	DD_LIST_FOREACH(supported_list, l, sp) {
		_D("** Mode      : %d", sp->mode);
		_D("********************************");
	}
}

static void show_operation_list(void)
{
	int i;
	dd_list *l;
	struct xmlOperation *oper;

	_D("********************************");
	_D("** Operation list");
	_D("********************************");
	if (!oper_list) {
		_D("Operation list is empty");
		_D("********************************");
		return;
	}

	i = 0;
	DD_LIST_FOREACH(oper_list, l, oper) {
		_D("** Number    : %d", i++);
		_D("** Name      : %s", oper->name);
		_D("** Operation : %s", oper->oper);
		_D("** Background: %d", oper->background);
		_D("********************************");
	}
}

static void show_configuration_list(void)
{
	int i;
	dd_list *l;
	struct xmlConfiguration *conf;

	if (!conf_list) {
		_D("Configuration list is empty");
		return;
	}

	_D("********************************");
	_D("** Configuration list");
	_D("********************************");

	i = 0;
	DD_LIST_FOREACH(conf_list, l, conf) {
		_D("** Number: %d", i++);
		_D("** Name  : %s", conf->name);
		_D("** Path  : %s", conf->path);
		_D("** Value : %s", conf->value);
		_D("********************************");
	}
}
#else
#define show_supported_confs_list() do {} while(0)
#define show_operation_list() do {} while(0)
#define show_configuration_list() do {} while(0)
#endif

static xmlDocPtr xml_open(const char *xml)
{
	if (!xml)
		return NULL;

	return xmlReadFile(xml, NULL, 0);
}

static void xml_close(xmlDocPtr doc)
{
	xmlFreeDoc(doc);
}

static bool skip_node(xmlNodePtr node)
{
	if (!node)
		return true;

	if (!xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_TEXT))
		return true;
	if (!xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_COMMENT))
		return true;

	return false;
}

static int get_xml_property(xmlNodePtr node, char *property, char *value, int len)
{
	xmlChar *xmlprop;

	xmlprop = xmlGetProp(node, (const xmlChar *)property);
	if (!xmlprop)
		return -ENOMEM;

	snprintf(value, len, "%s", (char *)xmlprop);
	xmlFree(xmlprop);

	return 0;
}

static bool is_usb_mode_supported(int usb_mode)
{
	dd_list *l;
	struct xmlSupported *sp;

	DD_LIST_FOREACH(supported_list, l, sp) {
		if (sp->mode == usb_mode)
			return true;
	}

	return false;
}

static int get_iproduct_name(char *name, int size)
{
	if (!name)
		return -EINVAL;

	/* TODO: Product name should be set using device model
	 * ex) TIZEN_SM-Z9005 */

	snprintf(name, size, "%s", DEFAULT_IPRODUCT_NAME);
	return 0;
}

static int get_driver_version(xmlNodePtr root)
{
	int ret;
	xmlNodePtr node;
	FILE *fp;
	char buf[BUF_MAX];
	char path[BUF_MAX];

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_DRIVER))
			continue;

		ret = get_xml_property(node, ATTR_VALUE, path, sizeof(path));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		if (access(path, F_OK) != 0) {
			/* TODO: If the path does not exist,
			 * usb gadgets are not from Galaxy, but from Linux Kernel */
			driver = NULL;
			return 0;
		}

		fp = fopen(path, "r");
		if (!fp) {
			_E("fopen() failed(%s)", path);
			return -ENOMEM;
		}

		if (!fgets(buf, sizeof(buf), fp)) {
			_E("fgets() failed");
			fclose(fp);
			return -ENOMEM;
		}

		fclose(fp);

		driver = strdup(buf);
		break;
	}
	return 0;
}

/* Getting sysfs nodes to set usb configurations */
static int add_conf_nodes_to_list(xmlNodePtr root)
{
	int ret;
	xmlNodePtr node;
	struct xmlConfiguration *conf;
	char path[BUF_MAX];

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		ret = get_xml_property(node, ATTR_VALUE, path, sizeof(path));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		conf = (struct xmlConfiguration *)malloc(sizeof(struct xmlConfiguration));
		if (!conf) {
			_E("malloc() failed");
			return -ENOMEM;
		}

		conf->name = strdup((const char *)(node->name));
		conf->path = strdup(path);
		conf->value = NULL;

		if (!(conf->name) || !(conf->path)) {
			_E("strdup() failed");
			if (conf->name)
				free(conf->name);
			if (conf->path)
				free(conf->path);
			free(conf);
			continue;
		}

		DD_LIST_APPEND(conf_list, conf);
	}

	return 0;
}

static int get_configuration_nodes(xmlNodePtr root)
{
	int ret;
	xmlNodePtr node;
	char ver[BUF_MAX];

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_USB_DRV))
			continue;

		ret = get_xml_property(node, ATTR_VERSION, ver, sizeof(ver));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		if (strncmp(ver, driver, strlen(ver)))
			continue;

		return add_conf_nodes_to_list(node);
	}

	return -ENOMEM;
}

int make_empty_configuration_list(void)
{
	int ret;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;

	doc = xml_open(USB_CONFIGURATIONS_PATH);
	if (!doc) {
		_E("fail to open xml file (%s)", USB_CONFIGURATIONS_PATH);
		return -ENOMEM;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		_E("FAIL: xmlDocGetRootElement()");
		ret = -ENOMEM;
		goto out;
	}

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (!xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_CONFIG_NODES)) {
			ret = get_driver_version(node);
			if (ret < 0) {
				_E("Failed to get usb driver version");
				ret = -ENOMEM;
				goto out;
			}

			ret = get_configuration_nodes(node);
			if (ret < 0) {
				_E("Failed to get conf nodes");
				ret = -ENOMEM;
				goto out;
			}

			show_configuration_list();
			break;
		}
	}

	ret = 0;

out:
	xml_close(doc);
	return ret;
}

static int add_configurations_to_list(xmlNodePtr root)
{
	int ret;
	dd_list *l;
	struct xmlConfiguration *conf;
	xmlNodePtr node;
	char buf[BUF_MAX];
	char value[BUF_MAX];

	if (!root)
		return -EINVAL;

	DD_LIST_FOREACH(conf_list, l, conf) {
		if (conf->value) {
			free(conf->value);
			conf->value = NULL;
		}

		if (!strncmp(conf->name, NODE_NAME_IPRODUCT, strlen(NODE_NAME_IPRODUCT))) {
			ret = get_iproduct_name(buf, sizeof(buf));
			if (ret == 0)
				conf->value = strdup(buf);
			continue;
		}

		for (node = root->children ; node ; node = node->next) {
			if (skip_node(node))
				continue;

			if (xmlStrcmp((const xmlChar *)conf->name, node->name))
				continue;

			ret = get_xml_property(node, ATTR_VALUE, value, sizeof(value));
			if (ret < 0) {
				_E("Failed to get property(%d)", ret);
				return ret;
			}

			conf->value = strdup(value);
			break;
		}
	}

	return 0;
}

static int get_configurations_by_usb_mode(xmlNodePtr root, int mode)
{
	xmlNodePtr node;
	char cMode[BUF_MAX];
	int iMode, ret;

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_MODE))
			continue;

		ret = get_xml_property(node, ATTR_VALUE, cMode, sizeof(cMode));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		iMode = atoi(cMode);
		if (mode != iMode)
			continue;

		return add_configurations_to_list(node);
	}
	return -EINVAL;
}

static int get_configurations_by_version(xmlNodePtr root, int mode)
{
	xmlNodePtr node;
	char ver[BUF_MAX];
	int ret;

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_USB_DRV))
			continue;

		ret = get_xml_property(node, ATTR_VERSION, ver, sizeof(ver));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		if (strncmp(ver, driver, strlen(ver)))
			continue;

		return get_configurations_by_usb_mode(node, mode);
	}

	return -EINVAL;
}

static int get_configurations_info(xmlNodePtr root, int mode)
{
	xmlNodePtr node;

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_USB_CONFS))
			continue;

		return get_configurations_by_version(node, mode);
	}

	return -EINVAL;
}

int make_configuration_list(int usb_mode)
{
	int ret;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;

	if (!is_usb_mode_supported(usb_mode)) {
		_E("USB mode (%d) is not supported", usb_mode);
		return -EINVAL;
	}

	doc = xml_open(USB_CONFIGURATIONS_PATH);
	if (!doc) {
		_E("fail to open xml file (%s)", USB_CONFIGURATIONS_PATH);
		return -ENOMEM;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		_E("FAIL: xmlDocGetRootElement()");
		ret = -ENOMEM;
		goto out;
	}

	ret = get_configurations_info(root, usb_mode);
	if (ret < 0) {
		_E("Failed to get operations for usb mode(%d)", usb_mode);
		ret = -ENOMEM;
		goto out;
	}

	show_configuration_list();
	ret = 0;

out:
	xml_close(doc);
	return ret;
}


void release_configuration_list(void)
{
	dd_list *l;
	struct xmlConfiguration *conf;

	if (!conf_list)
		return;

	DD_LIST_FOREACH(conf_list, l, conf) {
		if (conf->name)
			free(conf->name);
		if (conf->path)
			free(conf->path);
		if (conf->value)
			free(conf->value);
		if (conf)
			free(conf);
	}

	DD_LIST_FREE_LIST(conf_list);
	conf_list = NULL;
}

/* Getting configurations supported */
static int get_supported_confs(xmlNodePtr root)
{
	int ret;
	xmlNodePtr node;
	char cMode[BUF_MAX];
	int iMode;
	struct xmlSupported *sp;

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_MODE))
			continue;

		ret = get_xml_property(node, ATTR_VALUE, cMode, sizeof(cMode));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		iMode = atoi(cMode);

		sp = (struct xmlSupported *)malloc(sizeof(struct xmlSupported));
		if (!sp) {
			_E("malloc() failed");
			return -ENOMEM;
		}

		sp->mode = iMode;

		DD_LIST_APPEND(supported_list, sp);
	}

	return 0;
}

int make_supported_confs_list(void)
{
	int ret;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;

	doc = xml_open(SUPPORTED_CONF_PATH);
	if (!doc) {
		_E("fail to open xml file (%s)", SUPPORTED_CONF_PATH);
		return -ENOMEM;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		_E("FAIL: xmlDocGetRootElement()");
		ret = -ENOMEM;
		goto out;
	}

	ret = get_supported_confs(root);
	if (ret < 0) {
		_E("Failed to get supported confs");
		ret = -ENOMEM;
		goto out;
	}

	show_supported_confs_list();
	ret = 0;

out:
	xml_close(doc);
	return ret;
}

void release_supported_confs_list(void)
{
	struct xmlSupported *sp;
	dd_list *l;

	if (!supported_list)
		return;

	DD_LIST_FOREACH(supported_list, l, sp) {
		free(sp);
	}

	DD_LIST_FREE_LIST(supported_list);
	supported_list = NULL;
}

/* Getting operations for each usb mode */
static int get_operations_info(xmlNodePtr node, struct xmlOperation **oper)
{
	int ret;
	char background[BUF_MAX];
	char operation[BUF_MAX];

	if (!node || !oper)
		return -EINVAL;

	ret = get_xml_property(node, ATTR_VALUE, operation, sizeof(operation));
	if (ret < 0) {
		_E("Failed to get property(%d)", ret);
		return ret;
	}

	*oper = (struct xmlOperation *)malloc(sizeof(struct xmlOperation));
	if (!(*oper)) {
		_E("malloc() failed");
		return -ENOMEM;
	}

	(*oper)->name = strdup((const char *)(node->name));
	(*oper)->oper = strdup(operation);

	if (!((*oper)->name) || !((*oper)->oper)) {
		if ((*oper)->name)
			free((*oper)->name);
		if ((*oper)->oper)
			free((*oper)->oper);
		if (*oper) {
			free(*oper);
			*oper = NULL;
		}
		return -ENOMEM;
	}

	ret = get_xml_property(node, ATTR_BACKGROUND, background, sizeof(background));
	if (ret < 0) {
		(*oper)->background = false;
		return 0;
	}

	if (strncmp(background, "true", strlen(background)))
		(*oper)->background = false;
	else
		(*oper)->background = true;

	return 0;
}

static int add_operations_to_list(xmlNodePtr root)
{
	int ret;
	xmlNodePtr node;
	struct xmlOperation *oper;

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		ret = get_operations_info(node, &oper);
		if (ret < 0 || !oper) {
			_E("Failed to get operations info");
			return -ENOMEM;
		}

		DD_LIST_APPEND(oper_list, oper);
	}

	return 0;
}

static int get_operations_by_usb_mode(xmlNodePtr root, int usb_mode)
{
	int ret;
	xmlNodePtr node;
	char cMode[BUF_MAX];
	int iMode;

	if (!root)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_MODE))
			continue;

		ret = get_xml_property(node, ATTR_VALUE, cMode, sizeof(cMode));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		iMode = atoi(cMode);

		if (usb_mode != iMode)
			continue;

		ret = add_operations_to_list(node);
		if (ret < 0) {
			_E("Failed to add operations to list ");
			return -ENOMEM;
		}
		break;
	}

	return 0;
}

static int get_operations_by_action(xmlNodePtr root, int usb_mode, char *action)
{
	int ret;
	xmlNodePtr node;
	char act[BUF_MAX];

	if (!root || !action)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_ACTION))
			continue;

		ret = get_xml_property(node, ATTR_VALUE, act, sizeof(act));
		if (ret < 0) {
			_E("Failed to get property(%d)", ret);
			return ret;
		}

		if (strncmp(act, action, strlen(act)))
			continue;

		ret = get_operations_by_usb_mode(node, usb_mode);
		if (ret < 0) {
			_E("Failed to get operations for usb_mode (%d)", usb_mode);
			return -ENOMEM;
		}
		break;
	}

	return 0;
}

static int get_usb_operations(xmlNodePtr root, int usb_mode, char *action)
{
	xmlNodePtr node;

	if (!root || !action)
		return -EINVAL;

	for (node = root->children ; node ; node = node->next) {
		if (skip_node(node))
			continue;

		if (xmlStrcmp(node->name, (const xmlChar *)NODE_NAME_USB_OPERS))
			continue;

		return get_operations_by_action(node, usb_mode, action);
	}

	return -ENOMEM;
}

int make_operation_list(int usb_mode, char *action)
{
	int ret;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;
	char conf_file_path[BUF_MAX];

	if (!action)
		return -EINVAL;

	if (!is_usb_mode_supported(usb_mode)) {
		_E("USB mode (%d) is not supported", usb_mode);
		return -EINVAL;
	}

	doc = xml_open(USB_CONFIGURATIONS_PATH);
	if (!doc) {
		_E("fail to open xml file (%s)", USB_CONFIGURATIONS_PATH);
		return -ENOMEM;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		_E("FAIL: xmlDocGetRootElement()");
		ret = -ENOMEM;
		goto out;
	}

	ret = get_usb_operations(root, usb_mode, action);
	if (ret < 0) {
		_E("Failed to get operations info(%d, %s)", usb_mode, action);
		ret = -ENOMEM;
		goto out;
	}

	show_operation_list();
	ret = 0;

out:
	xml_close(doc);
	return ret;
}

void release_operations_list(void)
{
	dd_list *l;
	struct xmlOperation *oper;

	if (!oper_list)
		return;

	DD_LIST_FOREACH(oper_list, l, oper) {
		if (oper->name)
			free(oper->name);
		if (oper->oper)
			free(oper->oper);
		if (oper)
			free(oper);
	}

	DD_LIST_FREE_LIST(oper_list);
	oper_list = NULL;
}
