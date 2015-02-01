#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "events.h"
#include "macro.h"

#define LOAD_APPS_SQL "SELECT * FROM applications;"
#define STORE_APP_SQL "INSERT INTO applications (name) VALUES (?);"
#define STORE_EVENT_SQL "INSERT INTO events\
	(object, action, time_stamp, app_id, info) VALUES (?, ?, ?, ?, ?);"
#define LOAD_EVENTS_SQL "SELECT E.object, E.action, E.time_stamp, A.name, E.info\
	FROM applications A, events E					\
	WHERE E.app_id=A.id"
#define DEL_OLD_EVENTS_SQL "DELETE FROM events WHERE time_stamp < ?"


#define LOAD_EVENTS_ALL "SELECT E.object, E.action, E.time_stamp, A.name, E.info\
	FROM applications A, events E\
	WHERE E.app_id=A.id"
#define AND " AND"
#define OR " OR"
#define TIMESTAMP_LT " (time_stamp < %lld)"
#define TIMESTAMP_GT " (time_stamp > %lld)"
#define BY_OBJECT " (object = %d)"
#define BY_ACTION " (action = %d)"
#define EXTRA (32)

#define QUERY_BASE_STR_LEN sizeof(LOAD_EVENTS_ALL)
#define QUERY_ADD_STR_TIME_LEN sizeof(TIMESTAMP_LT) + sizeof(AND) \
	+ sizeof(TIMESTAMP_GT) + EXTRA
#define QUERY_ADD_STR_LEN_MAX sizeof(BY_OBJECT) + sizeof(AND) + EXTRA

static sqlite3 *db;

static sqlite3_stmt *load_apps_stmt;
static sqlite3_stmt *store_app_stmt;
static sqlite3_stmt *store_event_stmt;
static sqlite3_stmt *load_events_stmt;
static sqlite3_stmt *del_old_events_stmt;

int events_init(void)
{
	db = logd_get_db();

	PREPARE_STMT(load_apps_stmt, LOAD_APPS_SQL);
	PREPARE_STMT(store_app_stmt, STORE_APP_SQL);
	PREPARE_STMT(store_event_stmt, STORE_EVENT_SQL);
	PREPARE_STMT(load_events_stmt, LOAD_EVENTS_SQL);
	PREPARE_STMT(del_old_events_stmt, DEL_OLD_EVENTS_SQL);

	return 0;
}

int events_finalize(void)
{
	FINALIZE_STMT(load_apps_stmt);
	FINALIZE_STMT(store_app_stmt);
	FINALIZE_STMT(store_event_stmt);
	FINALIZE_STMT(load_events_stmt);
	FINALIZE_STMT(del_old_events_stmt);

	return 0;
}

