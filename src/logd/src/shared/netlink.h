#ifndef _LOGD_NETLINK_H_
#define _LOGD_NETLINK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/genetlink.h>
/* TODO: replace "logd-taskstats.h" on " <linux/taskstats.h>
 * if the build system will contains kernel-headers from tizen.
 * Now it's necessary due to build system using 2.6.36 kernel and
 * struct taskstats not contains ac_stime_power_cons, ac_utime_power_cons
 * variables.
 */
#include "logd-taskstats.h"
#include <linux/netlink.h>

#define GENLMSG_DATA(glh)	((void *)(NLMSG_DATA(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(glh)	(NLMSG_PAYLOAD(glh, 0) - GENL_HDRLEN)
#define NLA_DATA(na)		((void *)((char*)(na) + NLA_HDRLEN))
#define NLA_PAYLOAD(len)	(len - NLA_HDRLEN)

#define MAX_MSG_SIZE		1024

struct msgtemplate {
	struct nlmsghdr n;
	struct genlmsghdr g;
	char buf[MAX_MSG_SIZE];
};

int create_netlink_socket(int protocol, int groups, int pid);
int send_cmd(int sock, unsigned short family_id, pid_t pid, __u8 cmd,
	 unsigned short nl_type, void *nl_data, int nl_len);
int get_family_id(int sock, const char *n);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_NETLINK_H_ */
