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
#include <dd-display.h>

#define API_NAME_DISPLAY_GET_COUNT "display_get_count"
#define API_NAME_DISPLAY_GET_MAX_BRIGHTNESS "display_get_max_brightness"
#define API_NAME_DISPLAY_GET_MIN_BRIGHTNESS "display_get_min_brightness"
#define API_NAME_DISPLAY_GET_BRIGHTNESS "display_get_brightness"
#define API_NAME_DISPLAY_SET_BRIGHTNESS_WITH_SETTING "display_set_brightness_with_setting"
#define API_NAME_DISPLAY_SET_BRIGHTNESS "display_set_brightness"
#define API_NAME_DISPLAY_RELEASE_BRIGHTNESS "display_release_brightness"
#define API_NAME_DISPLAY_GET_ACL_STATUS "display_get_acl_status"
#define API_NAME_DISPLAY_SET_ACL_STATUS "display_set_acl_status"
#define API_NAME_DISPLAY_GET_IMAGE_ENHANCE_INFO "display_get_enhance_info"
#define API_NAME_DISPLAY_GET_IMAGE_ENHANCE "display_get_image_enhance"
#define API_NAME_DISPLAY_SET_IMAGE_ENHANCE "display_set_image_enhance"
#define API_NAME_DISPLAY_SET_REFRESH_RATE "display_set_refresh_rate"
#define API_NAME_DISPLAY_LOCK_STATE "display_lock_state"
#define API_NAME_DISPLAY_UNLOCK_STATE "display_unlock_state"
#define API_NAME_DISPLAY_CHANGE_STATE "display_change_state"
#define API_NAME_DISPLAY_GET_AUTO_SCREEN_TONE "display_get_auto_screen_tone"
#define API_NAME_DISPLAY_SET_AUTO_SCREEN_TONE "display_Set_auto_screen_tone"
#define API_NAME_DISPLAY_GET_COLOR_BLIND "display_get_color_blind"
#define API_NAME_DISPLAY_SET_COLOR_BLIND "display_set_color_blind"
#define API_NAME_DISPLAY_GET_ENHANCED_TOUCH "display_get_enhanced_touch"
#define API_NAME_DISPLAY_SET_ENHANCED_TOUCH "display_set_enhanced_touch"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_display_get_count_p(void);
static void utc_system_deviced_display_get_max_brightness_p(void);
static void utc_system_deviced_display_get_min_brightness_p(void);
static void utc_system_deviced_display_get_brightness_p(void);
static void utc_system_deviced_display_set_brightness_with_setting_p(void);
static void utc_system_deviced_display_set_brightness_with_setting_n(void);
static void utc_system_deviced_display_set_brightness_p(void);
static void utc_system_deviced_display_set_brightness_n(void);
static void utc_system_deviced_display_release_brightness_p(void);
static void utc_system_deviced_display_get_acl_status_p(void);
static void utc_system_deviced_display_set_acl_status_p_1(void);
static void utc_system_deviced_display_set_acl_status_p_2(void);
static void utc_system_deviced_display_set_acl_status_n(void);
static void utc_system_deviced_display_get_image_enhance_info_p(void);
static void utc_system_deviced_display_get_image_enhance_p(void);
static void utc_system_deviced_display_get_image_enhance_n(void);
static void utc_system_deviced_display_set_image_enhance_p(void);
static void utc_system_deviced_display_set_image_enhance_n_1(void);
static void utc_system_deviced_display_set_image_enhance_n_2(void);
static void utc_system_deviced_display_set_refresh_rate_p(void);
static void utc_system_deviced_display_set_refresh_rate_n_1(void);
static void utc_system_deviced_display_set_refresh_rate_n_2(void);
static void utc_system_deviced_display_lock_state_p(void);
static void utc_system_deviced_display_lock_state_n_1(void);
static void utc_system_deviced_display_lock_state_n_2(void);
static void utc_system_deviced_display_lock_state_n_3(void);
static void utc_system_deviced_display_unlock_state_p(void);
static void utc_system_deviced_display_unlock_state_n_1(void);
static void utc_system_deviced_display_unlock_state_n_2(void);
static void utc_system_deviced_display_change_state_p(void);
static void utc_system_deviced_display_change_state_n(void);
static void utc_system_deviced_display_get_auto_screen_tone_p(void);
static void utc_system_deviced_display_set_auto_screen_tone_p_1(void);
static void utc_system_deviced_display_set_auto_screen_tone_p_2(void);
static void utc_system_deviced_display_set_auto_screen_tone_n(void);
static void utc_system_deviced_display_get_color_blind_p(void);
static void utc_system_deviced_display_set_color_blind_p_1(void);
static void utc_system_deviced_display_set_color_blind_p_2(void);
static void utc_system_deviced_display_set_color_blind_n_1(void);
static void utc_system_deviced_display_set_color_blind_n_2(void);
static void utc_system_deviced_display_get_enhanced_touch_p(void);
static void utc_system_deviced_display_set_enhanced_touch_p_1(void);
static void utc_system_deviced_display_set_enhanced_touch_p_2(void);
static void utc_system_deviced_display_set_enhanced_touch_n(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_display_get_count_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_get_max_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_get_min_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_get_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_brightness_with_setting_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_brightness_with_setting_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_set_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_brightness_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_release_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_get_acl_status_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_acl_status_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_acl_status_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_acl_status_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_get_image_enhance_info_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_get_image_enhance_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_get_image_enhance_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_set_image_enhance_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_image_enhance_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_set_image_enhance_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_set_refresh_rate_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_refresh_rate_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_set_refresh_rate_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_lock_state_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_lock_state_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_lock_state_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_lock_state_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_unlock_state_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_unlock_state_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_unlock_state_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_change_state_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_change_state_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_get_auto_screen_tone_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_auto_screen_tone_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_auto_screen_tone_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_auto_screen_tone_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_get_color_blind_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_color_blind_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_color_blind_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_color_blind_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_set_color_blind_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_display_get_enhanced_touch_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_enhanced_touch_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_enhanced_touch_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_display_set_enhanced_touch_n, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of display_get_count()
 */
static void utc_system_deviced_display_get_count_p(void)
{
	int ret;

	ret = display_get_count();
	dts_check_ge(API_NAME_DISPLAY_GET_COUNT, ret, 0);
}

/**
 * @brief Positive test case of display_get_max_brightness()
 */
static void utc_system_deviced_display_get_max_brightness_p(void)
{
	int ret;

	ret = display_get_max_brightness();
	dts_check_ge(API_NAME_DISPLAY_GET_MAX_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of display_get_min_brightness()
 */
static void utc_system_deviced_display_get_min_brightness_p(void)
{
	int ret;

	ret = display_get_min_brightness();
	dts_check_ge(API_NAME_DISPLAY_GET_MIN_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of display_get_brightness()
 */
static void utc_system_deviced_display_get_brightness_p(void)
{
	int ret;

	ret = display_get_brightness();
	dts_check_ge(API_NAME_DISPLAY_GET_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of display_set_brightness_with_setting()
 */
static void utc_system_deviced_display_set_brightness_with_setting_p(void)
{
	int ret;

	ret = display_set_brightness_with_setting(100);
	dts_check_eq(API_NAME_DISPLAY_SET_BRIGHTNESS_WITH_SETTING, ret, 0);
}

/**
 * @brief Negative test case of display_set_brightness_with_setting()
 */
static void utc_system_deviced_display_set_brightness_with_setting_n(void)
{
	int ret;

	ret = display_set_brightness_with_setting(-1);
	dts_check_ne(API_NAME_DISPLAY_SET_BRIGHTNESS_WITH_SETTING, ret, 0);
}

/**
 * @brief Positive test case of display_set_brightness()
 */
static void utc_system_deviced_display_set_brightness_p(void)
{
	int ret;

	ret = display_set_brightness(50);
	dts_check_eq(API_NAME_DISPLAY_SET_BRIGHTNESS, ret, 0);
}

/**
 * @brief Negative test case of display_set_brightness()
 */
static void utc_system_deviced_display_set_brightness_n(void)
{
	int ret;

	ret = display_set_brightness(-1);
	dts_check_ne(API_NAME_DISPLAY_SET_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of display_release_brightness()
 */
static void utc_system_deviced_display_release_brightness_p(void)
{
	int ret;

	ret = display_release_brightness();
	dts_check_eq(API_NAME_DISPLAY_RELEASE_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of display_get_acl_status()
 */
static void utc_system_deviced_display_get_acl_status_p(void)
{
	int ret;

	ret = display_get_acl_status();
	dts_check_ge(API_NAME_DISPLAY_GET_ACL_STATUS, ret, 0);
}

/**
 * @brief Positive test case of display_set_acl_status()
 */
static void utc_system_deviced_display_set_acl_status_p_1(void)
{
	int ret;

	ret = display_set_acl_status(1);
	dts_check_eq(API_NAME_DISPLAY_SET_ACL_STATUS, ret, 0, "Enable acl");
}

/**
 * @brief Positive test case of display_set_acl_status()
 */
static void utc_system_deviced_display_set_acl_status_p_2(void)
{
	int ret;

	ret = display_set_acl_status(0);
	dts_check_eq(API_NAME_DISPLAY_SET_ACL_STATUS, ret, 0, "Disable acl");
}

/**
 * @brief Negative test case of display_set_acl_status()
 */
static void utc_system_deviced_display_set_acl_status_n(void)
{
	int ret;

	ret = display_set_acl_status(-1);
	dts_check_ne(API_NAME_DISPLAY_SET_ACL_STATUS, ret, 0);
}

/**
 * @brief Positive test case of display_get_image_enhance_info()
 */
static void utc_system_deviced_display_get_image_enhance_info_p(void)
{
	int ret;

	ret = display_get_image_enhance_info();
	dts_check_ge(API_NAME_DISPLAY_GET_IMAGE_ENHANCE_INFO, ret, 0);
}

/**
 * @brief Positive test case of display_get_image_enhance()
 */
static void utc_system_deviced_display_get_image_enhance_p(void)
{
	int i, ret;

	for (i = ENHANCE_MODE; i <= ENHANCE_OUTDOOR; ++i) {
		ret = display_get_image_enhance(i);
		dts_check_ge(API_NAME_DISPLAY_GET_IMAGE_ENHANCE, ret, 0, "enhance type : %d", i);
	}
}

/**
 * @brief Negative test case of display_get_image_enhance()
 */
static void utc_system_deviced_display_get_image_enhance_n(void)
{
	int ret;

	ret = display_get_image_enhance(-1);
	dts_check_ne(API_NAME_DISPLAY_GET_IMAGE_ENHANCE, ret, 0);
}

/**
 * @brief Positive test case of display_set_image_enhance()
 */
static void utc_system_deviced_display_set_image_enhance_p(void)
{
	int i, ret;

	for (i = ENHANCE_MODE; i <= ENHANCE_OUTDOOR; ++i) {
		ret = display_set_image_enhance(i, 0);
		dts_check_eq(API_NAME_DISPLAY_SET_IMAGE_ENHANCE, ret, 0, "enhance type : %d", i);
	}
}

/**
 * @brief Negative test case of display_set_image_enhance()
 */
static void utc_system_deviced_display_set_image_enhance_n_1(void)
{
	int ret;

	ret = display_set_image_enhance(-1, 0);
	dts_check_ne(API_NAME_DISPLAY_SET_IMAGE_ENHANCE, ret, 0);
}

/**
 * @brief Negative test case of display_set_image_enhance()
 */
static void utc_system_deviced_display_set_image_enhance_n_2(void)
{
	int ret;

	ret = display_set_image_enhance(0, -1);
	dts_check_ne(API_NAME_DISPLAY_SET_IMAGE_ENHANCE, ret, 0);
}

/**
 * @brief Positive test case of display_set_refresh_rate()
 */
static void utc_system_deviced_display_set_refresh_rate_p(void)
{
	int i, ret;

	for (i = REFRESH_SETTING; i <= REFRESH_WEB; ++i) {
		ret = display_set_refresh_rate(i, 60);
		dts_check_eq(API_NAME_DISPLAY_SET_REFRESH_RATE, ret, 0, "refresh type : %d", i);
	}
}

/**
 * @brief Negative test case of display_set_refresh_rate()
 */
static void utc_system_deviced_display_set_refresh_rate_n_1(void)
{
	int ret;

	ret = display_set_refresh_rate(-1, 60);
	dts_check_ne(API_NAME_DISPLAY_SET_REFRESH_RATE, ret, 0);
}

/**
 * @brief Negative test case of display_set_refresh_rate()
 */
static void utc_system_deviced_display_set_refresh_rate_n_2(void)
{
	int ret;

	ret = display_set_refresh_rate(0, -1);
	dts_check_ne(API_NAME_DISPLAY_SET_REFRESH_RATE, ret, 0);
}

/**
 * @brief Positive test case of display_lock_state()
 */
static void utc_system_deviced_display_lock_state_p(void)
{
	int ret;

	ret = display_lock_state(LCD_NORMAL, GOTO_STATE_NOW, 0);
	dts_check_eq(API_NAME_DISPLAY_LOCK_STATE, ret, 0);
}

/**
 * @brief Negative test case of display_lock_state()
 */
static void utc_system_deviced_display_lock_state_n_1(void)
{
	int ret;

	ret = display_lock_state(-1, GOTO_STATE_NOW, 0);
	dts_check_ne(API_NAME_DISPLAY_LOCK_STATE, ret, 0);
}

/**
 * @brief Negative test case of display_lock_state()
 */
static void utc_system_deviced_display_lock_state_n_2(void)
{
	int ret;

	ret = display_lock_state(LCD_NORMAL, -1, 0);
	dts_check_ne(API_NAME_DISPLAY_LOCK_STATE, ret, 0);
}

/**
 * @brief Negative test case of display_lock_state()
 */
static void utc_system_deviced_display_lock_state_n_3(void)
{
	int ret;

	ret = display_lock_state(LCD_NORMAL, GOTO_STATE_NOW, -1);
	dts_check_ne(API_NAME_DISPLAY_LOCK_STATE, ret, 0);
}

/**
 * @brief Positive test case of display_unlock_state()
 */
static void utc_system_deviced_display_unlock_state_p(void)
{
	int ret;

	ret = display_unlock_state(LCD_NORMAL, PM_RESET_TIMER);
	dts_check_eq(API_NAME_DISPLAY_UNLOCK_STATE, ret, 0);
}

/**
 * @brief Negative test case of display_unlock_state()
 */
static void utc_system_deviced_display_unlock_state_n_1(void)
{
	int ret;

	ret = display_unlock_state(-1, PM_RESET_TIMER);
	dts_check_ne(API_NAME_DISPLAY_UNLOCK_STATE, ret, 0);
}

/**
 * @brief Negative test case of display_unlock_state()
 */
static void utc_system_deviced_display_unlock_state_n_2(void)
{
	int ret;

	ret = display_unlock_state(LCD_NORMAL, -1);
	dts_check_ne(API_NAME_DISPLAY_UNLOCK_STATE, ret, 0);
}

/**
 * @brief Positive test case of display_change_state()
 */
static void utc_system_deviced_display_change_state_p(void)
{
	int ret;

	ret = display_change_state(LCD_NORMAL);
	dts_check_eq(API_NAME_DISPLAY_CHANGE_STATE, ret, 0);
}

/**
 * @brief Positive test case of display_change_state()
 */
static void utc_system_deviced_display_change_state_n(void)
{
	int ret;

	ret = display_change_state(-1);
	dts_check_ne(API_NAME_DISPLAY_CHANGE_STATE, ret, 0);
}

/**
 * @brief Positive test case of display_get_auto_screen_tone()
 */
static void utc_system_deviced_display_get_auto_screen_tone_p(void)
{
	int ret;

	ret = display_get_auto_screen_tone();
	dts_check_ge(API_NAME_DISPLAY_GET_AUTO_SCREEN_TONE, ret, 0);
}

/**
 * @brief Positive test case of display_set_auto_screen_tone()
 */
static void utc_system_deviced_display_set_auto_screen_tone_p_1(void)
{
	int ret;

	ret = display_set_auto_screen_tone(TONE_ON);
	dts_check_eq(API_NAME_DISPLAY_SET_AUTO_SCREEN_TONE, ret, 0, "Enable auto screen tone");
}

/**
 * @brief Positive test case of display_set_auto_screen_tone()
 */
static void utc_system_deviced_display_set_auto_screen_tone_p_2(void)
{
	int ret;

	ret = display_set_auto_screen_tone(TONE_OFF);
	dts_check_eq(API_NAME_DISPLAY_SET_AUTO_SCREEN_TONE, ret, 0, "Disable auto screen tone");
}

/**
 * @brief Negative test case of display_set_auto_screen_tone()
 */
static void utc_system_deviced_display_set_auto_screen_tone_n(void)
{
	int ret;

	ret = display_set_auto_screen_tone(-1);
	dts_check_ne(API_NAME_DISPLAY_SET_AUTO_SCREEN_TONE, ret, 0);
}

/**
 * @brief Positive test case of display_get_color_blind()
 */
static void utc_system_deviced_display_get_color_blind_p(void)
{
	int ret;

	ret = display_get_color_blind();
	dts_check_ge(API_NAME_DISPLAY_GET_COLOR_BLIND, ret, 0);
}

/**
 * @brief Positive test case of display_set_color_blind()
 */
static void utc_system_deviced_display_set_color_blind_p_1(void)
{
	int ret;
	struct blind_color_info info = {0,};

	ret = display_set_color_blind(true, &info);
	dts_check_eq(API_NAME_DISPLAY_SET_COLOR_BLIND, ret, 0);
}

/**
 * @brief Positive test case of display_set_color_blind()
 */
static void utc_system_deviced_display_set_color_blind_p_2(void)
{
	int ret;
	struct blind_color_info info = {0,};

	ret = display_set_color_blind(false, &info);
	dts_check_eq(API_NAME_DISPLAY_SET_COLOR_BLIND, ret, 0);
}

/**
 * @brief Negative test case of display_set_color_blind()
 */
static void utc_system_deviced_display_set_color_blind_n_1(void)
{
	int ret;
	struct blind_color_info info = {0,};

	ret = display_set_color_blind(-1, &info);
	dts_check_ne(API_NAME_DISPLAY_SET_COLOR_BLIND, ret, 0);
}

/**
 * @brief Positive test case of display_set_color_blind()
 */
static void utc_system_deviced_display_set_color_blind_n_2(void)
{
	int ret;

	ret = display_set_color_blind(true, NULL);
	dts_check_ne(API_NAME_DISPLAY_SET_COLOR_BLIND, ret, 0);
}

/**
 * @brief Positive test case of display_get_enhanced_touch()
 */
static void utc_system_deviced_display_get_enhanced_touch_p(void)
{
	int ret;

	ret = display_get_enhanced_touch();
	dts_check_ge(API_NAME_DISPLAY_GET_ENHANCED_TOUCH, ret, 0);
}

/**
 * @brief Positive test case of display_set_enhanced_touch()
 */
static void utc_system_deviced_display_set_enhanced_touch_p_1(void)
{
	int ret;

	ret = display_set_enhanced_touch(true);
	dts_check_eq(API_NAME_DISPLAY_SET_ENHANCED_TOUCH, ret, 0, "Enable enhanced touch");
}

/**
 * @brief Positive test case of display_set_enhanced_touch()
 */
static void utc_system_deviced_display_set_enhanced_touch_p_2(void)
{
	int ret;

	ret = display_set_enhanced_touch(false);
	dts_check_eq(API_NAME_DISPLAY_SET_ENHANCED_TOUCH, ret, 0, "Disable enhanced touch");
}

/**
 * @brief Negative test case of display_set_enhanced_touch()
 */
static void utc_system_deviced_display_set_enhanced_touch_n(void)
{
	int ret;

	ret = display_set_enhanced_touch(-1);
	dts_check_ne(API_NAME_DISPLAY_SET_ENHANCED_TOUCH, ret, 0);
}
