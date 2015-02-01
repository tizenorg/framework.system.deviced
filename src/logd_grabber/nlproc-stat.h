#ifndef __NLPROC_STAT_H__
#define __NLPROC_STAT_H__

#include "logd-taskstats.h"
#include "proc-events.h"

int nlproc_stat_init(void);
int get_task_stats_sock(void);
int get_proc_events_sock(void);
int nlproc_stat_exit(void);

int proc_forked(struct proc_event *e, void *user_data);
int proc_terminated(struct taskstats *t, void *user_data);

int send_proc_stat(int socket);
int nlproc_stat_store(void);
int delete_old_proc();

#endif /* __PROC_STAT_H__ */
