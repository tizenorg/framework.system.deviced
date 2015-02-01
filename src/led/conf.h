/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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


#ifndef __CONF_H__
#define __CONF_H__

enum {
	DURING_LCD_ON = 0x1,
	DURING_LCD_OFF = 0x2,
	DURING_BOTH = DURING_LCD_ON | DURING_LCD_OFF,
};

struct led_mode {
	int mode;
	int state;
	struct led_data {
		int priority;
		int on;
		int off;
		unsigned int color;
		int wave;
		int lcd;
		int duration;
	} data;
};

int get_led_data(void);
void release_led_data(void);
struct led_mode *find_led_data(int mode);
struct led_mode *get_valid_led_data(int lcd_state);
void print_all_data(void);

#endif
