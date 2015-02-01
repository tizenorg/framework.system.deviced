#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "task-stats.h"

static short taskstats_family_id = 0;

int reg_task_stats_cpu_mask(int sock, const char *cpu_mask)
{
	char buf[80] = { 0, };
	int rc;
	pid_t self_pid = 0;

	if (taskstats_family_id == 0) {
		taskstats_family_id = get_family_id(sock, TASKSTATS_GENL_NAME);
		if (taskstats_family_id < 0) {
			_E("get_family_id taskstats failed");
			return taskstats_family_id;
		}
	}

	strcpy(buf, cpu_mask);
	self_pid = getpid();
	rc = send_cmd(sock, taskstats_family_id, self_pid, TASKSTATS_CMD_GET,
		TASKSTATS_CMD_ATTR_REGISTER_CPUMASK, &buf, sizeof(buf));
	if (rc < 0) {
		_E("TASKSTATS_CMD_ATTR_REGISTER_CPUMASK failed");
		return rc;
	}

	return sock;
}


int request_stat_by_pid(int sock, pid_t pid)
{
	pid_t self_pid = 0;

	if (taskstats_family_id == 0) {
		taskstats_family_id = get_family_id(sock, TASKSTATS_GENL_NAME);
		if (taskstats_family_id < 0) {
			_E("get_family_id taskstats failed");
			return taskstats_family_id;
		}
	}
	self_pid = getpid();

	return send_cmd(sock, taskstats_family_id, self_pid, TASKSTATS_CMD_GET,
		TASKSTATS_CMD_ATTR_PID, &pid, sizeof(__u32));
}


int process_task_stats_answer(int sock,
	int (*process_cb)(struct taskstats *t, void *user_data), void *user_data)
{
	int repl_len = 0;
	int len = 0, offset = 0;
	struct msgtemplate msg;
	char *buff = (char *)&msg;
	struct nlattr *na;
	int aggr_len;

	errno = 0;
	do {
		len = recv(sock, buff + offset, sizeof(msg) - offset, 0);
		if (len < 0 && errno != EAGAIN) {
			int ret = -errno;
			_E("recv failed");
			return ret;
		}
		if (len > 0)
			offset += len;
	} while (errno == EAGAIN);

	if (msg.n.nlmsg_type == NLMSG_OVERRUN) {
		_E("unexpected netlink message");
		return -1;
	}

	if (msg.n.nlmsg_type == NLMSG_ERROR ||
		!NLMSG_OK((&msg.n), (unsigned int)offset)) {
		struct nlmsgerr *err = NLMSG_DATA(&msg);
		return err->error;
	}

	repl_len = GENLMSG_PAYLOAD(&msg.n);
	na = (struct nlattr *) GENLMSG_DATA(&msg);
	len = 0;

	while (len < repl_len) {
		len += NLA_ALIGN(na->nla_len);

		switch (na->nla_type) {
		case TASKSTATS_TYPE_AGGR_PID:
			aggr_len = NLA_PAYLOAD(na->nla_len);
			offset = 0;

			na = (struct nlattr *) NLA_DATA(na);
			while (offset < aggr_len) {
				switch (na->nla_type) {
				case TASKSTATS_TYPE_STATS:
					if (process_cb((struct taskstats *) NLA_DATA(na),
						user_data) < 0) {
						_E("process_cb failed");
					}
					break;
				default:
					break;
				}
				offset += NLA_ALIGN(na->nla_len);
				na = (struct nlattr *) ((char *) na + offset);
			}
			break;
		default:
			break;
		}
		na = (struct nlattr *) (GENLMSG_DATA(&msg) + len);
	}

	return 0;
}
