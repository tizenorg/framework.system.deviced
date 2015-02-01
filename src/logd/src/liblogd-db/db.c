#include <sqlite3.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "devices.h"
#include "events.h"
#include "logd-db.h"
#include "macro.h"
#include "padvisor.h"
#include "proc-stat.h"

static sqlite3 *db = 0;
static char *db_file_path = "/opt/dbspace/.logd.db";

__attribute__ ((constructor))
static int db_init(void)
{
	DB_CHECK(sqlite3_open(db_file_path, &db));

	devices_init();
	events_init();
	padvisor_init();
	proc_stat_init();


	return 0;
}

__attribute__ ((destructor))
static int db_finalize(void)
{
	devices_finalize();
	events_finalize();
	padvisor_finalize();
	proc_stat_finalize();

	sqlite3_close(db);

	return 0;
}

sqlite3 *logd_get_db(void)
{
	return db;
}
