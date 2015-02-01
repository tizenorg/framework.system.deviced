#ifndef _LOGD_SOCKET_HELPER_H_
#define _LOGD_SOCKET_HELPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LOGD_SOCKET_PATH "/tmp/logd.socket"

int create_logd_socket();
int connect_to_logd_socket();
int read_from_socket(int sock, void *buf, int size);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_PROC_STAT_H_ */
