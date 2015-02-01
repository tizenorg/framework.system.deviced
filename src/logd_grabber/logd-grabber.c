#include <Ecore.h>
#include <Eina.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"

#include "battery.h"
#include "config.h"
#include "display.h"
#include "event.h"
#include "events.h"
#include "journal-reader.h"
#include "logd-db.h"
#include "logd.h"
#include "macro.h"
#include "mmc.h"
#include "nlproc-stat.h"
#include "socket-helper.h"
#include "task-stats.h"
#include "timer.h"


static int proc_stat_store_period;
static int old_events_rotate_period;
static int old_events_del_period;

static Eina_Hash *sockets;
struct socket_info {
	void (*cb)(void *);
	void *user_data;
};

static int api_socket;
static Ecore_Thread *logd_thread;
static struct timer *proc_stat_store_timer;
static struct timer *old_event_del_timer;
static struct timer *old_proc_power_cons_timer;

static int send_devices_power_cons(int sock)
{
	size_t i;
	int ret;
	struct power_cons {
		enum logd_object object;
		float total;
		float curr;
	} devices_power_cons[2] = {{
		LOGD_DISPLAY,
		get_display_total_power_cons(),
		get_display_curr_power_cons()
	}, {
		LOGD_MMC,
		mmc_power_cons(),
		0
	}};
	const size_t count = ARRAY_SIZE(devices_power_cons);

	if (write(sock, &count, sizeof(count)) != sizeof(count)) {
		ret = -errno;
		_E("write failed: %s", strerror(errno));
		return ret;
	}

	for (i = 0; i < count; ++i) {
		if (write(sock, &devices_power_cons[i], sizeof(devices_power_cons[i])) !=
			sizeof(devices_power_cons[i])) {
			ret = -errno;
			_E("write failed: %s", strerror(errno));
			return ret;
		}
	}

	return 0;
}

static void api_socket_cb(void *user_data)
{
	enum logd_socket_req_type req_type;
	int ns;
	int ret;

	if ((ns = accept(api_socket, NULL, NULL)) < 0) {
		_E("accept failed: %s", strerror(errno));
		return;
	}

	ret = read(ns, (void*)&req_type, sizeof(req_type));
	if (ret != sizeof(req_type)) {
		_E("failed read API request type");
		if (close(ns) < 0) {
			_E("close failed: %s", strerror(errno));
		}
		return;
	}

	if (req_type == LOGD_DEV_STAT_REQ) { /* devices power consumption */
		if (send_devices_power_cons(ns) < 0) {
			_E("send_devices_power_cons failed");
		}
	} else if (req_type == LOGD_PROC_STAT_REQ) {
		if (send_proc_stat(ns) < 0) {
			_E("send_proc_stat failed");
		}
	} else if (req_type == LOGD_EST_TIME_REQ) {
		if (battery_send_estimate_lifetime(ns) < 0) {
			_E("battery_send_estimate_lifetime failed");
		}
	} else if (req_type == LOGD_BATTERY_LVL_REQ) {
		if (battery_send_check_points(ns) < 0) {
			_E("battery_send_check_points failed");
		}
	}

	if (close(ns) < 0) {
		_E("close failed: %s", strerror(errno));
	}
}

static void task_stat_socket_cb(void *user_data)
{
	process_task_stats_answer(get_task_stats_sock(), proc_terminated, NULL);
}

static void proc_events_socket_cb(void *user_data)
{
	process_proc_event_answer(get_proc_events_sock(), proc_forked, NULL);
}

static void proc_stat_store_timer_cb(void *user_data)
{
	if (nlproc_stat_store() < 0) {
		_E("nlproc_stat_store failed");
	}

	_I("per-process statistics stored");
}

static void old_event_del_timer_cb(void *usr_data)
{
	time_t t = time(NULL);

	if (t == (time_t) -1) {
		_E("time failed: %s", strerror(errno));
		return;
	}

	if (delete_old_events(t - old_events_rotate_period) < 0) {
		_E("delete_old_events failed");
	}

	_I("old events deleted");
}

static int sec_till_new_day()
{
	struct tm *tm;
	time_t t;

	time(&t);
	tm = localtime(&t);
	if (tm == NULL) {
		_E("localtime failed");
		return SEC_PER_DAY;
	}

	return SEC_PER_DAY - (tm->tm_sec + tm->tm_min * 60 + tm->tm_hour * 3600);
}

