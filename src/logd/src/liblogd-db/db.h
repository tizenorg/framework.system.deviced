#ifndef _LOGD_DB_H_
#define _LOGD_DB_H_

#include <sqlite3.h>
#include <stdint.h>

#include "core/log.h"
#include "logd.h"
#include "logd-db.h"
#include "padvisor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SQL_SIZE 1000


#define PREPARE_STMT(stmt, sql)						\
	do {								\
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)	\
			!= SQLITE_OK) {					\
			_E("Can't prepare statement");			\
			stmt = 0;					\
			return -1;					\
		}							\
	} while (0);

#define FINALIZE_STMT(stmt)						\
	do {								\
		if (sqlite3_finalize(stmt) != SQLITE_OK) {		\
			_E("Can't finalize prepared statement: %s",	\
			   sqlite3_errmsg(db));				\
			return -1;					\
		}							\
		stmt = 0;						\
	} while (0);

#define DB_CHECK(ret)							\
	do {								\
		if (ret != SQLITE_OK) {					\
			_E("data base error: %s", sqlite3_errmsg(db));	\
			sqlite3_close(db);				\
			return -1;					\
		}							\
	} while (0);

sqlite3 *logd_get_db(void);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_DB_H_ */
