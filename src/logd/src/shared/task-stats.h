#ifndef _LOGD_TASK_STATS_H_
#define _LOGD_TASK_STATS_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "netlink.h"

int reg_task_stats_cpu_mask(int sock, const char *cpu_mask);
int process_task_stats_answer(int sock,
	int (*process_cb)(struct taskstats *t, void *user_data), void *user_data);
int request_stat_by_pid(int sock, pid_t pid);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_TASKSTATS_H_ */
