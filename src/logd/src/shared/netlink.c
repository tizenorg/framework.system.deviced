#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "netlink.h"

int create_netlink_socket(int protocol, int groups, int pid)
{
	int sock;
	int flags;
	struct sockaddr_nl local;
	int buf_size = 1024*1024;

	sock = socket(AF_NETLINK, SOCK_RAW, protocol);
	if (sock < 0) {
		int ret = -errno;
		_E("socket failed: %s", strerror(errno));
		return ret;
	}
	if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
		int ret = -errno;
		_E("fcntl failed");
		close(sock);
		return ret;
	}

	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
		int ret = -errno;
		_E("fcntl failed: %s", strerror(errno));
		close(sock);
		return ret;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&buf_size, sizeof(buf_size)) < 0)
		return -1;

	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&buf_size, sizeof(buf_size)) < 0)
		return -1;

	bzero(&local, sizeof(struct sockaddr_nl));
	local.nl_family = AF_NETLINK;
	local.nl_groups = groups;
	local.nl_pid = pid;

	if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
		int ret = -errno;
		_E("bind failed: %s", strerror(errno));
		close(sock);
		return ret;
	}

	return sock;
}

int send_cmd(int sock, unsigned short family_id, pid_t pid, __u8 cmd,
	 unsigned short nl_type, void *nl_data, int nl_len)
{
	struct nlattr *attr;
	struct sockaddr_nl addr;
	int ret = 0;
	int len;
	char *buffer;

	struct msgtemplate message;

	message.g.cmd = cmd;
	message.g.version = 1;
	message.n.nlmsg_flags = NLM_F_REQUEST;
	message.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	message.n.nlmsg_pid = pid;
	message.n.nlmsg_seq = 0;
	message.n.nlmsg_type = family_id;

	attr = (struct nlattr *) GENLMSG_DATA(&message);
	attr->nla_type = nl_type;
	attr->nla_len = nl_len + 1 + NLA_HDRLEN;

	memcpy(NLA_DATA(attr), nl_data, nl_len);
	message.n.nlmsg_len += NLMSG_ALIGN(attr->nla_len);

	buffer = (char *) &message;
	len = message.n.nlmsg_len;
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;

	for (; ret < len; buffer += ret, len -= ret) {
		ret = sendto(sock, buffer, len, 0, (struct sockaddr *) &addr,
			sizeof(addr));
		if (ret < 0 && errno != EAGAIN)
			return -errno;
	}

	return 0;
}

int get_family_id(int sock, const char *name)
{
	struct {
		struct nlmsghdr n;
		struct genlmsghdr g;
		char buf[256];
	} ans;

	struct nlattr *na;
	int id = 0, rc;
	int rep_len;

	rc = send_cmd(sock, GENL_ID_CTRL, getpid(), CTRL_CMD_GETFAMILY,
			CTRL_ATTR_FAMILY_NAME, (void *)name, strlen(name) + 1);
	if (rc < 0) {
		_E("send_cmd error");
		return rc;
	}

	rep_len = recv(sock, &ans, sizeof(ans), 0);
	if (rep_len < 0 || ans.n.nlmsg_type == NLMSG_ERROR ||
	    !NLMSG_OK((&ans.n), (unsigned int)rep_len)) {
		_E("recv error");
		return -errno;
	}

	na = (struct nlattr *) GENLMSG_DATA(&ans);
	na = (struct nlattr *) ((char *) na + NLA_ALIGN(na->nla_len));

	if (na->nla_type == CTRL_ATTR_FAMILY_ID) {
		id = *(__u16 *) NLA_DATA(na);
	}

	return id;
}