static void old_proc_power_cons_timer_cb(void *user_data)
{
	int time_till_new_day = sec_till_new_day();
	struct timer *timer = (struct timer *)user_data;

	if (delete_old_proc() < 0) {
		_E("delete_old_proc failed");
	}
	_I("old cpu power cons deleted");

	update_timer_exp(timer, time_till_new_day <= 10 ?
		SEC_PER_DAY - 10 + time_till_new_day : time_till_new_day);

	return;
}

static int init_timers()
{
	int ret;
	struct socket_info *socket_info;

	/* Timers */
	proc_stat_store_timer = create_timer(proc_stat_store_period,
		proc_stat_store_timer_cb, NULL);
	if (!proc_stat_store_timer) {
		_E("ecore_timer_add failed");
	} else {
		socket_info = calloc(1, sizeof(struct socket_info));
		if (!socket_info) {
			ret = -errno;
			_E("calloc failed: %s", strerror(errno));
			return ret;
		}
		socket_info->cb = (void(*)(void*))process_timer;
		socket_info->user_data = proc_stat_store_timer;

		if (eina_hash_add(sockets, (void*)&proc_stat_store_timer->fd, socket_info) ==
			EINA_FALSE) {
			_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
			return -ENOMEM;
		}
	}


	old_event_del_timer = create_timer(old_events_del_period,
		old_event_del_timer_cb, NULL);
	if (!old_event_del_timer) {
		_E("ecore_timer_add failed");
	} else {
		socket_info = calloc(1, sizeof(struct socket_info));
		if (!socket_info) {
			ret = -errno;
			_E("calloc failed: %s", strerror(errno));
			return ret;
		}
		socket_info->cb = (void(*)(void*))process_timer;
		socket_info->user_data = old_event_del_timer;

		if (eina_hash_add(sockets, (void*)&old_event_del_timer->fd, socket_info) ==
			EINA_FALSE) {
			_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
			return -ENOMEM;
		}
	}


	old_proc_power_cons_timer = create_timer(sec_till_new_day() - 10,
		old_proc_power_cons_timer_cb, NULL);
	if (!old_proc_power_cons_timer) {
		_E("ecore_timer_add failed");
	} else {
		old_proc_power_cons_timer->user_data = old_proc_power_cons_timer;
		socket_info = calloc(1, sizeof(struct socket_info));
		if (!socket_info) {
			ret = -errno;
			_E("calloc failed: %s", strerror(errno));
			return ret;
		}
		socket_info->cb = (void(*)(void*))process_timer;
		socket_info->user_data = old_proc_power_cons_timer;

		if (eina_hash_add(sockets, (void*)&old_proc_power_cons_timer->fd, socket_info) ==
			EINA_FALSE) {
			_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
			return -ENOMEM;
		}
	}

	return 0;
}

void logd_event_loop(void *user_data, Ecore_Thread *th)
{
	int max_fd = 0;
	fd_set set;
	void *data;
	Eina_Iterator *it;

	if (init_timers() < 0)
		_E("init_timers failed");

	it = eina_hash_iterator_key_new(sockets);
	while (eina_iterator_next(it, &data)) {
		int fd = *(int*)data;
		max_fd = max(max_fd, fd);
	}
	eina_iterator_free(it);

	while (1) {
		FD_ZERO(&set);
		it = eina_hash_iterator_key_new(sockets);
		while (!ecore_thread_check(th) && eina_iterator_next(it, &data)) {
			int fd = *(int*)data;
			FD_SET(fd, &set);
		}
		eina_iterator_free(it);

		if (ecore_thread_check(th))
			break;

		select(max_fd + 1, &set, NULL, NULL, NULL);

		it = eina_hash_iterator_key_new(sockets);
		while (!ecore_thread_check(th) && eina_iterator_next(it, &data)) {
			int fd = *(int*)data;
			struct socket_info *info;

			if (FD_ISSET(fd, &set)) {
				info = eina_hash_find(sockets, &fd);
				if (info) {
					info->cb(info->user_data);
				} else {
					_E("eina_hash_find failed: wrong fd");
				}
			}

		}
		eina_iterator_free(it);
		if (ecore_thread_check(th))
			break;
	}
	_I("loop exited");
}


