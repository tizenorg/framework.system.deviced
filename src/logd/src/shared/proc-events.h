#ifndef _LOGD_PROC_EVENTS_H_
#define _LOGD_PROC_EVENTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>

int subscribe_on_proc_events(int sock);
int process_proc_event_answer(int sock,
	int (*process_cb)(struct proc_event *e, void *user_data), void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_PROC_EVENTS_H_ */
