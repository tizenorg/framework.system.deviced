/*
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG
 * ELECTRONICS ("Confidential Information"). You agree and acknowledge that
 * this software is owned by Samsung and you shall not disclose such
 * Confidential Information and shall use it only in accordance with the terms
 * of the license agreement you entered into with SAMSUNG ELECTRONICS. SAMSUNG
 * make no representations or warranties about the suitability of the software,
 * either express or implied, including but not limited to the implied
 * warranties of merchantability, fitness for a particular purpose, or
 * non-infringement. SAMSUNG shall not be liable for any damages suffered by
 * licensee arising out of or related to this software.
 *
 */
#include <tet_api.h>
#include <dd-battery.h>

#define API_NAME_BATTERY_GET_PERCENT "battery_get_percent"
#define API_NAME_BATTERY_GET_PERCENT_RAW "battery_get_percent_raw"
#define API_NAME_BATTERY_IS_FULL "battery_is_full"
#define API_NAME_BATTERY_GET_HEALTH "battery_get_health"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_battery_get_percent_p(void);
static void utc_system_deviced_battery_get_percent_raw_p(void);
static void utc_system_deviced_battery_is_full_p(void);
static void utc_system_deviced_battery_get_health_p(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_battery_get_percent_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_battery_get_percent_raw_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_battery_is_full_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_battery_get_health_p, POSITIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of battery_get_percent()
 */
static void utc_system_deviced_battery_get_percent_p(void)
{
	int ret;

	ret = battery_get_percent();
	dts_check_ge(API_NAME_BATTERY_GET_PERCENT, ret, 0);
}

/**
 * @brief Positive test case of battery_get_percent_raw()
 */
static void utc_system_deviced_battery_get_percent_raw_p(void)
{
	int ret;

	ret = battery_get_percent_raw();
	dts_check_ge(API_NAME_BATTERY_GET_PERCENT_RAW, ret, 0);
}

/**
 * @brief Positive test case of battery_is_full()
 */
static void utc_system_deviced_battery_is_full_p(void)
{
	int ret;

	ret = battery_is_full();
	dts_check_ge(API_NAME_BATTERY_IS_FULL, ret, 0);
}

/**
 * @brief Positive test case of battery_get_health()
 */
static void utc_system_deviced_battery_get_health_p(void)
{
	int ret;

	ret = battery_get_health();
	dts_check_ge(API_NAME_BATTERY_GET_HEALTH, ret, 0);
}
