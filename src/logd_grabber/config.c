/*
 *  deviced
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
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

#include <Eina.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "core/log.h"

static Eina_Hash *vars;
static const char *path = "/etc/deviced/logd-grabber.conf";

static int is_correct(char ch)
{
	if (ch >= '0' && ch <= '9')
		return 1;
	if (ch >='a' && ch <= 'z')
		return 1;
	if (ch >='A' && ch <= 'Z')
		return 1;
	if (ch == '_' || ch == '.')
		return 1;

	return 0;
}

static int split(char *str, char **key, char **value)
{
	char *ptr = NULL;
	char *_key;
	char *_value;

	_key = NULL;
	_value = strchr(str, '=');
	if (!_value) {
		_E("Wrong variable declaration: \"%s\". "
			"Must be like \"key = value\"\n", str);
		return -EINVAL;
	}
	ptr = str;
	while (*str == ' ') ++str;
	while (ptr < _value) {
		if (!is_correct(*ptr)) {
			if (*ptr == ' ') {
				_key = ptr;
				while (*ptr == ' ' && ptr != _value)
					++ptr;
				if (ptr != _value) {
					_E("Expected '=' before '%c' at %d\n",
						*ptr, ptr - str);
				}
			} else if (ptr != _value) {
				_E("wrong char '%c' at %d\n", *ptr, ptr - str);
				return -EINVAL;
			}
		}
		++ptr;
	}
	if (!_key)
		_key = _value;
	ptr = _key;
	*key = (char*) calloc(1, ptr - str + 1);
	strncpy(*key, str, ptr - str);

	ptr = _value + 1;
	while (*ptr == ' ') ++ptr;
	if (*ptr == 0) {
		_E("expected char symbol at %d", ptr - str);
		free(*key);
		return -EINVAL;
	}

	_value = ptr;

	while (*ptr != 0) {
		if (!is_correct(*ptr)) {
			_E("wrong char '%c' at %d\n", *ptr, ptr - str);
			free(*key);
			return -EINVAL;
		}
		++ptr;
	}

	ptr = _value;
	*value = (char*) calloc(1, strlen(str) - (ptr - str) + 1);
	strncpy(*value, str + (ptr - str) , strlen(str) - (ptr - str));

	return 0;
}

int config_init(void)
{
	FILE *fp = NULL;
	char *key;
	char *value;
	char buf[100];

	vars = eina_hash_string_superfast_new(free);

	fp = fopen(path, "r");
	if (!fp) {
		perror("fopen: ");
		return -errno;
	}
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = 0;
		if (split(buf, &key, &value) < 0) {
			_E("split failed");
			if (fclose(fp) < 0) {
				_E("fclose failed: %s", strerror(errno));
			}
			return -EINVAL;
		}
		if (eina_hash_add(vars, key, value) != EINA_FALSE) {
			_I("%s = %s added\n", key, value);
		}
	}
	if (fclose(fp) < 0) {
		int ret = -errno;
		_E("fclose failed: %s", strerror(errno));
		return ret;
	}

	return 0;
}

const char *config_get_string(const char *key, const char *def_value, int *error_code)
{
	const char *value = eina_hash_find(vars, key);

	if (error_code) {
		if (!value) {
			_E("key %d not exist", key);
			*error_code = -1;
			value = def_value;
		} else {
			*error_code = 0;
		}
	}

	return value;
}

int config_get_int(const char *key, int def_value, int *error_code)
{
	const char *value = config_get_string(key, NULL, error_code);
	int converted_value;

	if (!value)
		return def_value;
	converted_value = atoi(value);

	return converted_value;
}

float config_get_float(const char *key, float def_value, int *error_code)
{
	const char *value = config_get_string(key, NULL, error_code);
	float converted_value;

	if (!value)
		return def_value;
	converted_value = atof(value);

	return converted_value;
}

double config_get_double(const char *key, double def_value, int *error_code)
{
	const char *value = config_get_string(key, NULL, error_code);
	double converted_value;

	if (!value)
		return def_value;
	converted_value = atof(value);

	return converted_value;
}

int config_exit(void)
{
	eina_hash_free(vars);

	return 0;
}
