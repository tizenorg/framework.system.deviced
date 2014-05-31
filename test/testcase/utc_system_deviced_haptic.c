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
#include <dd-haptic.h>
#include <sys/stat.h>
#include <errno.h>

#define API_NAME_HAPTIC_STARTUP "haptic_startup"
#define API_NAME_HAPTIC_GET_COUNT "haptic_get_count"
#define API_NAME_HAPTIC_OPEN "haptic_open"
#define API_NAME_HAPTIC_CLOSE "haptic_close"
#define API_NAME_HAPTIC_VIBRATE_MONOTONE "haptic_vibrate_monotone"
#define API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL "haptic_vibrate_monotone_with_detail"
#define API_NAME_HAPTIC_VIBRATE_FILE "haptic_vibrate_file"
#define API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL "haptic_vibrate_file_with_detail"
#define API_NAME_HAPTIC_VIBRATE_BUFFERS "haptic_vibrate_buffers"
#define API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL "haptic_vibrate_buffers_with_detail"
#define API_NAME_HAPTIC_STOP_ALL_EFFECTS "haptic_stop_all_effects"
#define API_NAME_HAPTIC_GET_EFFECT_STATE "haptic_get_effect_state"
#define API_NAME_HAPTIC_CREATE_EFFECT "haptic_create_effect"
#define API_NAME_HAPTIC_SAVE_EFFECT "haptic_save_effect"
#define API_NAME_HAPTIC_GET_FILE_DURATION "haptic_get_file_duration"
#define API_NAME_HAPTIC_GET_BUFFER_DURATION "haptic_get_buffer_duration"
#define API_NAME_HAPTIC_SAVE_LED "haptic_save_led"


