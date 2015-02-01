#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <Eina.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "config.h"
#include "logd-db.h"
#include "logd-taskstats.h"
#include "macro.h"
#include "nlproc-stat.h"
#include "proc-events.h"
#include "proc-stat.h"
#include "task-stats.h"

#define LOGD_INTERNAL_PID 3

struct process_stat {
	int64_t utime, stime;
	float utime_power_cons, stime_power_cons;
	int is_active;
	time_t duration;
};

static uint64_t power_const_per_uah;

static int task_stats_sock;
static int update_task_stats_sock;
static int proc_events_sock;

static Eina_Hash *active_pids;
static Eina_Hash *terminated_stats;
static Eina_Hash *total_stats;

static struct process_stat* find_or_create_statistic(Eina_Hash *table, const char *cmdline);
static int proc_state_update(void);

const char *kernel_threads_name = "[kernel_threads]";

static int day_of_week()
{
	time_t curr_time;
	int ret;
	int day;
	struct tm *curr_tm = NULL;

	curr_time = time(NULL);
	if (curr_time == ((time_t) -1)) {
		_E("time failed: %s", strerror(errno));
		return -EFAULT;
	}
	curr_tm = localtime(&curr_time);
	if (curr_tm == NULL) {
		ret = -errno;
		_E("time failed: %s", strerror(errno));
		return ret;
	}

	return curr_tm->tm_wday;
}

static Eina_Bool sub_active_from_term_cb(const Eina_Hash *hash, const void *key,
	void *data, void *fdata)
{
	struct process_stat *statistic;
	struct process_stat *active_statistic = (struct process_stat *)data;
	char *cmdline = (char*)key;

	statistic = find_or_create_statistic(terminated_stats, cmdline);
	if (!statistic) {
		_E("find_or_create_statistic failed");
		return 1; /* continue to process */
	}

	statistic->utime -= active_statistic->utime;
	statistic->stime -= active_statistic->stime;
	statistic->utime_power_cons -= active_statistic->utime_power_cons;
	statistic->stime_power_cons -= active_statistic->stime_power_cons;
	statistic->duration -= active_statistic->duration;

	return 1; /* continue to process */
}


int delete_old_proc()
{
	int next_day;
	int ret;

	_I("delete_old_proc start");

	if (nlproc_stat_store() < 0)
		_E("nlproc_stat_store failed");

	/* Already stored into database, so have no reason to keep it */
	eina_hash_free_buckets(terminated_stats);

	if (!terminated_stats) {
		ret = 0;
		goto out;
	}

	/* we have to clear next day stat due to run this function at 23:59:50 */
	next_day = (day_of_week() + 1) % DAYS_PER_WEEK; /* next day */
	if (next_day < 0) {
		_E("day_of_week failed");
		ret = next_day;
		goto out;
	}

	ret = delete_old_power_cons(next_day);
	if (ret < 0) {
		_E("delete_old_power_cons failed");
		goto out;
	}
	/* It's need to avoid double counting the same data that was
	already stored into db */
	proc_state_update();
	eina_hash_foreach(total_stats, sub_active_from_term_cb, NULL);
	ret = 0;

out:
	return ret;
}

int get_task_stats_sock(void)
{
	return task_stats_sock;
}

int get_proc_events_sock(void)
{
	return proc_events_sock;
}

static int get_cpu_mask(char **mask)
{
	int cpus_number;
	int ret;

	errno = 0;
	cpus_number = sysconf(_SC_NPROCESSORS_CONF);
	if (errno != 0) {
		ret = -errno;
		_E("sysconf failed: %s", strerror(errno));
		return ret;
	}

	if (asprintf(mask, "0-%d", cpus_number - 1) == -1) {
		ret = -errno;
		_E("asprintf failed: %s", strerror(errno));
		return ret;
	}

	return 0;
}

