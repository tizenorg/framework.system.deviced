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


#ifndef __DEVICES_H__
#define __DEVICES_H__

#include <errno.h>
#include "common.h"
#include "list.h"

enum device_priority {
	DEVICE_PRIORITY_NORMAL = 0,
	DEVICE_PRIORITY_HIGH,
};

enum device_flags {
	NORMAL_MODE                   = 0x00000001,
	AMBIENT_MODE                  = 0x00000002,
	CORE_LOGIC_MODE               = 0x00010000,
	TOUCH_SCREEN_OFF_MODE         = 0x00020000,
	LCD_PANEL_OFF_MODE            = 0x00040000,
	LCD_PHASED_TRANSIT_MODE       = 0x00080000,
	LCD_ON_BY_GESTURE             = 0x00100000,
	LCD_ON_BY_POWER_KEY           = 0x00200000,
	LCD_ON_BY_EVENT               = 0x00400000,
	LCD_ON_BY_TOUCH               = 0x00800000,
	LCD_OFF_BY_POWER_KEY          = 0x01000000,
	LCD_OFF_BY_TIMEOUT            = 0x02000000,
	LCD_OFF_BY_EVENT              = 0x04000000,
	LCD_OFF_LATE_MODE             = 0x08000000,
};

struct device_ops {
	enum device_priority priority;
	char *name;
	int (*probe) (void *data);
	void (*init) (void *data);
	void (*exit) (void *data);
	int (*start) (enum device_flags flags);
	int (*stop) (enum device_flags flags);
	int (*status) (void);
	int (*execute) (void *data);
	int (*dump) (FILE *fp, int mode, void *dump_data);
	void *dump_data;
};

enum device_ops_status {
	DEVICE_OPS_STATUS_UNINIT,
	DEVICE_OPS_STATUS_START,
	DEVICE_OPS_STATUS_STOP,
	DEVICE_OPS_STATUS_MAX,
};

void devices_init(void *data);
void devices_exit(void *data);

static inline int device_start(const struct device_ops *dev)
{
	if (dev && dev->start)
		return dev->start(NORMAL_MODE);

	return -EINVAL;
}

static inline int device_stop(const struct device_ops *dev)
{
	if (dev && dev->stop)
		return dev->stop(NORMAL_MODE);

	return -EINVAL;
}

static inline int device_exit(const struct device_ops *dev, void *data)
{
	if (dev && dev->exit) {
		dev->exit(data);
		return 0;
	}

	return -EINVAL;
}

static inline int device_execute(const struct device_ops *dev, void *data)
{
	if (dev && dev->execute)
		return dev->execute(data);

	return -EINVAL;
}

static inline int device_get_status(const struct device_ops *dev)
{
	if (dev && dev->status)
		return dev->status();

	return -EINVAL;
}

#define DEVICE_OPS_REGISTER(dev)	\
static void __CONSTRUCTOR__ module_init(void)	\
{	\
	add_device(dev);	\
}	\
static void __DESTRUCTOR__ module_exit(void)	\
{	\
	remove_device(dev);	\
}

dd_list *get_device_list_head(void);
void add_device(const struct device_ops *dev);
void remove_device(const struct device_ops *dev);

const struct device_ops *find_device(const char *name);
int check_default(const struct device_ops *dev);

#define NOT_SUPPORT_OPS(dev) \
	((check_default(dev))? 1 : 0)

#define FIND_DEVICE(dev, name) do { \
	if (!dev) dev = find_device(name); \
} while(0)

#define FIND_DEVICE_INT(dev, name) do { \
	if (!dev) dev = find_device(name); if(check_default(dev)) return -ENODEV; \
} while(0)

#define FIND_DEVICE_VOID(dev, name) do { \
	if (!dev) dev = find_device(name); if(check_default(dev)) return; \
} while(0)

#define GET_DEVICE_STATUS(name, defaults, ret) do { \
	const struct device_ops *dev; \
	dev = find_device(name); \
	ret = dev->status ? dev->status() : defaults; \
} while(0)

#endif
