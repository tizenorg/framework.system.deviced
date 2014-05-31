/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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


#ifndef __DD_LED_H__
#define __DD_LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @file        dd-led.h
 * @defgroup    CAPI_SYSTEM_DEVICED_LED_MODULE Led
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of led
 * @section CAPI_SYSTEM_DEVICED_LED_MODULE_HEADER Required Header
 *   \#include <dd-led.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_LED_MODULE
 * @{
 */

/**
 * @par Description
 * LED mode
 */
enum LED_MODE {
	LED_OFF = 0,
	LED_LOW_BATTERY,
	LED_CHARGING,
	LED_FULLY_CHARGED,
	LED_CHARGING_ERROR,
	LED_MISSED_NOTI,
	LED_VOICE_RECORDING,
	LED_POWER_OFF,
	LED_CUSTOM,
	LED_MODE_MAX,
};

#define led_set_brightness(val)	\
		led_set_brightness_with_noti(val, false)

#define led_set_mode(mode, on)	\
		led_set_mode_with_color(mode, on, 0)

/**
 * @par Description:
 *  This API is used to get the current brightness of the led.\n
 *  It gets the current brightness of the led by calling device_get_property() function.\n
 *  It returns integer value which is the current brightness on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return current brightness value on success, -1 if failed
 * @see led_set_brightness_with_noti()
 * @par Example
 * @code
 *  ...
 *  int cur_brt;
 *  cur_brt = led_get_brightness();
 *  if( cur_brt < 0 )
 *      printf("Fail to get the current brightness of the led.\n");
 *  else
 *      printf("Current brightness of the led is %d\n", cur_brt);
 *  ...
 * @endcode
 */
int led_get_brightness(void);

/**
 * @par Description:
 *  This API is used to get the max brightness of the led.\n
 *  It gets the max brightness of the led by calling device_get_property() function.\n
 *  It returns integer value which is the max brightness on success.\n
 *  Or a negative value(-1) is returned on failure
 * @return max brightness value on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  int max_brt;
 *  max_brt = led_get_max_brightness();
 *  if( max_brt < 0 )
 *      printf("Fail to get the max brightness of the led.\n");
 *  ...
 * @endcode
 */
int led_get_max_brightness(void);

/**
 * @par Description:
 *  This API is used to set the current brightness of the led.\n
 *      It sets the current brightness of the led by calling device_set_property() function.\n
 * @param[in] val brightness value that you want to set
 * @param[in] enable noti
 * @return 0 on success, -1 if failed
 * @see led_get_brightness()
 * @par Example
 * @code
 *  ...
 *  if( led_set_brightness_with_noti(1, 1) < 0 )
 *     printf("Fail to set the brightness of the led\n");
 *  ...
 * @endcode
 */
int led_set_brightness_with_noti(int val, bool enable);

/**
 * @par Description:
 *  This API is used to set command of the irled.\n
 *  It sets the command to control irled by calling device_set_property() function.\n
 * @param[in] value string that you want to set
 * @return 0 on success, -1 if failed
 * @see led_set_brightness_with_noti()
 * @par Example
 * @code
 *  ...
 *  if( led_set_ir_command(("38000,173,171,24,...,1880") < 0 )
 *     printf("Fail to set the command of the irled\n");
 *  ...
 * @endcode
 */
int led_set_ir_command(char *value);

/**
 * @par Description:
 *  This API is used to set LED mode with color.\n
 *  It sets the command to set LED mode by calling device_set_property() function.\n
 * @param[in] mode LED mode
 * @param[in] on enable/disable LED
 * @param[in] color LED color
 * @return 0 on success, -1 if failed
 * @see LED_MODE
 * @par Example
 * @code
 *  ...
 *  if( led_set_mode_with_color(LED_LOW_BATTERY, 1, 0) < 0 )
 *      printf("Fail to set LED mode with color\n");
 *  ...
 * @endcode
 * @todo describe color
 */
int led_set_mode_with_color(int mode, bool on, unsigned int color);

/**
 * @par Description:
 *  This API is used to set LED mode with all option such as on/off duty and color.\n
 *  It sets the command to set LED mode by calling device_set_property() function.\n
 * @param[in] mode LED mode
 * @param[in] val enable/disable LED
 * @param[in] on  duty value (millisecond)
 * @param[in] off duty value (millisecond)
 * @param[in] color LED color
 * @return 0 on success, -1 if failed
 * @see LED_MODE
 * @par Example
 * @code
 *  ...
 *  if( led_set_mode_with_property(LED_LOW_BATTERY, true, 500, 5000, 0xFFFF0000) < 0 )
 *      printf("Fail to set LED mode with color\n");
 *  ...
 * @endcode
 * @todo describe color
 */
int led_set_mode_with_property(int mode, bool val, int on, int off, unsigned int color);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_LED_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