static int get_pids_from_proc(Eina_List **pids)
{
	struct dirent *entry = NULL;
	DIR *dir = NULL;
	int ret;

	dir = opendir("/proc");
	if (dir == NULL) {
		ret = -errno;
		_E("opendir failed: %s", strerror(errno));
		return ret;
	}

	while ((entry = readdir(dir)) != NULL) {
		const char *p = entry->d_name;
		char buf[30] = { 0, };
		FILE *fp = NULL;
		char state;
		pid_t pid;

		while (isdigit(*p))
			++p;
		if (*p != 0)
			continue;

		pid = atoi(entry->d_name);
		sprintf(buf, "/proc/%d/stat", pid);
		fp = fopen(buf, "r");
		if (!fp) {
			_E("fopen failed %s: %s", buf, strerror(errno));
			continue;
		}

		ret = fscanf(fp, "%*s %*s %c", &state);
		if (ret != 1) {
			_E("fscanf failed: %s", strerror(errno));
		}

		if (fclose(fp) != 0) {
			_E("fclose failed: %s", strerror(errno));
		}

		if (ret == 1 && state != 'Z') {
			*pids = eina_list_append(*pids, (void*)pid);
			if (eina_error_get()) {
				_E("eina_list_append failed: %s", eina_error_msg_get(eina_error_get()));
			}
		}
	}

	if (closedir(dir) < 0) {
		ret = -errno;
		_E("closedir failed: %s", strerror(errno));
		return ret;
	}

	return 0;
}

static int get_cmdline_by_pid(pid_t pid, char *cmdline)
{
	FILE *fp;
	char path[30];
	char buf[PATH_MAX + 1] = { 0, }; /* "+1" for readlink */
	int ret;

	ret = snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	if (ret < 0 || ret == sizeof(buf)) {
		_E("snprintf failed or output was truncated");
		return ret;
	}
	fp = fopen(path, "r");
	if (fp != NULL) {
		if (fscanf(fp, "%s", buf) != 1) {
			buf[0] = '\0';
		}
		if (fclose(fp) != 0) {
			_E("fclose failed: %s", strerror(errno));
		}
		if (buf[0] == '/') {
			strcpy(cmdline, buf);
			return 0;
		}
	}

	bzero(path, sizeof(path));
	ret = snprintf(path, sizeof(path), "/proc/%d/exe", pid);
	if (ret < 0 || ret == sizeof(path)) {
		_E("snprintf failed or output was truncated");
		return ret;
	}

	bzero(buf, sizeof(buf));
	if (readlink(path, buf, sizeof(buf) - 1) < 0) {
		ret = snprintf(path, sizeof(path), "/proc/%d/comm", pid);
		if (ret < 0 || ret == sizeof(path)) {
			_E("snprintf failed or output was truncated");
			return ret;
		}
		if (access(path, R_OK) < 0) {
			ret = -errno;
			return ret;
		}
		strcpy(cmdline, kernel_threads_name);
		return 0;
	}

	strcpy(cmdline, buf);

	return 0;
}

static struct process_stat* find_or_create_statistic(Eina_Hash *table, const char *cmdline)
{
	struct process_stat* statistic =
		eina_hash_find(table, cmdline);

	if (!statistic) {
		statistic = (struct process_stat *)
			calloc(1, sizeof(struct process_stat));
		if (!statistic) {
			_E("calloc failed: %s", strerror(errno));
			return NULL;
		}

		if (eina_hash_add(table, cmdline,
			statistic) == EINA_FALSE) {
			_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
			free((void*)cmdline);
			free(statistic);
			return NULL;
		}
	}

	return statistic;
}

