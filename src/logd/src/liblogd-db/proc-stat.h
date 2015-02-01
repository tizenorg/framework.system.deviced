#ifndef _LOGD_PROC_STAT_H_
#define _LOGD_PROC_STAT_H_

#include "logd.h"
#include "logd-db.h"

#ifdef __cplusplus
extern "C" {
#endif

struct proc_power_cons {
	const char *appid;
	uint64_t power_cons;
	int duration;
};

int proc_stat_init(void);
int proc_stat_finalize(void);

int foreach_proc_power_cons(enum logd_db_query (*cb)
	(const struct proc_power_cons *pc, void *user_data), int day, void *user_data);

int update_proc_power_cons(struct proc_power_cons *pc, int day);
int delete_old_power_cons(int day);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_PROC_STAT_H_ */
