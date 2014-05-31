/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#ifndef __TICKER_H__
#define __TICKER_H__


#include "core/common.h"
#include "core/data.h"

enum ticker_type {
	WITHOUT_QUEUE, /* for usb client */
	WITH_QUEUE,    /* for other situations */
};

struct ticker_data {
	char *name;
	int type;
};

#endif /* __TICKER_H__ */