static int init_sockets()
{
	int ret;
	int event_socket;
	int task_stat_socket;
	int proc_events_socket;
	struct socket_info *socket_info;

	sockets = eina_hash_pointer_new(free);
	if (!sockets) {
		_E("eina_hash_pointer_new failed");
		return -ENOMEM;
	}

	/* event_socket */
	ret = jr_init();
	CHECK_RET(ret, "jr_init");
	event_socket = jr_get_socket();
	if (event_socket <= 0) {
		_E("jr_get_socket returned wrong socket");
		return -ENOTSOCK;
	}

	socket_info = calloc(1, sizeof(struct socket_info));
	if (!socket_info) {
		ret = -errno;
		_E("calloc failed: %s", strerror(errno));
		return ret;
	}
	socket_info->cb = event_socket_cb;

	if (eina_hash_add(sockets, (void*)&event_socket, socket_info) ==
		EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* API socket */
	if ((api_socket = create_logd_socket()) < 0) {
		_E("create_logd_socket failed");
		return api_socket;
	}

	socket_info = calloc(1, sizeof(struct socket_info));
	if (!socket_info) {
		ret = -errno;
		_E("calloc failed: %s", strerror(errno));
		return ret;
	}
	socket_info->cb = api_socket_cb;

	if (eina_hash_add(sockets, (void*)&api_socket, socket_info) ==
		EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* Task stat socket */
	task_stat_socket = get_task_stats_sock();

	socket_info = calloc(1, sizeof(struct socket_info));
	if (!socket_info) {
		ret = -errno;
		_E("calloc failed: %s", strerror(errno));
		return ret;
	}
	socket_info->cb = task_stat_socket_cb;

	if (eina_hash_add(sockets, (void*)&task_stat_socket, socket_info) ==
		EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* Proc events socket */
	proc_events_socket = get_proc_events_sock();

	socket_info = calloc(1, sizeof(struct socket_info));
	if (!socket_info) {
		ret = -errno;
		_E("calloc failed: %s", strerror(errno));
		return ret;
	}
	socket_info->cb = proc_events_socket_cb;

	if (eina_hash_add(sockets, (void*)&proc_events_socket, socket_info) ==
		EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	return 0;
}

static void logd_grabber_exit()
{
	int ret;

	if (!proc_stat_store_timer)
		delete_timer(proc_stat_store_timer);
	if (!old_event_del_timer)
		delete_timer(old_event_del_timer);
	if (!old_proc_power_cons_timer)
		delete_timer(old_proc_power_cons_timer);

	if (sockets) {
		eina_hash_free(sockets);
		sockets = NULL;
	}

	ret = event_exit();
	if (ret < 0) {
		_E("event_exit failed");
	}
}

void logd_loop_cancel(void *user_data, Ecore_Thread *th)
{
	_I("logd_loop_cancel called");
	logd_grabber_exit();
}

static int logd_grabber_init()
{
	int ret = 0;
	const char *enable;

	tzset();

	ret = config_init();
	if (ret < 0) {
		_E("config_init failed");
		return ret;
	}
	enable = config_get_string("enable_grabber", "yes", NULL);
	if (strncmp(enable, "yes", 3)) {
		_I("disable logd_grabber");
		return ret;
	}

	log_init();

	proc_stat_store_period =
		config_get_int("proc_stat_store_period", 30 * 60, NULL);
	old_events_rotate_period =
		config_get_int("old_events_rotate_period", 60 * 60 *24 * 7, NULL);
	old_events_del_period =
		config_get_int("old_events_del_period", 30 * 60, NULL);

	ret = mmc_init();
	if (ret < 0) {
		_E("mmc_init failed");
		return ret;
	}

	ret = display_init();
	if (ret < 0) {
		_E("display_init failed");
		return ret;
	}

	ret = nlproc_stat_init();
	if (ret < 0) {
		_E("nlproc_stat_init failed");
		return ret;
	}

	ret = battery_init();
	if (ret < 0) {
		_E("battery_init failed");
		return ret;
	}

	ret = event_init();
	if (ret < 0) {
		_E("event_init failed");
		return ret;
	}

	ret = init_sockets();
	if (ret < 0) {
		_E("init_sockets failed");
		return ret;
	}

	logd_thread = ecore_thread_run(logd_event_loop,
			NULL, logd_loop_cancel, NULL);

	return 0;
}


static void logd_init()
{
	if (logd_grabber_init() < 0)
		_E("logd_grabber_init failed");
}

static void logd_exit()
{
	if (logd_thread && !ecore_thread_check(logd_thread)) {
		ecore_thread_cancel(logd_thread);
		logd_thread = NULL;
	}
}

static const struct device_ops logd_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "logd",
	.init     = logd_init,
	.exit     = logd_exit,
};

DEVICE_OPS_REGISTER(&logd_device_ops)
