#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "core/log.h"
#include "proc-events.h"
#include "netlink.h"

int subscribe_on_proc_events(int sock)
{
	struct {
		struct nlmsghdr n;
		struct __attribute__ ((__packed__)) {
			struct cn_msg cn_msg;
			enum proc_cn_mcast_op cn_mcast;
		};
	} msg;

	bzero(&msg, sizeof(msg));
	msg.n.nlmsg_len = sizeof(msg);
	msg.n.nlmsg_pid = getpid();
	msg.n.nlmsg_type = NLMSG_DONE;

	msg.cn_msg.id.idx = CN_IDX_PROC;
	msg.cn_msg.id.val = CN_VAL_PROC;
	msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);
	msg.cn_mcast = PROC_CN_MCAST_LISTEN;

	if (send(sock, &msg, sizeof(msg), 0) < 0) {
		_E("send failed");
		return -errno;
	}

	return 0;
}


int process_proc_event_answer(int sock,
	int (*process_cb)(struct proc_event *e, void *user_data), void *user_data)
{
	int rc;
	struct {
		struct nlmsghdr nl_hdr;
		struct __attribute__ ((__packed__)) {
			struct cn_msg cn_msg;
			struct proc_event event;
		};
	} msg;

	rc = recv(sock, &msg, sizeof(msg), 0);
	if (rc < 0 && errno != EAGAIN) {
		_E("recv failed");
		return -errno;
	}

	if (rc > 0)
		process_cb(&msg.event, user_data);

	return 0;
}
