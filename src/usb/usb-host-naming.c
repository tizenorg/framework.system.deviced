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

#define _GNU_SOURCE
#include "usb-host.h"
#include <string.h>

#define VENDOR_HP_1         "Hewlett-Packard"
#define VENDOR_HP_2         "Hewlett Packard"
#define VENDOR_HP_3         "HP"
#define VENDOR_SAMSUNG      "Samsung"

static void remove_underbar(char *name)
{
	int i;
	char *temp = name;

	if (!temp)
		return;

	for (i = 0 ; i < strlen(name); temp++, i++) {
		if (*temp == '_')
			*temp = ' ';
	}
}

static void remove_non_alphabet(char *name)
{
	int i;
	char *pos, *temp;

	if (!name)
		return;

	pos = name + strlen(name) - 1;

	for (i = strlen(name); i > 0 ; i--, pos--) {
		if ((*pos >= 48 && *pos <= 57)          /* number */
				|| (*pos >= 65 && *pos <= 90)   /* uppercase letter */
				|| (*pos >= 97 && *pos <= 122)  /* lowercase letter */
				|| *pos == '-'                 /* hyphen  */
				|| *pos == ' ')                /* white space */
			continue;

		temp = pos;
		while (*temp != '\0') {
			*temp = *(temp + 1);
			temp++;
		}
	}
}

static void change_to_short(char *name, int len)
{
	if (strcasestr(name, VENDOR_HP_1)) {
		snprintf(name, len, "%s", VENDOR_HP_3);
		return;
	}

	if (strcasestr(name, VENDOR_HP_2)) {
		snprintf(name, len, "%s", VENDOR_HP_3);
		return;
	}

	if (strcasestr(name, VENDOR_HP_3)) {
		snprintf(name, len, "%s", VENDOR_HP_3);
		return;
	}

	if (strcasestr(name, VENDOR_SAMSUNG)) {
		snprintf(name, len, "%s", VENDOR_SAMSUNG);
		return;
	}
}

static void remove_vendor(char *model, char *vendor)
{
	char *pos, *temp;
	int i;

	pos = strcasestr(model, vendor);
	if (!pos)
		return;

	temp = pos + strlen(vendor);

	while(*pos != '\0') {
		*pos = *temp;
		pos++;
		temp++;
	}
}

int verify_vendor_name(const char *vendor, char *buf, int len)
{
	char name[BUF_MAX];

	if (!vendor || !buf || len <= 0)
		return -EINVAL;

	snprintf(name, sizeof(name), "%s", vendor);

	remove_underbar(name);

	remove_non_alphabet(name);

	change_to_short(name, sizeof(name));

	snprintf(buf, len, "%s", name);
	return 0;
}

int verify_model_name(const char *model, char *vendor, char *buf, int len)
{
	char name[BUF_MAX];

	if (!model || !vendor || !buf || len <= 0)
		return -EINVAL;

	snprintf(name, sizeof(name), "%s", model);

	remove_underbar(name);

	remove_non_alphabet(name);

	change_to_short(name, sizeof(name));

	remove_vendor(name, vendor);

	snprintf(buf, len, "%s", name);
	return 0;
}
