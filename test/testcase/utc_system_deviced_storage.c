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
#include <dd-storage.h>

#define API_NAME_STORAGE_GET_PATH "storage_get_path"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_storage_get_path_p_1(void);
static void utc_system_deviced_storage_get_path_p_2(void);
static void utc_system_deviced_storage_get_path_p_3(void);
static void utc_system_deviced_storage_get_path_n_1(void);
static void utc_system_deviced_storage_get_path_n_2(void);
static void utc_system_deviced_storage_get_path_n_3(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_storage_get_path_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_storage_get_path_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_storage_get_path_p_3, POSITIVE_TC_IDX },
	{ utc_system_deviced_storage_get_path_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_storage_get_path_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_storage_get_path_n_3, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};


static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of storage_get_path()
 */
static void utc_system_deviced_storage_get_path_p_1(void)
{
	char path[40];
	int ret;

	ret = storage_get_path(STORAGE_DEFAULT, path, sizeof(path));
	dts_check_eq(API_NAME_STORAGE_GET_PATH, ret, 0, "Default storage");
}

/**
 * @brief Positive test case of storage_get_path()
 */
static void utc_system_deviced_storage_get_path_p_2(void)
{
	char path[40];
	int ret;

	ret = storage_get_path(STORAGE_INTERNAL, path, sizeof(path));
	dts_check_eq(API_NAME_STORAGE_GET_PATH, ret, 0, "Internal storage");
}

/**
 * @brief Positive test case of storage_get_path()
 */
static void utc_system_deviced_storage_get_path_p_3(void)
{
	char path[40];
	int ret;

	ret = storage_get_path(STORAGE_EXTERNAL, path, sizeof(path));
	dts_check_eq(API_NAME_STORAGE_GET_PATH, ret, 0, "External storage");
}

/**
 * @brief Negative test case of storage_get_path()
 */
static void utc_system_deviced_storage_get_path_n_1(void)
{
	char path[40];
	int ret;

	ret = storage_get_path(-1, path, sizeof(path));
	dts_check_ne(API_NAME_STORAGE_GET_PATH, ret, 0);
}

/**
 * @brief Negative test case of storage_get_path()
 */
static void utc_system_deviced_storage_get_path_n_2(void)
{
	char path[40];
	int ret;

	ret = storage_get_path(STORAGE_DEFAULT, NULL, sizeof(path));
	dts_check_ne(API_NAME_STORAGE_GET_PATH, ret, 0);
}

/**
 * @brief Negative test case of storage_get_path()
 */
static void utc_system_deviced_storage_get_path_n_3(void)
{
	char path[40];
	int ret;

	ret = storage_get_path(STORAGE_DEFAULT, path, -1);
	dts_check_ne(API_NAME_STORAGE_GET_PATH, ret, 0);
}
