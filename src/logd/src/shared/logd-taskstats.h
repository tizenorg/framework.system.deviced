/* taskstats.h - exporting per-task statistics
 *
 * Copyright (C) Shailabh Nagar, IBM Corp. 2006
 *           (C) Balbir Singh,   IBM Corp. 2006
 *           (C) Jay Lan,        SGI, 2006
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
/*
 * This header was generated from a Linux kernel header
 * "include/linux/taskstats.h", to make information necessary for userspace to
 * call into the kernel available to logd. It contains only constants,
 * structures, and macros generated from the original header, and thus,
 * contains no copyrightable information.
*/
/*
 * Modification: Unused variable is changed to padding variable in taskstats
 * and removed unused enum variable about TASKSTATS.
 * Feb. 02th 2014
 */


#ifndef _LINUX_TASKSTATS_H
#define _LINUX_TASKSTATS_H

#include <linux/types.h>

#define TS_COMM_LEN            32

struct taskstats {
	__u16	version;
	__u32	ac_exitcode;
	__u8	ac_flag;
	__u8	ac_nice;
	__u64	cpu_count __attribute__((aligned(8)));
	__u64	cpu_delay_total;
	__u64	blkio_count;
	__u64	blkio_delay_total;
	__u64	swapin_count;
	__u64	swapin_delay_total;
	__u64	cpu_run_real_total;
	__u64	cpu_run_virtual_total;
	char	ac_comm[TS_COMM_LEN];
	__u8	ac_sched __attribute__((aligned(8)));
	__u8	ac_pad[3];
	__u32	ac_uid __attribute__((aligned(8)));
	__u32	ac_gid;
	__u32	ac_pid;
	__u32	ac_ppid;
	__u32	ac_btime;
	__u64	ac_etime __attribute__((aligned(8)));
	__u64	ac_utime;
	__u64	ac_stime;
	__u64   ac_utime_power_cons;
	__u64   ac_stime_power_cons;
	__u64	extra_pad[20];
};

enum {
	TASKSTATS_CMD_GET = 1,
};

enum {
	TASKSTATS_TYPE_STATS = 3,
	TASKSTATS_TYPE_AGGR_PID = 4,
};

enum {
	TASKSTATS_CMD_ATTR_UNSPEC = 0,
	TASKSTATS_CMD_ATTR_PID = 1,
	TASKSTATS_CMD_ATTR_REGISTER_CPUMASK = 3,
};

#define TASKSTATS_GENL_NAME	"TASKSTATS"

#endif
