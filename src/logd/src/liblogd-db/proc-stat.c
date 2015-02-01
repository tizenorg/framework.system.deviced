#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "proc-stat.h"
#include "macro.h"
#include "socket-helper.h"

#define UPDATE_PROC_STAT_SQL "REPLACE INTO proc_power_cons \
	(appid, power_cons, duration, day) VALUES(?, ?, ?, ?)"
#define LOAD_PROC_STAT_SQL "SELECT * FROM proc_power_cons WHERE day=?"
#define DELETE_OLD_POWER_CONS "DELETE FROM proc_power_cons WHERE day=?"

static sqlite3 *db;
static sqlite3_stmt *update_proc_stat_stmt;
static sqlite3_stmt *load_proc_stat_stmt;
static sqlite3_stmt *delete_old_power_cons_stmt;

API int proc_stat_init(void)
{
	db = logd_get_db();

	PREPARE_STMT(update_proc_stat_stmt, UPDATE_PROC_STAT_SQL);
	PREPARE_STMT(load_proc_stat_stmt, LOAD_PROC_STAT_SQL);
	PREPARE_STMT(delete_old_power_cons_stmt, DELETE_OLD_POWER_CONS);

	return 0;
}

API int proc_stat_finalize(void)
{
	FINALIZE_STMT(load_proc_stat_stmt);
	FINALIZE_STMT(update_proc_stat_stmt);
	FINALIZE_STMT(delete_old_power_cons_stmt);

	return 0;
}

API int foreach_proc_power_cons(enum logd_db_query (*cb)
	(const struct proc_power_cons *pc, void *user_data), int day, void *user_data)
{
	enum logd_db_query res;

	DB_CHECK(sqlite3_reset(load_proc_stat_stmt));
	DB_CHECK(sqlite3_bind_int(load_proc_stat_stmt, 1, day));
	while (sqlite3_step(load_proc_stat_stmt) == SQLITE_ROW) {
		struct proc_power_cons pc;

		pc.appid = strdup((const char*)
			sqlite3_column_text(load_proc_stat_stmt, 0));
		if (!pc.appid) {
			_E("strdup failed");
			return -ENOMEM;
		}
		pc.power_cons = sqlite3_column_int64(load_proc_stat_stmt, 1);
		pc.duration = sqlite3_column_int(load_proc_stat_stmt, 2);
		res = cb(&pc, user_data);
		free((void*)pc.appid);
		if (res == LOGD_DB_QUERY_STOP)
			break;
	}

	return 0;
}


API int logd_foreach_apps_energy_efficiency(enum logd_db_query (*cb)
	(const char *application, float efficiency, void *user_data), void *user_data)
{
	enum logd_db_query res;

	DB_CHECK(sqlite3_reset(load_proc_stat_stmt));
	while (sqlite3_step(load_proc_stat_stmt) == SQLITE_ROW) {
		struct proc_power_cons pc;
		float efficiency = -1;

		pc.appid = strdup((const char*)
			sqlite3_column_text(load_proc_stat_stmt, 0));
		if (!pc.appid) {
			_E("strdup failed");
			return -ENOMEM;
		}
		pc.power_cons = sqlite3_column_int64(load_proc_stat_stmt, 1);
		pc.duration = sqlite3_column_int(load_proc_stat_stmt, 2);
		if (pc.duration >= 2 * 60 * 60) /* 2 hours */
			efficiency = ((float)pc.power_cons) / pc.duration;
		res = cb(pc.appid, efficiency * 60 * 60, user_data);
		free((void*)pc.appid);
		if (res == LOGD_DB_QUERY_STOP)
			break;
	}

	return 0;
}


API int update_proc_power_cons(struct proc_power_cons *pc, int day)
{
	DB_CHECK(sqlite3_reset(update_proc_stat_stmt));
	DB_CHECK(sqlite3_bind_text(update_proc_stat_stmt, 1, pc->appid, -1,
		SQLITE_STATIC));
	DB_CHECK(sqlite3_bind_int64(update_proc_stat_stmt, 2, pc->power_cons));
	DB_CHECK(sqlite3_bind_int(update_proc_stat_stmt, 3, pc->duration));
	DB_CHECK(sqlite3_bind_int(update_proc_stat_stmt, 4, day));

	if (sqlite3_step(update_proc_stat_stmt) != SQLITE_DONE) {
		_E("Failed to record to proc_stat table");
		return -1;
	}

	return 0;
}

API int logd_foreach_proc_stat(enum logd_db_query (*cb)
	(const struct logd_proc_stat *, void *), void *user_data)
{
	int ret = 0;
	int count;
	int sock;
	struct logd_proc_stat proc_stat;
	enum logd_socket_req_type req_type = LOGD_PROC_STAT_REQ;

	if ((sock = connect_to_logd_socket()) < 0) {
		return sock;
	}

	if (write(sock, &req_type, sizeof(req_type)) != sizeof(req_type)) {
		ret = -errno;
		_E("Can'r write req_type");
		goto out;
	}

	if (read_from_socket(sock, &count, sizeof(count)) != sizeof(count)) {
		ret = -errno;
		_E("Can't read count");
		goto out;
	}

	for (int i = 0; i < count; ++i) {
		int length;
		char buf[FILENAME_MAX] = { 0, };
		if (read_from_socket(sock, &length, sizeof(length)) != sizeof(length)) {
			ret = -errno;
			_E("Can't read len");
			goto out;
		}

		if (read_from_socket(sock, buf, length) != length) {
			ret = -errno;
			_E("recv failed");
			goto out;
		}
		proc_stat.application = strdup(buf);
		if (read_from_socket(sock, &proc_stat.utime, sizeof(proc_stat.utime)) !=
			sizeof(proc_stat.utime)) {
			ret = -errno;
			_E("recv failed");
			goto out;
		}

		if (read_from_socket(sock, &proc_stat.stime, sizeof(proc_stat.stime)) !=
			sizeof(proc_stat.stime)) {
			ret = -errno;
			_E("recv failed");
		}

		if (read_from_socket(sock, &proc_stat.utime_power_cons, sizeof(proc_stat.utime_power_cons)) !=
			sizeof(proc_stat.utime_power_cons)) {
			ret = -errno;
			_E("recv failed");
			goto out;
		}

		if (read_from_socket(sock, &proc_stat.stime_power_cons, sizeof(proc_stat.stime_power_cons)) !=
			sizeof(proc_stat.stime_power_cons)) {
			ret = -errno;
			_E("recv failed");
			goto out;
		}

		if (read_from_socket(sock, &proc_stat.is_active, sizeof(proc_stat.is_active)) !=
			sizeof(proc_stat.is_active)) {
			ret = -errno;
			_E("recv failed");
			goto out;
		}

		ret = cb(&proc_stat, user_data);
		free(proc_stat.application);
		if (ret == LOGD_DB_QUERY_STOP) {
			break;
		}
	}

out:
	close(sock);
	return ret;
}

API int delete_old_power_cons(int day)
{
	DB_CHECK(sqlite3_reset(delete_old_power_cons_stmt));
	DB_CHECK(sqlite3_bind_int(delete_old_power_cons_stmt, 1, day));

	if (sqlite3_step(delete_old_power_cons_stmt) != SQLITE_DONE) {
		_E("Failed to record to proc_stat table");
		return -1;
	}

	return 0;
}
