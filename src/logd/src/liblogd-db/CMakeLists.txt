CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(PC_NAME ${LIB_LOGD_DB})
SET(PC_REQUIRED "sqlite3")
SET(PC_PROVIDED_LIBS "-l${LIB_LOGD_DB}")
SET(PC_CFLAGS -I\${includedir}/${LIB_LOGD_DB})

CONFIGURE_FILE(
	lib${LIB_LOGD_DB}.pc.in
	lib${LIB_LOGD_DB}.pc
	@ONLY
)

SET(LIB_SOURCE
	db.c
	devices.c
	events.c
	proc-stat.c
	padvisor.c)

############### SET FLAGS ########################
INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(liblogd_db_pkgs REQUIRED sqlite3 libsystemd-journal dlog)

FOREACH(flag ${liblogd_db_pkgs_LDFLAGS})
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

FOREACH(flag ${liblogd_db_pkgs_CFLAGS})
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

############### TARGET ###########################
ADD_LIBRARY(${LIB_LOGD_DB} SHARED ${LIB_SOURCE})
TARGET_LINK_LIBRARIES(${LIB_LOGD_DB} socket-helper systemd-journal)

############### INSTALL ##########################
INSTALL(TARGETS ${LIB_LOGD_DB} DESTINATION lib)
INSTALL(FILES logd-db.h DESTINATION include/${LIB_LOGD_DB})
INSTALL(FILES padvisor.h DESTINATION include/${LIB_LOGD_DB})
INSTALL(FILES lib${LIB_LOGD_DB}.pc DESTINATION lib/pkgconfig)

############### SUBDIRS ##########################
ADD_SUBDIRECTORY(tests)
