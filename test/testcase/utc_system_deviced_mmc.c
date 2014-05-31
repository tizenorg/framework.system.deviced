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
#include <dd-mmc.h>

#define API_NAME_MMC_SECURE_MOUNT "mmc_secure_mount"
#define API_NAME_MMC_SECURE_UNMOUNT "mmc_secure_unmount"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_mmc_secure_mount_p(void);
static void utc_system_deviced_mmc_secure_mount_n(void);
static void utc_system_deviced_mmc_secure_unmount_p(void);
static void utc_system_deviced_mmc_secure_unmount_n(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_mmc_secure_mount_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_mmc_secure_mount_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_mmc_secure_unmount_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_mmc_secure_unmount_n, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

#define SECURE_MOUNT_POINT "/opt/storage/secure"

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of mmc_secure_mount()
 */
static void utc_system_deviced_mmc_secure_mount_p(void)
{
	int ret;

	dts_message("This testcase is only valid when mmc is inserted");

	ret = mmc_secure_mount(SECURE_MOUNT_POINT);
	dts_check_eq(API_NAME_MMC_SECURE_MOUNT, ret, 0);
}

/**
 * @brief Negative test case of mmc_secure_mount()
 */
static void utc_system_deviced_mmc_secure_mount_n(void)
{
	int ret;

	dts_message("This testcase is only valid when mmc is inserted");

	ret = mmc_secure_mount(NULL);
	dts_check_ne(API_NAME_MMC_SECURE_MOUNT, ret, 0);
}

/**
 * @brief Positive test case of mmc_secure_unmount()
 */
static void utc_system_deviced_mmc_secure_unmount_p(void)
{
	int ret;

	ret = mmc_secure_unmount(SECURE_MOUNT_POINT);
	dts_check_eq(API_NAME_MMC_SECURE_UNMOUNT, ret, 0);
}

/**
 * @brief Negative test case of mmc_secure_unmount()
 */
static void utc_system_deviced_mmc_secure_unmount_n(void)
{
	int ret;

	ret = mmc_secure_unmount(NULL);
	dts_check_ne(API_NAME_MMC_SECURE_UNMOUNT, ret, 0);
}
