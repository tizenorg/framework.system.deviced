/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/*
 * @file buxton-helper.c
 *
 * @desc Helper function for simplification of using and
 * for looking like vconf interface
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 */

#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "buxton-helper.h"
#include "log.h"
#include "common.h"

#define BUXTON_ATTEMPT_NUMBER 5

static BuxtonClient client;

struct callback_context {
	struct buxton_variant *b_variant;
	int error_code;
};

void general_get_callback(BuxtonResponse response, void *data)
{
	BuxtonKey key;
	struct buxton_variant *b_variant;
	struct callback_context *context;
	void *p;
	uint32_t ret;
	BuxtonDataType key_type;

	ret_msg_if(data == NULL,
		"Invalid pointer argument!");
	context = (struct callback_context *)data;
	ret_msg_if(context->b_variant == NULL,
		"Invalid b_variant argument!");

	b_variant = context->b_variant;
	context->error_code = -1;

	ret = buxton_response_status(response);
	ret_msg_if(ret, "Invalid buxton response");

	p = buxton_response_value(response);

	ret_msg_if(p == NULL, "Bad buxton response: %s", strerror(errno));

	key = buxton_response_key(response);
	if (!key) {
		free(p);
		return;
	}

	key_type = buxton_key_get_type(key);
	if(key_type != b_variant->type) {
		_E("Not proper type");
		goto out;
	}

	switch (key_type) {
	case STRING:
		b_variant->string_value = (char *)p;
		break;
	case INT32:
		b_variant->int32_value = *(int32_t *)p;
		break;
	case UINT32:
		b_variant->uint32_value = *(uint32_t *)p;
		break;
	case INT64:
		b_variant->int64_value = *(int64_t *)p;
		break;
	case UINT64:
		b_variant->uint64_value = *(uint64_t *)p;
		break;
	case FLOAT:
		b_variant->float_value = *(float *)p;
		break;
	case DOUBLE:
		b_variant->double_value = *(double *)p;
		break;
	case BOOLEAN:
		b_variant->bool_value = *(bool *)p;
		break;
	default:
		break;
	}

	context->error_code = 0;
out:
	/* we need to free only in case of data types which was copied by
	 * _value_, string's pointer will be used by client */
	if (key_type != STRING)
		free(p);
	free(key);
}

static int init_buxton_client(void)
{
	int ret;

	if(client != NULL)
		return 0;

	ret = buxton_open(&client);
	ret_value_msg_if(ret <= 0, -1, "couldn't connect to buxton server");

	return 0;
}

void finilize_buxton_client(void)
{
	/* buxton_close is robust to null pointer */
	buxton_close(client);
}

int buxton_get_var(char *layer_name, char *group_name,
	 char *key_name, struct buxton_variant *value)
{
	BuxtonKey key;
	int ret;
	int attempts = 0;

	struct callback_context context = {
		.b_variant = value,
		.error_code = 0,
	};

	ret_value_msg_if(value == NULL, -1,
		"Please provide valid pointer!");

get_init:
	ret = init_buxton_client();
	++attempts;
	ret_value_if(ret, ret);

	key = buxton_key_create(group_name, key_name,
		layer_name, value->type);

	ret = buxton_get_value(client, key, general_get_callback, &context,
		true);
	buxton_key_free(key);
	/* don't return buxton error code, need to return internal error
	 * code */
	if (errno == EPIPE && attempts < BUXTON_ATTEMPT_NUMBER) {
		buxton_close(client);
		client = NULL;
		goto get_init;
	}
	return ret || context.error_code ? -1 : 0;
}

void general_set_callback(BuxtonResponse response, void *data)
{
	BuxtonKey key;
	char *name;
	int *ret;
	int resp_ret;

	ret_msg_if(data == NULL, "Please provide *data for return value!");
	ret = data;
	resp_ret = buxton_response_status(response);

	if (resp_ret != 0) {
		_E("Failed to set value\n");
		*ret = resp_ret;
		return;
	}

	key = buxton_response_key(response);
	name = buxton_key_get_name(key);
	_I("Set value for key %s\n", name);
	buxton_key_free(key);
	free(name);
	*ret = 0;
}

/* for avoid ptr unligtment */
static void *get_variant_align(struct buxton_variant *b_variant)
{
	switch (b_variant->type) {
	case CONF_STRING:
		return b_variant->string_value;
	case CONF_INT32:
		return &b_variant->int32_value;
	case CONF_UINT32:
		return &b_variant->uint32_value;
	case CONF_INT64:
		return &b_variant->int64_value;
	case CONF_UINT64:
		return &b_variant->uint64_value;
	case CONF_FLOAT:
		return &b_variant->float_value;
	case CONF_DOUBLE:
		return &b_variant->double_value;
	case CONF_BOOLEAN:
		return &b_variant->bool_value;
	}
	return NULL;
}

int buxton_set_var(char *layer_name, char *group_name,
	char *key_name, struct buxton_variant *value)
{
	int ret_data = -1;
        BuxtonKey key;
	int ret;
	int attempts = 0;
	ret_value_msg_if(value == NULL, -1, "Please provide valid pointer");

set_init:
	ret = init_buxton_client();
	++attempts;
	ret_value_if(ret, ret);

	key = buxton_key_create(group_name, key_name,
		layer_name, value->type);

        ret = buxton_set_value(client, key, get_variant_align(value),
		general_set_callback, &ret_data, true);
	buxton_key_free(key);

	if (errno == EPIPE && attempts < BUXTON_ATTEMPT_NUMBER) {
		buxton_close(client);
		client = NULL;
		goto set_init;
	}

	return ret || ret_data ? -1 : 0;
}