static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_haptic_get_count_p(void);
static void utc_system_deviced_haptic_get_count_n(void);
static void utc_system_deviced_haptic_open_p(void);
static void utc_system_deviced_haptic_open_n_1(void);
static void utc_system_deviced_haptic_open_n_2(void);
static void utc_system_deviced_haptic_close_p(void);
static void utc_system_deviced_haptic_close_n(void);
static void utc_system_deviced_haptic_vibrate_monotone_p_1(void);
static void utc_system_deviced_haptic_vibrate_monotone_p_2(void);
static void utc_system_deviced_haptic_vibrate_monotone_n_1(void);
static void utc_system_deviced_haptic_vibrate_monotone_n_2(void);
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_p_1(void);
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_p_2(void);
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_1(void);
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_2(void);
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_3(void);
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_4(void);
static void utc_system_deviced_haptic_vibrate_file_p_1(void);
static void utc_system_deviced_haptic_vibrate_file_p_2(void);
static void utc_system_deviced_haptic_vibrate_file_n_1(void);
static void utc_system_deviced_haptic_vibrate_file_n_2(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_p_1(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_p_2(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_1(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_2(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_3(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_4(void);
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_5(void);
static void utc_system_deviced_haptic_vibrate_buffers_p_1(void);
static void utc_system_deviced_haptic_vibrate_buffers_p_2(void);
static void utc_system_deviced_haptic_vibrate_buffers_n_1(void);
static void utc_system_deviced_haptic_vibrate_buffers_n_2(void);
static void utc_system_deviced_haptic_vibrate_buffers_n_3(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_p_1(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_p_2(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_1(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_2(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_3(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_4(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_5(void);
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_6(void);
static void utc_system_deviced_haptic_stop_all_effects_p(void);
static void utc_system_deviced_haptic_stop_all_effects_n(void);
static void utc_system_deviced_haptic_get_effect_state_p(void);
static void utc_system_deviced_haptic_get_effect_state_n_1(void);
static void utc_system_deviced_haptic_get_effect_state_n_2(void);
static void utc_system_deviced_haptic_create_effect_p(void);
static void utc_system_deviced_haptic_create_effect_n_1(void);
static void utc_system_deviced_haptic_create_effect_n_2(void);
static void utc_system_deviced_haptic_create_effect_n_3(void);
static void utc_system_deviced_haptic_create_effect_n_4(void);
static void utc_system_deviced_haptic_save_effect_p(void);
static void utc_system_deviced_haptic_save_effect_n_1(void);
static void utc_system_deviced_haptic_save_effect_n_2(void);
static void utc_system_deviced_haptic_save_effect_n_3(void);
static void utc_system_deviced_haptic_get_file_duration_p(void);
static void utc_system_deviced_haptic_get_file_duration_n_1(void);
static void utc_system_deviced_haptic_get_file_duration_n_2(void);
static void utc_system_deviced_haptic_get_file_duration_n_3(void);
static void utc_system_deviced_haptic_get_buffer_duration_p(void);
static void utc_system_deviced_haptic_get_buffer_duration_n_1(void);
static void utc_system_deviced_haptic_get_buffer_duration_n_2(void);
static void utc_system_deviced_haptic_get_buffer_duration_n_3(void);
static void utc_system_deviced_haptic_save_led_p(void);
static void utc_system_deviced_haptic_save_led_n_1(void);
static void utc_system_deviced_haptic_save_led_n_2(void);
static void utc_system_deviced_haptic_save_led_n_3(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_haptic_get_count_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_count_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_open_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_open_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_open_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_create_effect_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_create_effect_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_create_effect_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_create_effect_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_create_effect_n_4, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_effect_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_effect_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_effect_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_effect_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_with_detail_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_with_detail_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_with_detail_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_with_detail_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_with_detail_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_monotone_with_detail_n_4, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_n_4, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_file_with_detail_n_5, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_n_4, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_n_5, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_vibrate_buffers_with_detail_n_6, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_stop_all_effects_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_stop_all_effects_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_effect_state_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_effect_state_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_effect_state_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_file_duration_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_file_duration_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_file_duration_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_file_duration_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_buffer_duration_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_buffer_duration_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_buffer_duration_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_get_buffer_duration_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_led_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_led_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_led_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_save_led_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_haptic_close_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_haptic_close_n, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

#define DEVICED_FILE_PATH "/opt/usr/share/deviced"
#define HAPTIC_FILE_PATH DEVICED_FILE_PATH"/test.ivt"
#define BINARY_FILE_PATH DEVICED_FILE_PATH"/test.led"

static haptic_device_h haptic_h;
static unsigned char haptic_buffer[1024];
static int haptic_buf_size = 1024;

static void startup(void)
{
	int ret;

	ret = mkdir(DEVICED_FILE_PATH, 0777);
	if (ret < 0)
		dts_message(API_NAME_HAPTIC_STARTUP,
				"fail to make direcotry(%s) : %s",
				DEVICED_FILE_PATH, strerror(errno));
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of haptic_get_count()
 */
static void utc_system_deviced_haptic_get_count_p(void)
{
	int val, ret;

	ret = haptic_get_count(&val);
	dts_check_eq(API_NAME_HAPTIC_GET_COUNT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_count()
 */
static void utc_system_deviced_haptic_get_count_n(void)
{
	int ret;

	ret = haptic_get_count(NULL);
	dts_check_ne(API_NAME_HAPTIC_GET_COUNT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_open()
 */
static void utc_system_deviced_haptic_open_p(void)
{
	int ret;

	ret = haptic_open(HAPTIC_DEVICE_ALL, &haptic_h);
	dts_check_eq(API_NAME_HAPTIC_OPEN, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_open()
 */
static void utc_system_deviced_haptic_open_n_1(void)
{
	int ret;

	ret = haptic_open(-1, &haptic_h);
	dts_check_ne(API_NAME_HAPTIC_OPEN, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_open()
 */
static void utc_system_deviced_haptic_open_n_2(void)
{
	int ret;

	ret = haptic_open(HAPTIC_DEVICE_ALL, NULL);
	dts_check_ne(API_NAME_HAPTIC_OPEN, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_close()
 */
static void utc_system_deviced_haptic_close_p(void)
{
	int ret;

	ret = haptic_close(haptic_h);
	dts_check_eq(API_NAME_HAPTIC_CLOSE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_close()
 */
static void utc_system_deviced_haptic_close_n(void)
{
	int ret;

	ret = haptic_close((haptic_device_h)-1);
	dts_check_ne(API_NAME_HAPTIC_CLOSE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_monotone()
 */
static void utc_system_deviced_haptic_vibrate_monotone_p_1(void)
{
	haptic_effect_h eh;
	int ret;

	ret = haptic_vibrate_monotone(haptic_h, 1000, &eh);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_MONOTONE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_monotone()
 */
static void utc_system_deviced_haptic_vibrate_monotone_p_2(void)
{
	int ret;

	ret = haptic_vibrate_monotone(haptic_h, 1000, NULL);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_MONOTONE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_monotone()
 */
static void utc_system_deviced_haptic_vibrate_monotone_n_1(void)
{
	int ret;

	ret = haptic_vibrate_monotone((haptic_device_h)-1, 1000, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_MONOTONE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_monotone()
 */
static void utc_system_deviced_haptic_vibrate_monotone_n_2(void)
{
	int ret;

	ret = haptic_vibrate_monotone(haptic_h, -1, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_MONOTONE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_monotone_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_p_1(void)
{
	haptic_effect_h eh;
	int ret;

	ret = haptic_vibrate_monotone_with_detail(haptic_h, 1000,
			HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, &eh);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_monotone_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_p_2(void)
{
	int ret;

	ret = haptic_vibrate_monotone_with_detail(haptic_h, 1000,
			HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_monotone_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_1(void)
{
	int ret;

	ret = haptic_vibrate_monotone_with_detail((haptic_device_h)-1, 1000,
			HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_monotone_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_2(void)
{
	int ret;

	ret = haptic_vibrate_monotone_with_detail(haptic_h, -1,
			HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_monotone_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_3(void)
{
	int ret;

	ret = haptic_vibrate_monotone_with_detail(haptic_h, 1000,
			-1, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_monotone_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_monotone_with_detail_n_4(void)
{
	int ret;

	ret = haptic_vibrate_monotone_with_detail(haptic_h, 1000,
			HAPTIC_FEEDBACK_5, -1, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_MONOTONE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_file()
 */
static void utc_system_deviced_haptic_vibrate_file_p_1(void)
{
	haptic_effect_h eh;
	int ret;

	ret = haptic_vibrate_file(haptic_h, HAPTIC_FILE_PATH, &eh);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_FILE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_file()
 */
static void utc_system_deviced_haptic_vibrate_file_p_2(void)
{
	int ret;

	ret = haptic_vibrate_file(haptic_h, HAPTIC_FILE_PATH, NULL);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_FILE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file()
 */
static void utc_system_deviced_haptic_vibrate_file_n_1(void)
{
	int ret;

	ret = haptic_vibrate_file((haptic_device_h)-1, HAPTIC_FILE_PATH, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file()
 */
static void utc_system_deviced_haptic_vibrate_file_n_2(void)
{
	int ret;

	ret = haptic_vibrate_file(haptic_h, NULL, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_p_1(void)
{
	haptic_effect_h eh;
	int ret;

	ret = haptic_vibrate_file_with_detail(haptic_h, HAPTIC_FILE_PATH,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, &eh);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_p_2(void)
{
	int ret;

	ret = haptic_vibrate_file_with_detail(haptic_h, HAPTIC_FILE_PATH,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_1(void)
{
	int ret;

	ret = haptic_vibrate_file_with_detail((haptic_device_h)-1, HAPTIC_FILE_PATH,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_2(void)
{
	int ret;

	ret = haptic_vibrate_file_with_detail(haptic_h, NULL,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_3(void)
{
	int ret;

	ret = haptic_vibrate_file_with_detail(haptic_h, HAPTIC_FILE_PATH,
			-1, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_4(void)
{
	int ret;

	ret = haptic_vibrate_file_with_detail(haptic_h, HAPTIC_FILE_PATH,
			HAPTIC_ITERATION_ONCE, -1, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_file_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_file_with_detail_n_5(void)
{
	int ret;

	ret = haptic_vibrate_file_with_detail(haptic_h, HAPTIC_FILE_PATH,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, -1, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_FILE_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_buffers()
 */
static void utc_system_deviced_haptic_vibrate_buffers_p_1(void)
{
	haptic_effect_h eh;
	int ret;

	ret = haptic_vibrate_buffers(haptic_h, haptic_buffer, haptic_buf_size, &eh);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_BUFFERS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_buffers()
 */
static void utc_system_deviced_haptic_vibrate_buffers_p_2(void)
{
	int ret;

	ret = haptic_vibrate_buffers(haptic_h, haptic_buffer, haptic_buf_size, NULL);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_BUFFERS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers()
 */
static void utc_system_deviced_haptic_vibrate_buffers_n_1(void)
{
	int ret;

	ret = haptic_vibrate_buffers((haptic_device_h)-1, haptic_buffer, haptic_buf_size, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers()
 */
static void utc_system_deviced_haptic_vibrate_buffers_n_2(void)
{
	int ret;

	ret = haptic_vibrate_buffers(haptic_h, NULL, haptic_buf_size, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers()
 */
static void utc_system_deviced_haptic_vibrate_buffers_n_3(void)
{
	int ret;

	ret = haptic_vibrate_buffers(haptic_h, haptic_buffer, -1, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_p_1(void)
{
	haptic_effect_h eh;
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, haptic_buffer, haptic_buf_size,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, &eh);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_p_2(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, haptic_buffer, haptic_buf_size,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_eq(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_1(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail((haptic_device_h)-1, haptic_buffer, haptic_buf_size,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_2(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, NULL, haptic_buf_size,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_3(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, haptic_buffer, -1,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_4(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, haptic_buffer, haptic_buf_size,
			-1, HAPTIC_FEEDBACK_5, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_5(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, haptic_buffer, haptic_buf_size,
			HAPTIC_ITERATION_ONCE, -1, HAPTIC_PRIORITY_MIN, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_vibrate_buffers_with_detail()
 */
static void utc_system_deviced_haptic_vibrate_buffers_with_detail_n_6(void)
{
	int ret;

	ret = haptic_vibrate_buffers_with_detail(haptic_h, haptic_buffer, haptic_buf_size,
			HAPTIC_ITERATION_ONCE, HAPTIC_FEEDBACK_5, -1, NULL);
	dts_check_ne(API_NAME_HAPTIC_VIBRATE_BUFFERS_WITH_DETAIL, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_stop_all_effects()
 */
static void utc_system_deviced_haptic_stop_all_effects_p(void)
{
	int ret;

	ret = haptic_stop_all_effects(haptic_h);
	dts_check_eq(API_NAME_HAPTIC_STOP_ALL_EFFECTS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_stop_all_effects()
 */
static void utc_system_deviced_haptic_stop_all_effects_n(void)
{
	int ret;

	ret = haptic_stop_all_effects((haptic_device_h)-1);
	dts_check_ne(API_NAME_HAPTIC_STOP_ALL_EFFECTS, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_get_effect_state()
 */
static void utc_system_deviced_haptic_get_effect_state_p(void)
{
	haptic_state_e val;
	int ret;

	ret = haptic_get_effect_state(haptic_h, (haptic_effect_h)0, &val);
	dts_check_eq(API_NAME_HAPTIC_GET_EFFECT_STATE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_effect_state()
 */
static void utc_system_deviced_haptic_get_effect_state_n_1(void)
{
	haptic_state_e val;
	int ret;

	ret = haptic_get_effect_state((haptic_device_h)NULL, (haptic_effect_h)0, &val);
	dts_check_ne(API_NAME_HAPTIC_GET_EFFECT_STATE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_effect_state()
 */
static void utc_system_deviced_haptic_get_effect_state_n_2(void)
{
	int ret;

	ret = haptic_get_effect_state(haptic_h, 0, NULL);
	dts_check_ne(API_NAME_HAPTIC_GET_EFFECT_STATE, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_create_effect()
 */
static void utc_system_deviced_haptic_create_effect_p(void)
{
	int ret;
	haptic_effect_element_s elem[3] = { {1000, 100}, {500, 0}, {500, 100} };

	ret = haptic_create_effect(haptic_buffer, haptic_buf_size, elem, 3);
	dts_check_eq(API_NAME_HAPTIC_CREATE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_create_effect()
 */
static void utc_system_deviced_haptic_create_effect_n_1(void)
{
	int ret;
	haptic_effect_element_s elem[3] = { {1000, 100}, {500, 0}, {500, 100} };

	ret = haptic_create_effect(NULL, haptic_buf_size, elem, 3);
	dts_check_ne(API_NAME_HAPTIC_CREATE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_create_effect()
 */
static void utc_system_deviced_haptic_create_effect_n_2(void)
{
	int ret;
	haptic_effect_element_s elem[3] = { {1000, 100}, {500, 0}, {500, 100} };

	ret = haptic_create_effect(haptic_buffer, -1, elem, 3);
	dts_check_ne(API_NAME_HAPTIC_CREATE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_create_effect()
 */
static void utc_system_deviced_haptic_create_effect_n_3(void)
{
	int ret;

	ret = haptic_create_effect(haptic_buffer, haptic_buf_size, NULL, 3);
	dts_check_ne(API_NAME_HAPTIC_CREATE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_create_effect()
 */
static void utc_system_deviced_haptic_create_effect_n_4(void)
{
	int ret;
	haptic_effect_element_s elem[3] = { {1000, 100}, {500, 0}, {500, 100} };

	ret = haptic_create_effect(haptic_buffer, haptic_buf_size, elem, -1);
	dts_check_ne(API_NAME_HAPTIC_CREATE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_save_effect()
 */
static void utc_system_deviced_haptic_save_effect_p(void)
{
	int ret;

	ret = haptic_save_effect(haptic_buffer, haptic_buf_size, HAPTIC_FILE_PATH);
	dts_check_eq(API_NAME_HAPTIC_SAVE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_save_effect()
 */
static void utc_system_deviced_haptic_save_effect_n_1(void)
{
	int ret;

	ret = haptic_save_effect(NULL, haptic_buf_size, HAPTIC_FILE_PATH);
	dts_check_ne(API_NAME_HAPTIC_SAVE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_save_effect()
 */
static void utc_system_deviced_haptic_save_effect_n_2(void)
{
	int ret;

	ret = haptic_save_effect(haptic_buffer, -1, HAPTIC_FILE_PATH);
	dts_check_ne(API_NAME_HAPTIC_SAVE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_save_effect()
 */
static void utc_system_deviced_haptic_save_effect_n_3(void)
{
	int ret;

	ret = haptic_save_effect(haptic_buffer, haptic_buf_size, NULL);
	dts_check_ne(API_NAME_HAPTIC_SAVE_EFFECT, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_get_file_duration()
 */
static void utc_system_deviced_haptic_get_file_duration_p(void)
{
	int val, ret;

	ret = haptic_get_file_duration(haptic_h, HAPTIC_FILE_PATH, &val);
	dts_check_eq(API_NAME_HAPTIC_GET_FILE_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_file_duration()
 */
static void utc_system_deviced_haptic_get_file_duration_n_1(void)
{
	int val, ret;

	ret = haptic_get_file_duration((haptic_device_h)-1, HAPTIC_FILE_PATH, &val);
	dts_check_ne(API_NAME_HAPTIC_GET_FILE_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_file_duration()
 */
static void utc_system_deviced_haptic_get_file_duration_n_2(void)
{
	int val, ret;

	ret = haptic_get_file_duration(haptic_h, NULL, &val);
	dts_check_ne(API_NAME_HAPTIC_GET_FILE_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_file_duration()
 */
static void utc_system_deviced_haptic_get_file_duration_n_3(void)
{
	int ret;

	ret = haptic_get_file_duration(haptic_h, HAPTIC_FILE_PATH, NULL);
	dts_check_ne(API_NAME_HAPTIC_GET_FILE_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_get_buffer_duration()
 */
static void utc_system_deviced_haptic_get_buffer_duration_p(void)
{
	int val, ret;

	ret = haptic_get_buffer_duration(haptic_h, haptic_buffer, &val);
	dts_check_eq(API_NAME_HAPTIC_GET_BUFFER_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_buffer_duration()
 */
static void utc_system_deviced_haptic_get_buffer_duration_n_1(void)
{
	int val, ret;

	ret = haptic_get_buffer_duration((haptic_device_h)-1, haptic_buffer, &val);
	dts_check_ne(API_NAME_HAPTIC_GET_BUFFER_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_buffer_duration()
 */
static void utc_system_deviced_haptic_get_buffer_duration_n_2(void)
{
	int val, ret;

	ret = haptic_get_buffer_duration(haptic_h, NULL, &val);
	dts_check_ne(API_NAME_HAPTIC_GET_BUFFER_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_get_buffer_duration()
 */
static void utc_system_deviced_haptic_get_buffer_duration_n_3(void)
{
	int ret;

	ret = haptic_get_buffer_duration(haptic_h, haptic_buffer, NULL);
	dts_check_ne(API_NAME_HAPTIC_GET_BUFFER_DURATION, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Positive test case of haptic_save_led()
 */
static void utc_system_deviced_haptic_save_led_p(void)
{
	int ret;

	ret = haptic_save_led(haptic_buffer, haptic_buf_size, BINARY_FILE_PATH);
	dts_check_eq(API_NAME_HAPTIC_SAVE_LED, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_save_led()
 */
static void utc_system_deviced_haptic_save_led_n_1(void)
{
	int ret;

	ret = haptic_save_led(NULL, haptic_buf_size, BINARY_FILE_PATH);
	dts_check_ne(API_NAME_HAPTIC_SAVE_LED, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_save_led()
 */
static void utc_system_deviced_haptic_save_led_n_2(void)
{
	int ret;

	ret = haptic_save_led(haptic_buffer, -1, BINARY_FILE_PATH);
	dts_check_ne(API_NAME_HAPTIC_SAVE_LED, ret, HAPTIC_ERROR_NONE);
}

/**
 * @brief Negative test case of haptic_save_led()
 */
static void utc_system_deviced_haptic_save_led_n_3(void)
{
	int ret;

	ret = haptic_save_led(haptic_buffer, haptic_buf_size, NULL);
	dts_check_ne(API_NAME_HAPTIC_SAVE_LED, ret, HAPTIC_ERROR_NONE);
}
