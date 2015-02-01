/*
 * deviced
 *
 * Copyright (c) 2012 - 2014 Samsung Electronics Co., Ltd.
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
#include "core/log.h"
#include "core/list.h"
#include "usb-client.h"
#include "core/config-parser.h"

#define BUF_MAX 128

#define USB_CLIENT_CONFIGURATION    "/etc/deviced/usb-client-configuration.conf"
#define USB_CLIENT_OPERATION        "/etc/deviced/usb-client-operation.conf"

#define SECTION_ROOT_PATH  "RootPath"
#define NAME_PATH          "path"

struct operation_data {
	char *action;
	char *mode;
};

struct usb_client_mode {
	int mode;
	char *name;
};

struct usb_client_mode usb_mode_match[] = {
	{ SET_USB_NONE             ,    "None"      },
	{ SET_USB_DEFAULT          ,    "Default"   },
	{ SET_USB_SDB              ,    "Sdb"       },
	{ SET_USB_SDB_DIAG         ,    "SdbDiag"   },
	{ SET_USB_RNDIS            ,    "Rndis"     },
	{ SET_USB_RNDIS_TETHERING  ,    "Rndis"     },
	{ SET_USB_RNDIS_DIAG       ,    "RndisDiag" },
	{ SET_USB_RNDIS_SDB        ,    "RndisSdb"  },
	{ SET_USB_DIAG_SDB         ,    "DiagSdb"   },
	{ SET_USB_DIAG_RMNET       ,    "DiagRmnet" },
};

static dd_list *oper_list;       /* Operations for USB mode */
static dd_list *conf_list;       /* Configurations for usb mode */
static char root_path[BUF_MAX] = {0,};


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

int get_root_path(char **path)
{
	if (strlen(root_path) <= 0)
		return -EINVAL;

	*path = root_path;
	return 0;
}

static int find_usb_mode_str(int usb_mode, char **name)
{
	int i;

	if (!name)
		return -EINVAL;

	for (i = 0 ; i < ARRAY_SIZE(usb_mode_match) ; i++) {
		if (usb_mode == usb_mode_match[i].mode) {
			*name = usb_mode_match[i].name;
			return 0;
		}
	}

	_E("Failed to find usb mode (%d)", usb_mode);
	return -EINVAL;
}

static int load_configuration_config(struct parse_result *result, void *user_data)
{
	char *name = user_data;
	int i;
	size_t res_sec_len, res_name_len, name_len;
	size_t root_path_len, path_len;
	struct usb_configuration *conf;

	if (!result)
		return -EINVAL;

	res_sec_len = strlen(result->section);

	root_path_len = strlen(SECTION_ROOT_PATH);
	if (res_sec_len == root_path_len
			&& !strncmp(result->section, SECTION_ROOT_PATH, res_sec_len)) {

		res_name_len = strlen(result->name);
		path_len = strlen(NAME_PATH);
		if (res_name_len == path_len
				&& !strncmp(result->name, NAME_PATH, res_name_len))
			snprintf(root_path, sizeof(root_path), "%s", result->value);

		return 0;
	}

	name_len = strlen(name);
	if (res_sec_len != name_len
			|| strncmp(result->section, name, res_sec_len))
		return 0;

	conf = (struct usb_configuration *)malloc(sizeof(struct usb_configuration));
	if (!conf) {
		_E("malloc() failed");
		return -ENOMEM;
	}

	snprintf(conf->name, sizeof(conf->name), "%s", result->name);
	snprintf(conf->value, sizeof(conf->value), "%s", result->value);

	DD_LIST_APPEND(conf_list, conf);
	_I("USB configuration: name(%s), value(%s)", conf->name, conf->value);

	return 0;
}

int make_configuration_list(int usb_mode)
{
	int i, ret = 0;
	char *name = NULL;

	ret = find_usb_mode_str(usb_mode, &name);
	if (ret < 0) {
		_E("Failed to get usb mode string (%d)", ret);
		return ret;
	}

	ret = config_parse(USB_CLIENT_CONFIGURATION, load_configuration_config, name);
	if (ret < 0)
		_E("Failed to load usb configurations");

	return ret;
}

void release_configuration_list(void)
{
	dd_list *l;
	struct usb_configuration *conf;

	if (!conf_list)
		return;

	DD_LIST_FOREACH(conf_list, l, conf) {
		if (conf)
			free(conf);
	}

	DD_LIST_FREE_LIST(conf_list);
	conf_list = NULL;
}

/* Getting operations for each usb mode */
static int load_operation_config(struct parse_result *result, void *user_data)
{
	struct operation_data *op_data = user_data;
	int i;
	size_t res_sec_len, res_name_len, mode_len, action_len;
	struct usb_operation *oper;

	if (!result)
		return -EINVAL;

	res_sec_len = strlen(result->section);
	mode_len = strlen(op_data->mode);
	if (res_sec_len != mode_len
			|| strncmp(result->section, op_data->mode, res_sec_len))
		return 0;

	res_name_len = strlen(result->name);
	action_len = strlen(op_data->action);
	if (res_name_len != action_len
			|| strncmp(result->name, op_data->action, res_name_len))
		return 0;

	oper = (struct usb_operation *)malloc(sizeof(struct usb_operation));
	if (!oper) {
		_E("malloc() failed");
		return -ENOMEM;
	}

	snprintf(oper->name, sizeof(oper->name), "%s", result->name);
	snprintf(oper->oper, sizeof(oper->oper), "%s", result->value);

	DD_LIST_APPEND(oper_list, oper);
	_I("USB operation: name(%s), value(%s)", oper->name, oper->oper);

	return 0;
}

int make_operation_list(int usb_mode, char *action)
{
	int ret = 0;
	char *mode;
	struct operation_data op_data;

	if (!action)
		return -EINVAL;

	ret = find_usb_mode_str(usb_mode, &mode);
	if (ret < 0) {
		_E("Failed to get usb mode string (%d)", ret);
		return ret;
	}

	op_data.action = action;
	op_data.mode = mode;

	ret = config_parse(USB_CLIENT_OPERATION, load_operation_config, &op_data);
	if (ret < 0)
		_E("Failed to load usb configurations");

	return ret;
}

void release_operations_list(void)
{
	dd_list *l;
	struct usb_operation *oper;

	if (!oper_list)
		return;

	DD_LIST_FOREACH(oper_list, l, oper) {
		if (oper)
			free(oper);
	}

	DD_LIST_FREE_LIST(oper_list);
	oper_list = NULL;
}
