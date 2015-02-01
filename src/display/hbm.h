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
 */


/**
 * @file	hbm.h
 * @brief	High Brightness Mode header file
 */
#ifndef __HBM_H__
#define __HBM_H__

/*
 * @brief Configuration structure
 */
struct hbm_config {
	int on;
	int off;
	int on_count;
	int off_count;
};

/*
 * Global variables
 *   hbm_conf : configuration of hbm
 */
extern struct hbm_config hbm_conf;

/**
 * @}
 */

#endif
