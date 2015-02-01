#ifndef _LOGD_EVENTS_H_
#define _LOGD_EVENTS_H_

#include <sqlite3.h>

#include "logd-db.h"

#ifdef __cplusplus
extern "C" {
#endif

int events_init(void);
int events_finalize(void);
int logd_store_app(const char *app);
int logd_load_apps(enum logd_db_query (*cb) (int, const char *, void *),
		void *user_data);
int logd_store_event(int event_type, uint64_t _time, int app_id,
		   const char *message);
int logd_load_events(enum logd_db_query (*cb)(const struct logd_event_info *, void *),
	sqlite3_stmt *stmt, void *user_data);
int delete_old_events(uint64_t min_time);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_EVENTS_H_ */