API int logd_store_app(const char *app)
{
	DB_CHECK(sqlite3_reset(store_app_stmt));
	DB_CHECK(sqlite3_bind_text(store_app_stmt, 1, app, -1, SQLITE_STATIC));
	if (sqlite3_step(store_app_stmt) != SQLITE_DONE) {
		_E("Failed to record to applications table: %s",
		   sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

API int logd_load_apps(enum logd_db_query (*cb) (int, const char *, void *),
		void *user_data)
{
	const char *app = NULL;
	int id;

	while (sqlite3_step(load_apps_stmt) == SQLITE_ROW) {
		app = (const char *)sqlite3_column_text(load_apps_stmt, 0);
		id = sqlite3_column_int(load_apps_stmt, 1);
		if (!cb(id, app, user_data))
			break;
	}

	return 0;
}

API int logd_store_event(int event_type, uint64_t _time, int app_id,
		   const char *message)
{
	DB_CHECK(sqlite3_reset(store_event_stmt));
	DB_CHECK(sqlite3_bind_int(store_event_stmt, 1, event_type & 0xffff));
	DB_CHECK(sqlite3_bind_int(store_event_stmt, 2, event_type >> 16));
	DB_CHECK(sqlite3_bind_int64(store_event_stmt, 3, _time));
	DB_CHECK(sqlite3_bind_int(store_event_stmt, 4, app_id));
	DB_CHECK(sqlite3_bind_text(store_event_stmt, 5, message, -1,
		 SQLITE_STATIC));

	if (sqlite3_step(store_event_stmt) != SQLITE_DONE) {
		_E("Failed to record to events table: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

API int logd_load_events(enum logd_db_query (*cb)(const struct logd_event_info *, void *),
	sqlite3_stmt *stmt, void *user_data)
{
	struct logd_event_info event;
	int ret = 0;

	stmt = stmt ? stmt : load_events_stmt;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		event.object = sqlite3_column_int(stmt, 0);
		event.action = sqlite3_column_int(stmt, 1);
		event.time = sqlite3_column_int64(stmt, 2);

		event.application =
			strdup((char *)sqlite3_column_text(stmt, 3));
		event.message =
			strdup((char *)sqlite3_column_text(stmt, 4));

		ret = cb(&event, user_data);
		free(event.message);
		free(event.application);
		if (ret != LOGD_DB_QUERY_CONTINUE)
			break;
	}

	return 0;
}


API int delete_old_events(uint64_t min_time)
{
	DB_CHECK(sqlite3_reset(del_old_events_stmt));
	DB_CHECK(sqlite3_bind_int64(del_old_events_stmt, 1, min_time));

	if (sqlite3_step(del_old_events_stmt) != SQLITE_DONE) {
		_E("Failed to remove old events: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

API int logd_foreach_events(const struct logd_events_filter *filter,
	enum logd_db_query (*cb)(const struct logd_event_info *, void *),
	void *user_data)
{
	char *buf, *qbuf;
	int idx, omidx, amidx, ret, len, firstskip;
	sqlite3_stmt *stmt;

	if (filter == NULL) {
		len = QUERY_BASE_STR_LEN;

		buf = (char *)malloc(len);
		if (!buf) {
			ret = -ENOMEM;
			goto out_finish;
		}
		len -= snprintf(buf, len, LOAD_EVENTS_ALL);
		if (len < 0) {
			ret = len;
			goto out_free;
		}

	} else {
		omidx = 0;
		for (idx = 0; idx < LOGD_OBJECT_MAX; idx++) {
			if (filter->objects_mask[idx] == 1)
				omidx++;
		}
		amidx = 0;
		for (idx = 0; idx < LOGD_ACTION_MAX; idx++) {
			if (filter->actions_mask[idx] == 1)
				amidx++;
		}

		len = QUERY_BASE_STR_LEN + sizeof(AND) +
			QUERY_ADD_STR_TIME_LEN +
			(omidx * QUERY_ADD_STR_LEN_MAX) +
			(amidx * QUERY_ADD_STR_LEN_MAX);

		buf = (char *)malloc(len);
		if (!buf) {
			ret = -ENOMEM;
			goto out_finish;
		}
		len -= snprintf(buf, len, LOAD_EVENTS_ALL);
		if (len < 0) {
			ret = len;
			goto out_free;
		}

		if (filter->from) {
			strcat(buf, AND);
			ret = asprintf(&qbuf, TIMESTAMP_GT, filter->from);
			if (ret <=0)
				goto out_free;
			strcat(buf, qbuf);
			free(qbuf);
		}

		if (filter->to) {
			strcat(buf, AND);
			ret = asprintf(&qbuf, TIMESTAMP_LT, filter->to);
			if (ret <=0)
				goto out_free;
			strcat(buf, qbuf);
			free(qbuf);
		}

		if (omidx) {
			strcat(buf, AND);
			strcat(buf, " (");
			firstskip = 0;
			for (idx = 0; idx < LOGD_OBJECT_MAX; idx++) {
				if (filter->objects_mask[idx] == 1) {
					if (firstskip != 0)
						strcat(buf, OR);
					else
						firstskip = 1;
					ret = asprintf(&qbuf, BY_OBJECT, idx);
					if (ret <= 0)
						goto out_free;
					strncat(buf, qbuf, ret);
					free(qbuf);
				}
			}
			strcat(buf, ")");
		}
		if (amidx) {
			strcat(buf, AND);
			strcat(buf, " (");
			firstskip = 0;
			for (idx = 0; idx < LOGD_ACTION_MAX; idx++) {
				if (filter->actions_mask[idx] == 1) {
					if (firstskip != 0)
						strcat(buf, OR);
					else
						firstskip = 1;
					ret = asprintf(&qbuf, BY_ACTION, idx);
					if (ret <= 0)
						goto out_free;
					strncat(buf, qbuf, ret);
					free(qbuf);
				}
			}
			strcat(buf, ")");
		}
	}

	PREPARE_STMT(stmt, buf);
	free(buf);
	ret = logd_load_events(cb, stmt, user_data);
	FINALIZE_STMT(stmt);

	return ret;

out_free:
	free(buf);
out_finish:
	return ret;
}