int proc_terminated(struct taskstats *t, void *user_data)
{
	pid_t pid = t->ac_pid;
	uint64_t s_time = t->ac_stime;
	uint64_t u_time = t->ac_utime;
	float u_time_power_cons = (float)t->ac_utime_power_cons / power_const_per_uah;
	float s_time_power_cons = (float)t->ac_stime_power_cons / power_const_per_uah;

	if (t->ac_stime || t->ac_utime) {
		struct process_stat *statistic;
		const char *cmdline = eina_hash_find(active_pids, &pid);

		if (!cmdline)
			return 0;
		statistic = find_or_create_statistic(terminated_stats, cmdline);
		if (!statistic) {
			_E("find_or_create_statistic failed");
			return -ENOMEM;
		}

		statistic->utime += u_time;
		statistic->stime += s_time;
		statistic->utime_power_cons += u_time_power_cons;
		statistic->stime_power_cons += s_time_power_cons;
		statistic->duration += t->ac_etime / USEC_PER_SEC;
	}

	return 0;
}

static enum logd_db_query load_stat_cb(const struct proc_power_cons *pc, void *user_data)
{
	struct process_stat *statistic;
	if (!user_data) {
		_E("Wrong hash table");
		return LOGD_DB_QUERY_STOP;
	}

	statistic = find_or_create_statistic((Eina_Hash *)user_data, pc->appid);
	if (!statistic) {
		_E("find_or_create_statistic failed");
		return -ENOMEM;
	}

	/* TODO: remove useless utime, stime - need only power consumption */
	statistic->utime_power_cons += pc->power_cons;
	statistic->duration += pc->duration;

	return LOGD_DB_QUERY_CONTINUE;
}

int nlproc_stat_init(void)
{
	char *cpus_mask = NULL;
	Eina_List *pids = NULL;
	Eina_List *l;
	int ret;
	int day;
	void *pid;

	power_const_per_uah = config_get_int("power_const_per_uah", 3213253205ll, NULL);

	task_stats_sock = create_netlink_socket(NETLINK_GENERIC, 0, 0);
	if (task_stats_sock < 0) {
		_E("create_netlink_sock failed (task_stats_sock)");
		return task_stats_sock;
	}

	update_task_stats_sock = create_netlink_socket(NETLINK_GENERIC, 0, 0);
	if (update_task_stats_sock < 0) {
		_E("create_netlink_sock failed (update_task_stats_sock)");
		return update_task_stats_sock;
	}

	ret = get_cpu_mask(&cpus_mask);
	if (ret < 0) {
		_E("get_cpu_mask failed");
		return ret;
	}

	ret = reg_task_stats_cpu_mask(task_stats_sock, cpus_mask);
	if (ret < 0) {
		_E("reg_task_stats_cpu_mask failed");
		return ret;
	}
	free(cpus_mask);
	proc_events_sock = create_netlink_socket(NETLINK_CONNECTOR, CN_IDX_PROC, getpid());
	if (proc_events_sock < 0) {
		_E("create_netlink_sock failed (proc_events_sock)");
		return proc_events_sock;
	}

	ret = subscribe_on_proc_events(proc_events_sock);
	if (ret < 0) {
		_E("subscribe_on_proc_events failed");
		return ret;
	}

	ret = get_pids_from_proc(&pids);
	if (ret < 0) {
		_E("get_pids_from_proc failed");
		return ret;
	}

	active_pids = eina_hash_int32_new(free);
	if (!active_pids) {
		_E("eina_hash_pointer_new failed");
		return -ENOMEM;
	}

	EINA_LIST_FOREACH(pids, l, pid) {
		char cmdline[PATH_MAX];
		char *tmp;

		ret = get_cmdline_by_pid((int)pid, cmdline);
		if (ret < 0) {
			continue;
		}

		tmp = strdup(cmdline);
		if (!tmp) {
			_E("strdup failed: %s", strerror(errno));
			continue;
		}
		if (eina_hash_add(active_pids, &pid, tmp) == EINA_FALSE) {
			_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
			return -ENOMEM;
		}
	}

	eina_list_free(pids);

	terminated_stats = eina_hash_string_superfast_new(free);
	if (!terminated_stats) {
		_E("eina_hash_string_superfast_new failed");
		return -ENOMEM;

	}

	day = day_of_week();
	if (day < 0) {
		_E("day_of_week failed");
		day = 0;
	}

	total_stats = eina_hash_string_superfast_new(free);
	if (!total_stats) {
		_E("eina_hash_string_superfast_new failed");
		return -ENOMEM;
	}

	/* ignore statistic before logd started */
	proc_state_update();
	eina_hash_foreach(total_stats, sub_active_from_term_cb, NULL);

	if (foreach_proc_power_cons(load_stat_cb, day, terminated_stats) < 0)
		_E("foreach_proc_power_cons failed");


	return 0;
}

