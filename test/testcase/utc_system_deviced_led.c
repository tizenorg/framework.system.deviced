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
#include <dd-led.h>

#define API_NAME_LED_GET_BRIGHTNESS "led_get_brightness"
#define API_NAME_LED_GET_MAX_BRIGHTNESS "led_get_max_brightness"
#define API_NAME_LED_SET_BRIGHTNESS_WITH_NOTI "led_set_brightness_with_noti"
#define API_NAME_LED_SET_IR_COMMAND "led_set_ir_command"
#define API_NAME_LED_SET_MODE_WITH_COLOR "led_set_mode_with_color"
#define API_NAME_LED_SET_MODE_WITH_PROPERTY "led_set_mode_with_property"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_led_get_brightness_p(void);
static void utc_system_deviced_led_get_max_brightness_p(void);
static void utc_system_deviced_led_set_brightness_with_noti_p_1(void);
static void utc_system_deviced_led_set_brightness_with_noti_p_2(void);
static void utc_system_deviced_led_set_brightness_with_noti_n(void);
static void utc_system_deviced_led_set_ir_command_p(void);
static void utc_system_deviced_led_set_ir_command_n(void);
static void utc_system_deviced_led_set_mode_with_color_p_1(void);
static void utc_system_deviced_led_set_mode_with_color_p_2(void);
static void utc_system_deviced_led_set_mode_with_color_n(void);
static void utc_system_deviced_led_set_mode_with_property_p_1(void);
static void utc_system_deviced_led_set_mode_with_property_p_2(void);
static void utc_system_deviced_led_set_mode_with_property_p_3(void);
static void utc_system_deviced_led_set_mode_with_property_p_4(void);
static void utc_system_deviced_led_set_mode_with_property_n(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_led_get_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_get_max_brightness_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_brightness_with_noti_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_brightness_with_noti_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_brightness_with_noti_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_led_set_ir_command_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_ir_command_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_color_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_color_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_color_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_property_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_property_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_property_p_3, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_property_p_4, POSITIVE_TC_IDX },
	{ utc_system_deviced_led_set_mode_with_property_n, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of led_get_brightness()
 */
static void utc_system_deviced_led_get_brightness_p(void)
{
	int ret;

	ret = led_get_brightness();
	dts_check_ge(API_NAME_LED_GET_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of led_get_max_brightness()
 */
static void utc_system_deviced_led_get_max_brightness_p(void)
{
	int ret;

	ret = led_get_max_brightness();
	dts_check_ge(API_NAME_LED_GET_MAX_BRIGHTNESS, ret, 0);
}

/**
 * @brief Positive test case of led_set_brightness_with_noti()
 */
static void utc_system_deviced_led_set_brightness_with_noti_p_1(void)
{
	int max, ret;

	max = led_get_max_brightness();

	ret = led_set_brightness_with_noti(max, true);
	dts_check_eq(API_NAME_LED_SET_BRIGHTNESS_WITH_NOTI, ret, 0, "Turn on torchled");
}

/**
 * @brief Positive test case of led_set_brightness_with_noti()
 */
static void utc_system_deviced_led_set_brightness_with_noti_p_2(void)
{
	int ret;

	ret = led_set_brightness_with_noti(0, true);
	dts_check_eq(API_NAME_LED_SET_BRIGHTNESS_WITH_NOTI, ret, 0, "Turn off torchled");
}

/**
 * @brief Negative test case of led_set_brightness_with_noti()
 */
static void utc_system_deviced_led_set_brightness_with_noti_n(void)
{
	int ret;

	ret = led_set_brightness_with_noti(-1, true);
	dts_check_ne(API_NAME_LED_SET_BRIGHTNESS_WITH_NOTI, ret, 0);
}

/**
 * @brief Positive test case of led_set_ir_command()
 */
static void utc_system_deviced_led_set_ir_command_p(void)
{
	int ret;

	ret = led_set_ir_command("38000,173,171,24,1880");
	dts_check_eq(API_NAME_LED_SET_IR_COMMAND, ret, 0);
}

/**
 * @brief Negative test case of led_set_ir_command()
 */
static void utc_system_deviced_led_set_ir_command_n(void)
{
	int ret;

	ret = led_set_ir_command(NULL);
	dts_check_ne(API_NAME_LED_SET_IR_COMMAND, ret, 0);
}

/**
 * @brief Positive test case of led_set_mode_with_color()
 */
static void utc_system_deviced_led_set_mode_with_color_p_1(void)
{
	int ret;

	ret = led_set_mode_with_color(LED_MISSED_NOTI, true, 0xffff00ff);
	dts_check_eq(API_NAME_LED_SET_MODE_WITH_COLOR, ret, 0);
}

/**
 * @brief Positive test case of led_set_mode_with_color()
 */
static void utc_system_deviced_led_set_mode_with_color_p_2(void)
{
	int ret;

	ret = led_set_mode_with_color(LED_MISSED_NOTI, false, 0xffff00ff);
	dts_check_eq(API_NAME_LED_SET_MODE_WITH_COLOR, ret, 0);
}

/**
 * @brief Negative test case of led_set_mode_with_color()
 */
static void utc_system_deviced_led_set_mode_with_color_n(void)
{
	int ret;

	ret = led_set_mode_with_color(-1, true, 0xffff00ff);
	dts_check_ne(API_NAME_LED_SET_MODE_WITH_COLOR, ret, 0);
}

/**
 * @brief Positive test case of led_set_mode_with_property()
 */
static void utc_system_deviced_led_set_mode_with_property_p_1(void)
{
	int ret;

	ret = led_set_mode_with_property(LED_MISSED_NOTI, true, 500, 500, 0xffff00ff);
	dts_check_eq(API_NAME_LED_SET_MODE_WITH_PROPERTY, ret, 0);
}

/**
 * @brief Positive test case of led_set_mode_with_property()
 */
static void utc_system_deviced_led_set_mode_with_property_p_2(void)
{
	int ret;

	ret = led_set_mode_with_property(LED_MISSED_NOTI, false, 500, 500, 0xffff00ff);
	dts_check_eq(API_NAME_LED_SET_MODE_WITH_PROPERTY, ret, 0);
}

/**
 * @brief Positive test case of led_set_mode_with_property()
 */
static void utc_system_deviced_led_set_mode_with_property_p_3(void)
{
	int ret;

	ret = led_set_mode_with_property(LED_MISSED_NOTI, true, -1, 500, 0xffff00ff);
	dts_check_eq(API_NAME_LED_SET_MODE_WITH_PROPERTY, ret, 0);
}

/**
 * @brief Positive test case of led_set_mode_with_property()
 */
static void utc_system_deviced_led_set_mode_with_property_p_4(void)
{
	int ret;

	ret = led_set_mode_with_property(LED_MISSED_NOTI, true, 500, -1, 0xffff00ff);
	dts_check_eq(API_NAME_LED_SET_MODE_WITH_PROPERTY, ret, 0);
}

/**
 * @brief Negative test case of led_set_mode_with_property()
 */
static void utc_system_deviced_led_set_mode_with_property_n(void)
{
	int ret;

	ret = led_set_mode_with_property(-1, true, 500, 500, 0xffff00ff);
	dts_check_ne(API_NAME_LED_SET_MODE_WITH_PROPERTY, ret, 0);
}
