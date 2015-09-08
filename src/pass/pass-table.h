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


#ifndef __PASS_TABLE__
#define __PASS_TABLE__

#include "pass.h"

static struct pass_table pass_table_exynos4412[] = {
	{
	/* Low Level */
		/* Maximum constraint for Level */
		.limit_max_freq = 1000000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 800000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1000000,
		.limit_max_cpu = 4,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 800000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
				.nr_running = 200,
				.busy_cpu = 1,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1100000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 900000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
				.nr_running = 200,
				.busy_cpu = 1,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1100000,
		.limit_max_cpu = 4,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 900000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
				.nr_running = 200,
				.busy_cpu = 1,
			},
		},
	}, {
	/* Middle Level */
		/* Maximum constraint for Level */
		.limit_max_freq = 1200000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1100000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1000000,
				.nr_running = 200,
				.busy_cpu = 1,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1200000,
		.limit_max_cpu = 4,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1100000,
				.nr_running =300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1000000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1300000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1200000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1000000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1300000,
		.limit_max_cpu = 4,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1200000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1000000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
	/* High Level */
		/* Maximum constraint for Level */
		.limit_max_freq = 1400000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1300000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1000000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1400000,
		.limit_max_cpu = 4,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1300000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1000000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1500000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1300000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1100000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1500000,
		.limit_max_cpu = 4,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1300000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1100000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1600000,
		.limit_max_cpu = 3,

		/* Level up for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1400000,
				.nr_running = 400,
				.busy_cpu = 3,
			},
		},

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1200000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* Maximum constraint for Level */
		.limit_max_freq = 1600000,
		.limit_max_cpu = -1,

		/* Level down for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 1200000,
				.nr_running = 300,
				.busy_cpu = 3,
			},
		},
	},
};

static struct pass_table pass_table_exynos4412_radiation[] = {
	{
		/* level 0 - 0,0 */
		.limit_max_freq = 1100000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1000000,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
	}, {
		/* level 1 - 0,1 */
		.limit_max_freq = 1100000,
		.limit_max_cpu = 2,

		/* Level up/down/left/right for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1000000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 2 - 0,2 */
		.limit_max_freq = 1100000,
		.limit_max_cpu = 3,

		/* Level up/down/left/right for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1000000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 3 - 0,3 */
		.limit_max_freq = 1100000,
		.limit_max_cpu = 4,

		/* Level up/down/left/right for condition */
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1000000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 4 - 1,0 */
		.limit_max_freq = 1200000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1200000,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
	}, {
		/* level 5 - 1,1 */
		.limit_max_freq = 1200000,
		.limit_max_cpu = 2,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 200000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1200000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 6 - 1,2 */
		.limit_max_freq = 1200000,
		.limit_max_cpu = 3,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 200000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1200000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 7 - 1,3 */
		.limit_max_freq = 1200000,
		.limit_max_cpu = 4,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 200000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1200000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 8 - 2,0 */
		.limit_max_freq = 1400000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1400000,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
	}, {
		/* level 9 - 2,1 */
		.limit_max_freq = 1400000,
		.limit_max_cpu = 2,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1400000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 10 - 2,2 */
		.limit_max_freq = 1400000,
		.limit_max_cpu = 3,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1400000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 11 - 2,3 */
		.limit_max_freq = 1400000,
		.limit_max_cpu = 4,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
			},
		},
		.num_up_cond = 1,
		.up_cond = {
			[0] = {
				.freq = 1400000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 12 - 3,0 */
		.limit_max_freq = 1600000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 800000,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
	}, {
		/* level 13 - 3,1 */
		.limit_max_freq = 1600000,
		.limit_max_cpu = 2,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 800000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 14 - 3,2 */
		.limit_max_freq = 1600000,
		.limit_max_cpu = 3,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 800000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
		.num_right_cond = 1,
		.right_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	}, {
		/* level 15 - 3,3 */
		.limit_max_freq = 1600000,
		.limit_max_cpu = 4,

		/* Level up/down/left/right for condition */
		.num_down_cond = 1,
		.down_cond = {
			[0] = {
				.freq = 800000,
			},
		},
		.num_left_cond = 1,
		.left_cond = {
			[0] = {
				.nr_running = 300,
				.busy_cpu = 2,
			},
		},
	},
};


static struct pass_table pass_table_w_exynos4212[] = {
	{
		/* level 0 - 0,0 */
		.limit_max_freq = 600000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_down_cond	= 0,
		.num_up_cond	= 1,
		.up_cond = {
			[0] = {
				.freq = 600000,
			},
		},
		.num_left_cond	= 0,
		.num_right_cond	= 0,
	}, {
		/* level 1 - 1,0 */
		.limit_max_freq = 700000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_down_cond	= 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
			},
		},
		.num_up_cond	= 1,
		.up_cond = {
			[0] = {
				.freq = 700000,
			},
		},
		.num_left_cond	= 0,
		.num_right_cond	= 0,
	}, {
		/* level 2 - 2,0 */
		.limit_max_freq = 800000,
		.limit_max_cpu = 1,

		/* Level up/down/left/right for condition */
		.num_down_cond	= 1,
		.down_cond = {
			[0] = {
				.freq = 700000,
			},
		},
		.num_up_cond	= 1,
		.up_cond = {
			[0] = {
				.freq = 800000,
				.nr_running = 200,
				.busy_cpu = 1,
			},
		},
		.num_left_cond	= 0,
		.num_right_cond	= 0,
	}, {
		/* level 3 - 0,1 */
		.limit_max_freq = 600000,
		.limit_max_cpu = 2,

		.num_down_cond	= 1,
		.down_cond = {
			[0] = {
				.freq = 500000,
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
		.num_up_cond	= 1,
		.up_cond = {
			[0] = { .freq = 600000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
		.num_left_cond	= 0,
		.num_right_cond	= 0,
	}, {
		/* level 4 - 1,1 */
		.limit_max_freq = 700000,
		.limit_max_cpu = 2,

		.num_down_cond	= 1,
		.down_cond = {
			[0] = {
				.freq = 600000,
				.nr_running = 100,
				.busy_cpu = 1,
			},
		},
		.num_up_cond	= 1,
		.up_cond = {
			[0] = { .freq = 700000,
				.nr_running = 200,
				.busy_cpu = 2,
			},
		},
		.num_left_cond	= 0,
		.num_right_cond	= 0,
	}, {
		/* level 5 - 2,1 */
		.limit_max_freq = 800000,
		.limit_max_cpu = 2,

		.num_down_cond	= 1,
		.down_cond = {
			[0] = {
				.freq = 600000,
				.nr_running = 150,
				.busy_cpu = 1,
			},
		},
		.num_up_cond	= 0,
		.num_left_cond	= 0,
		.num_right_cond	= 0,
	},
};
#endif	/* __PASS_TABLE__ */
