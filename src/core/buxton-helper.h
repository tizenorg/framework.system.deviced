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
 * @file buxton-helper.h
 *
 * @desc Helper function for simplification of using and
 * for looking like vconf interface
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 */

#ifndef __DEVICED_BUXTON_HELPER_H
#define __DEVICED_BUXTON_HELPER_H

#include <buxton.h>

#define BUXTON_DEVICED_LAYER "base"
#define BUXTON_DEVICED_SYSTEM_GROUP "deviced_system"
#define BUXTON_DEVICED_APPS_GROUP "deviced_apps"

/* This enum duplicates BuxtonDataType */
typedef enum conf_data_type {
	CONF_TYPE_MIN,
	CONF_STRING, /**<Represents type of a string value */
	CONF_INT32, /**<Represents type of an int32_t value */
	CONF_UINT32, /**<Represents type of an uint32_t value */
	CONF_INT64, /**<Represents type of a int64_t value */
	CONF_UINT64, /**<Represents type of a uint64_t value */
	CONF_FLOAT, /**<Represents type of a float value */
	CONF_DOUBLE, /**<Represents type of a double value */
	CONF_BOOLEAN, /**<Represents type of a boolean value */
	CONF_TYPE_MAX
} conf_data_type;

struct buxton_variant {
	union {
		char *string_value;
		int32_t int32_value;
		uint32_t uint32_value;
		int64_t int64_value;
		uint64_t uint64_value;
		float float_value;
		double double_value;
		bool bool_value;
	};
	conf_data_type type;
};

int buxton_get_var(char *layer_name, char *group_name, char *key_name,
	struct buxton_variant *value);

int buxton_set_var(char *layer_name, char *group_name, char *key_name,
	struct buxton_variant *value);

#endif /* __DEVICED_BUXTON_HELPER_H */

