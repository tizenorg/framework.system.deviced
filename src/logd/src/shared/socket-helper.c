#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/log.h"
#include "socket-helper.h"

int create_logd_socket()
{
	int ret = 0;
	struct sockaddr_un saun;
	int old_umask = umask(~(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
	struct group *group_entry = NULL;
	int len, sock;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		ret = -errno;
		_E("Failed to create socket");
		return ret;
	}

	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, LOGD_SOCKET_PATH);
	unlink(LOGD_SOCKET_PATH);

	len = sizeof(saun.sun_family) + sizeof(saun.sun_path);
	if (bind(sock, (const struct sockaddr*)&saun, len) < 0) {
		ret = -errno;
		_E("Failed bind socket");
		goto err;
	}
	umask(old_umask);

	group_entry = getgrnam("app");
	if (group_entry == NULL) {
		ret = -errno;
		_E("can't find app group id");
		goto err;
	}
	if (chown(LOGD_SOCKET_PATH, -1, group_entry->gr_gid) < 0) {
		ret = -errno;
		_E("can't change group");
		goto err;
	}

	if (listen(sock, 5) < 0) {
		ret = -errno;
		_E("Failed listen socket");
		goto err;
	}

	return sock;
err:
	close(sock);
	return ret;
}

int connect_to_logd_socket()
{
	struct sockaddr_un saun;
	int len, ret;
	int sock;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		ret = -errno;
		_E("Failed to create socket");
		return ret;
	}
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, LOGD_SOCKET_PATH);

	len = sizeof(saun.sun_family) + sizeof(saun.sun_path);
	if (connect(sock, (const struct sockaddr*)&saun, len) < 0) {
		ret = -errno;
		_E("Failed connect");
		close(sock);
		return ret;
	}

	return sock;
}

int read_from_socket(int sock, void *buf, int size)
{
	int i = 0;
	int ret;

	while (i < size) {
		ret = recv(sock, buf + i, size - i, 0);
		if (ret > 0) {
			i += ret;
		} else if (errno != EAGAIN)
			return -errno;

	}
	return i;
}
