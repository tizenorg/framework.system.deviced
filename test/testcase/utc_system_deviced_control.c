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
#include <dd-control.h>

#define API_NAME_DEVICED_MMC_CONTROL "deviced_mmc_control"
#define API_NAME_DEVICED_USB_CONTROL "deviced_usb_control"
#define API_NAME_DEVICED_GET_USB_CONTROL "deviced_get_usb_control"
#define API_NAME_DEVICED_RGBLED_CONTROL "deviced_rgbled_control"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_deviced_mmc_control_p_1(void);
static void utc_system_deviced_deviced_mmc_control_p_2(void);
static void utc_system_deviced_deviced_usb_control_p_1(void);
static void utc_system_deviced_deviced_usb_control_p_2(void);
static void utc_system_deviced_deviced_get_usb_control_p(void);
static void utc_system_deviced_deviced_rgbled_control_p_1(void);
static void utc_system_deviced_deviced_rgbled_control_p_2(void);
static void utc_system_deviced_deviced_rgbled_control_n(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_deviced_mmc_control_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_mmc_control_p_2, POSITIVE_TC_IDX },
//	{ utc_system_deviced_deviced_usb_control_p_1, POSITIVE_TC_IDX },
//	{ utc_system_deviced_deviced_usb_control_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_usb_control_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_rgbled_control_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_rgbled_control_p_2, POSITIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of deviced_mmc_control()
 */
static void utc_system_deviced_deviced_mmc_control_p_1(void)
{
	int ret;

	ret = deviced_mmc_control(false);
	dts_check_eq(API_NAME_DEVICED_MMC_CONTROL, ret, 0, "Disable MMC");
}

/**
 * @brief Positive test case of deviced_mmc_control()
 */
static void utc_system_deviced_deviced_mmc_control_p_2(void)
{
	int ret;

	ret = deviced_mmc_control(true);
	dts_check_eq(API_NAME_DEVICED_MMC_CONTROL, ret, 0, "Enable MMC");
}

/**
 * @brief Positive test case of deviced_usb_control()
 */
static void utc_system_deviced_deviced_usb_control_p_1(void)
{
	int ret;

	ret = deviced_usb_control(false);
	dts_check_eq(API_NAME_DEVICED_USB_CONTROL, ret, 0, "Disable USB");
}

/**
 * @brief Positive test case of deviced_usb_control()
 */
static void utc_system_deviced_deviced_usb_control_p_2(void)
{
	int ret;

	ret = deviced_usb_control(true);
	dts_check_eq(API_NAME_DEVICED_USB_CONTROL, ret, 0, "Enable USB");
}

/**
 * @brief Positive test case of deviced_get_usb_control()
 */
static void utc_system_deviced_deviced_get_usb_control_p(void)
{
	int ret;

	ret = deviced_get_usb_control();
	dts_check_ge(API_NAME_DEVICED_GET_USB_CONTROL, ret, 0);
}

/**
 * @brief Positive test case of deviced_rgbled_control()
 */
static void utc_system_deviced_deviced_rgbled_control_p_1(void)
{
	int ret;

	ret = deviced_rgbled_control(false);
	dts_check_eq(API_NAME_DEVICED_RGBLED_CONTROL, ret, 0, "Disable RGB led");
}

/**
 * @brief Positive test case of deviced_rgbled_control()
 */
static void utc_system_deviced_deviced_rgbled_control_p_2(void)
{
	int ret;

	ret = deviced_rgbled_control(true);
	dts_check_eq(API_NAME_DEVICED_USB_CONTROL, ret, 0, "Enable RGB led");
}