int proc_forked(struct proc_event *e, void *user_data)
{
	int pid;
	char cmdline[PATH_MAX];
	char *old_cmdline;
	int ret;
	char *tmp;

	if (e->what == PROC_EVENT_FORK) {
		pid = e->event_data.fork.child_pid;
	} else if (e->what == PROC_EVENT_EXEC) {
		pid = e->event_data.exec.process_pid;
	} else if (e->what == PROC_EVENT_EXIT) {
		pid = e->event_data.exit.process_pid;
		if (eina_hash_find(active_pids, &pid)) {
			if (eina_hash_del(active_pids, &pid, NULL) == EINA_FALSE) {
				_E("eina_hash_del failed %d", pid);
				return -EINVAL;
			}
		}
		return 0;
	} else
		return 0;

	ret = get_cmdline_by_pid(pid, cmdline);
	if (ret < 0) {
		return ret;
	}

	tmp = strdup(cmdline);
	if (!tmp) {
		_E("strdup failed: %s", strerror(errno));
		return -ENOMEM;
	}

	old_cmdline = eina_hash_set(active_pids, &pid, tmp);
	if (old_cmdline)
		free(old_cmdline);
	else if (eina_error_get()) {
		_E("eina_hash_set failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	return 0;
}

int add_active_proc(struct taskstats *t, void *user_data)
{
	const char *launchpad_cmdline = "/usr/bin/launchpad_preloading_preinitializing_daemon";
	int ret;
	pid_t pid = t->ac_pid;
	uint64_t s_time = t->ac_stime;
	uint64_t u_time = t->ac_utime;
	float u_time_power_cons = (float)t->ac_utime_power_cons / power_const_per_uah;
	float s_time_power_cons = (float)t->ac_stime_power_cons / power_const_per_uah;

	if (t->ac_stime || t->ac_utime) {
		char *cmdline = eina_hash_find(active_pids, &pid);
		struct process_stat *statistic;
		char buf[PATH_MAX];

		if (!cmdline) {
			ret = get_cmdline_by_pid(pid, buf);
			if (ret < 0) {
				return ret;
			}
			cmdline = strdup(buf);
			if (!cmdline) {
				_E("strdup failed: %s", strerror(errno));
				return -ENOMEM;
			}

			if (eina_hash_add(active_pids, &pid, cmdline) == EINA_FALSE) {
				_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
				free((void*)cmdline);
				return -ENOMEM;
			}
		} else {
			if (strcmp(cmdline, launchpad_cmdline) == 0) {
				char *old_cmdline = NULL;
				ret = get_cmdline_by_pid(pid, buf);
				if (ret < 0) {
					return ret;
				}
				cmdline = strdup(buf);
				if (!cmdline) {
					_E("strdup failed: %s", strerror(errno));
					return -ENOMEM;
				}
				old_cmdline = eina_hash_modify(active_pids,
					&pid, cmdline);
				if (!old_cmdline) {
					_E("eina_hash_modify failed: %s", eina_error_msg_get(eina_error_get()));
					return -EINVAL;
				}
				free(old_cmdline);
			}
		}

		statistic = find_or_create_statistic(total_stats, cmdline);
		if (!statistic) {
			_E("find_or_create_statistic failed");
			return -ENOMEM;
		}

		statistic->utime += u_time;
		statistic->stime += s_time;
		statistic->utime_power_cons += u_time_power_cons;
		statistic->stime_power_cons += s_time_power_cons;
		statistic->duration += t->ac_etime / USEC_PER_SEC;
		statistic->is_active = 1;
	}

	return 0;
}

static Eina_Bool update_total_stat_cb(const Eina_Hash *hash, const void *key,
	void *data, void *fdata)
{
	struct process_stat *statistic;
	struct process_stat *new_statistic = (struct process_stat *)data;
	char *cmdline = (char*)key;

	statistic = find_or_create_statistic(total_stats, cmdline);
	if (!statistic) {
		_E("find_or_create_statistic failed");
		return 1; /* continue to process */
	}

	statistic->utime += new_statistic->utime;
	statistic->stime += new_statistic->stime;
	statistic->utime_power_cons += new_statistic->utime_power_cons;
	statistic->stime_power_cons += new_statistic->stime_power_cons;
	statistic->duration += new_statistic->duration;

	return 1; /* continue to process */
}

Eina_Bool update_active_pids_cb(const Eina_Hash *hash, const void *key,
	void *data, void *fdata)
{
	int pid = *(int*)key;
	int ret;

	ret = request_stat_by_pid(update_task_stats_sock, (int)pid);
	if (ret < 0) {
		_E("request_stat_by_pid failed (pid=%d)", (int)pid);
		return 1; /* continue to process */
	}
	ret = process_task_stats_answer(update_task_stats_sock,
		add_active_proc, NULL);
	if (ret < 0) {
		_E("process_task_stats_answer failed (pid=%d)", (int)pid);
		return 1; /* continue to process */
	}

	return 1; /* continue to process */
}

static int proc_state_update(void)
{
	eina_hash_free_buckets(total_stats);

	eina_hash_foreach(active_pids, update_active_pids_cb, NULL);
	eina_hash_foreach(terminated_stats, update_total_stat_cb, NULL);

	return 0;
}

static int send_one_stat(const struct logd_proc_stat *statistics, int sock)
{
	const char *cmdline = (const char *)statistics->application;
	int len = strlen(cmdline);
	int ret;

	if (send(sock, (void*)&len, sizeof(len), 0) < 0) {
		goto err;
	}
	if (send(sock, cmdline, len, 0) < 0) {
		goto err;
	}
	if (send(sock, &statistics->utime, sizeof(statistics->utime), 0) < 0) {
		goto err;
	}
	if (send(sock, &statistics->stime, sizeof(statistics->stime), 0) < 0) {
		goto err;
	}
	if (send(sock, &statistics->utime_power_cons,
		sizeof(statistics->utime_power_cons), 0) < 0) {
		goto err;
	}
	if (send(sock, &statistics->stime_power_cons,
		sizeof(statistics->stime_power_cons), 0) < 0) {
		goto err;
	}
	if (send(sock, &statistics->is_active,
		sizeof(statistics->is_active), 0) < 0) {
		goto err;
	}

	return 0;
err:
	ret = -errno;
	if (errno != EPIPE)
		_E("send failed: %s", strerror(errno));
	return ret;
}

static int sort_stat_cb(void *d1, void *d2)
{
	const struct logd_proc_stat *s1 = d1;
	const struct logd_proc_stat *s2 = d2;

	if(!d1) return(1);
	if(!d2) return(-1);

	if (s1->utime_power_cons + s1->stime_power_cons <
		s2->utime_power_cons + s2->stime_power_cons)
		return 1;
	else if (s1->utime_power_cons + s1->stime_power_cons >
		s2->utime_power_cons + s2->stime_power_cons)
		return -1;
	return 0;
}

int send_proc_stat(int sock)
{
	int count = eina_hash_population(total_stats);
	int day = day_of_week();
	Eina_List *sorted_stat = NULL;
	Eina_List *l;
	Eina_Iterator *it;
	void *data;
	int ret;
	int i;

	ret = proc_state_update();
	if (ret < 0) {
		_E("proc_stat_update failed");
		return ret;
	}

	for (i = 0; i < DAYS_PER_WEEK; ++i) {
		/* stat for current day stored in activ/terminated hash tables */
		if (i == day)
			continue;
		if (foreach_proc_power_cons(load_stat_cb, i, total_stats) < 0)
			_E("foreach_proc_power_cons failed");
	}

	it = eina_hash_iterator_tuple_new(total_stats);
	if (!it) {
		_E("eina_hash_iterator_tuple_new failed");
		return -ENOMEM;
	}

	while (eina_iterator_next(it, &data)) {
		Eina_Hash_Tuple *t = data;
		char *cmdline = (char*)t->key;
		const struct process_stat *statistic = t->data;
		struct logd_proc_stat *ps =
			(struct logd_proc_stat*) malloc(sizeof(struct logd_proc_stat));

		if (!ps) {
			ret = -ENOMEM;
			_E("malloc failed: %s", strerror(errno));
			goto out_free;
		}
		ps->application = cmdline;
		ps->utime = statistic->utime;
		ps->stime = statistic->stime;
		ps->utime_power_cons = statistic->utime_power_cons;
		ps->stime_power_cons = statistic->stime_power_cons;
		ps->is_active = statistic->is_active;

		sorted_stat = eina_list_sorted_insert(sorted_stat,
			EINA_COMPARE_CB(sort_stat_cb), ps);
		if (eina_error_get()) {
			_E("eina_list_sorted_insert failed: %s", eina_error_msg_get(eina_error_get()));
		}

	}

	if (send(sock, &count, sizeof(count), 0) < 0) {
		ret = -errno;
		_E("send failed: %s", strerror(errno));
		goto out_free;
	}

	EINA_LIST_FOREACH(sorted_stat, l, data) {
		ret = send_one_stat(data, sock);
		if (ret < 0) {
			if (ret != -EPIPE)
				_E("send_one_stat failed");
			break;
		}
	}

	EINA_LIST_FOREACH(sorted_stat, l, data) {
		free(data);
	}

	eina_iterator_free(it);

	eina_list_free(sorted_stat);

	return 0;

out_free:
	if (sorted_stat) {
		EINA_LIST_FOREACH(sorted_stat, l, data) {
			free(data);
		}
	}

	if (it)
		eina_iterator_free(it);

	if (sorted_stat)
		eina_list_free(sorted_stat);
	return ret;
}

int nlproc_stat_store(void)
{
	Eina_Iterator *it;
	void *data;
	int ret;
	int day;

	_I("nlproc_stat_store start");

	ret = proc_state_update();
	if (ret < 0) {
		_E("proc_state_update failed");
		goto out;
	}

	it = eina_hash_iterator_tuple_new(total_stats);
	if (!it) {
		_E("eina_hash_iterator_tuple_new failed");
		ret = -ENOMEM;
		goto out;
	}

	day = day_of_week();
	if (day < 0) {
		_E("day_of_week failed");
		day = 0;
	}

	while (eina_iterator_next(it, &data)) {
		struct proc_power_cons pc;
		Eina_Hash_Tuple *t = data;
		const char *cmdline = t->key;
		const struct process_stat *statistic = t->data;

		pc.appid = cmdline;
		pc.power_cons = statistic->stime_power_cons + statistic->utime_power_cons;
		pc.duration = statistic->duration;

		update_proc_power_cons(&pc, day);
	}
	eina_iterator_free(it);
	ret = 0;

out:
	return ret;
}

int nlproc_stat_exit()
{
	if (active_pids)
		eina_hash_free(active_pids);
	/* TODO: free terminated keys */

	return 0;
}
