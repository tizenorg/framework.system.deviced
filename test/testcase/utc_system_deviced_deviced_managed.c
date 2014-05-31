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
#include <stdbool.h>
#include <dd-deviced-managed.h>

#define API_NAME_DEVICED_GET_PID "deviced_get_pid"
#define API_NAME_DEVICED_SET_DATETIME "deviced_set_datetime"
#define API_NAME_DEVICED_REQUEST_MOUNT_MMC "deviced_request_mount_mmc"
#define API_NAME_DEVICED_REQUEST_UNMOUNT_MMC "deviced_request_unmount_mmc"
#define API_NAME_DEVICED_REQUEST_FORMAT_MMC "deviced_request_format_mmc"
#define API_NAME_DEVICED_FORMAT_MMC "deviced_format_mmc"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_deviced_get_pid_p(void);
static void utc_system_deviced_deviced_get_pid_n(void);
static void utc_system_deviced_deviced_set_datetime_n(void);
static void utc_system_deviced_deviced_set_datetime_p(void);
static void utc_system_deviced_deviced_request_mount_mmc_p_1(void);
static void utc_system_deviced_deviced_request_mount_mmc_p_2(void);
static void utc_system_deviced_deviced_request_mount_mmc_p_3(void);
static void utc_system_deviced_deviced_request_unmount_mmc_p_1(void);
static void utc_system_deviced_deviced_request_unmount_mmc_p_2(void);
static void utc_system_deviced_deviced_request_unmount_mmc_p_3(void);
static void utc_system_deviced_deviced_request_unmount_mmc_p_4(void);
static void utc_system_deviced_deviced_request_unmount_mmc_n(void);
static void utc_system_deviced_deviced_request_format_mmc_p_1(void);
static void utc_system_deviced_deviced_request_format_mmc_p_2(void);
static void utc_system_deviced_deviced_request_format_mmc_p_3(void);
static void utc_system_deviced_deviced_format_mmc_p_1(void);
static void utc_system_deviced_deviced_format_mmc_p_2(void);
static void utc_system_deviced_deviced_format_mmc_p_3(void);
static void utc_system_deviced_deviced_format_mmc_p_4(void);
static void utc_system_deviced_deviced_format_mmc_n(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	/* The following TCs are for root application */
//	{ utc_system_deviced_deviced_get_pid_p, POSITIVE_TC_IDX },
//	{ utc_system_deviced_deviced_get_pid_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_set_datetime_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_set_datetime_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_mount_mmc_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_mount_mmc_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_mount_mmc_p_3, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_unmount_mmc_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_unmount_mmc_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_unmount_mmc_p_3, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_unmount_mmc_p_4, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_unmount_mmc_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_format_mmc_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_format_mmc_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_format_mmc_p_3, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_format_mmc_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_format_mmc_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_format_mmc_p_3, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_format_mmc_p_4, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_format_mmc_n, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of deviced_get_pid()
 */
static void utc_system_deviced_deviced_get_pid_p(void)
{
	int ret;

	ret = deviced_get_pid("/usb/bin/deviced");
	dts_check_ge(API_NAME_DEVICED_GET_PID, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_pid()
 */
static void utc_system_deviced_deviced_get_pid_n(void)
{
	int ret;

	ret = deviced_get_pid(NULL);
	dts_check_lt(API_NAME_DEVICED_GET_PID, ret, 0);
}

/**
 * @brief Negative test case of deviced_set_datetime()
 */
static void utc_system_deviced_deviced_set_datetime_n(void)
{
	int ret;

	ret = deviced_set_datetime(-1);
	dts_check_lt(API_NAME_DEVICED_SET_DATETIME, ret, 0);
}

/**
 * @brief Positive test case of deviced_set_datetime()
 */
static void utc_system_deviced_deviced_set_datetime_p(void)
{
	int ret;
	time_t now;

	localtime(&now);

	ret = deviced_set_datetime(now);
	dts_check_ge(API_NAME_DEVICED_SET_DATETIME, ret, 0);
}

static void mount_cb(int result, void *data)
{
	dts_message(API_NAME_DEVICED_REQUEST_MOUNT_MMC,
			"mount callback result : %d", result);
}

/**
 * @brief Positive test case of deviced_request_mount_mmc()
 */
static void utc_system_deviced_deviced_request_mount_mmc_p_1(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = mount_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_MOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_mount_mmc(&data);
	dts_check_eq(API_NAME_DEVICED_REQUEST_MOUNT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_mount_mmc()
 */
static void utc_system_deviced_deviced_request_mount_mmc_p_2(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = NULL, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_MOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_mount_mmc(&data);
	dts_check_eq(API_NAME_DEVICED_REQUEST_MOUNT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_mount_mmc()
 */
static void utc_system_deviced_deviced_request_mount_mmc_p_3(void)
{
	int ret;

	dts_message(API_NAME_DEVICED_REQUEST_MOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_mount_mmc(NULL);
	dts_check_eq(API_NAME_DEVICED_REQUEST_MOUNT_MMC, ret, 0);

	sleep(1);
}

static void unmount_cb(int result, void *data)
{
	dts_message(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC,
			"unmount callback result : %d", result);
}

/**
 * @brief Positive test case of deviced_request_unmount_mmc()
 */
static void utc_system_deviced_deviced_request_unmount_mmc_p_1(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = unmount_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_unmount_mmc(&data, 0);
	dts_check_eq(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_unmount_mmc()
 */
static void utc_system_deviced_deviced_request_unmount_mmc_p_2(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = unmount_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_unmount_mmc(&data, 1);
	dts_check_eq(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_unmount_mmc()
 */
static void utc_system_deviced_deviced_request_unmount_mmc_p_3(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = NULL, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_unmount_mmc(&data, 1);
	dts_check_eq(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_unmount_mmc()
 */
static void utc_system_deviced_deviced_request_unmount_mmc_p_4(void)
{
	int ret;

	dts_message(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_unmount_mmc(NULL, 0);
	dts_check_eq(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Negative test case of deviced_request_unmount_mmc()
 */
static void utc_system_deviced_deviced_request_unmount_mmc_n(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = unmount_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_unmount_mmc(&data, -1);
	dts_check_ne(API_NAME_DEVICED_REQUEST_UNMOUNT_MMC, ret, 0);
}

static void format_cb(int result, void *data)
{
	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"format callback result : %d", result);
}

/**
 * @brief Positive test case of deviced_request_format_mmc()
 */
static void utc_system_deviced_deviced_request_format_mmc_p_1(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = format_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_format_mmc(&data);
	dts_check_eq(API_NAME_DEVICED_REQUEST_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_format_mmc()
 */
static void utc_system_deviced_deviced_request_format_mmc_p_2(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = NULL, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_format_mmc(&data);
	dts_check_eq(API_NAME_DEVICED_REQUEST_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_request_format_mmc()
 */
static void utc_system_deviced_deviced_request_format_mmc_p_3(void)
{
	int ret;

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_request_format_mmc(NULL);
	dts_check_eq(API_NAME_DEVICED_REQUEST_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_format_mmc()
 */
static void utc_system_deviced_deviced_format_mmc_p_1(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = format_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_format_mmc(&data, 0);
	dts_check_eq(API_NAME_DEVICED_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_format_mmc()
 */
static void utc_system_deviced_deviced_format_mmc_p_2(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = format_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_format_mmc(&data, 1);
	dts_check_eq(API_NAME_DEVICED_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_format_mmc()
 */
static void utc_system_deviced_deviced_format_mmc_p_3(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = NULL, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_format_mmc(&data, 1);
	dts_check_eq(API_NAME_DEVICED_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Positive test case of deviced_format_mmc()
 */
static void utc_system_deviced_deviced_format_mmc_p_4(void)
{
	int ret;

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_format_mmc(NULL, 0);
	dts_check_eq(API_NAME_DEVICED_FORMAT_MMC, ret, 0);

	sleep(1);
}

/**
 * @brief Negative test case of deviced_format_mmc()
 */
static void utc_system_deviced_deviced_format_mmc_n(void)
{
	int ret;
	struct mmc_contents data = {.mmc_cb = format_cb, .user_data = NULL};

	dts_message(API_NAME_DEVICED_REQUEST_FORMAT_MMC,
			"This testcase is only valid when mmc is inserted");

	ret = deviced_format_mmc(&data, -1);
	dts_check_ne(API_NAME_DEVICED_FORMAT_MMC, ret, 0);
}
