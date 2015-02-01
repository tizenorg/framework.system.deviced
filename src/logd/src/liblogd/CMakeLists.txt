CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(PC_NAME ${LIB_LOGD})
SET(PC_REQUIRED "libsystemd-journal")
SET(PC_PROVIDED_LIBS "-l${LIB_LOGD}")
SET(PC_CFLAGS -I\${includedir}/${LIB_LOGD})

CONFIGURE_FILE(
	liblogd.pc.in
	liblogd.pc
	@ONLY
)

SET(LIB_SOURCE
	logd.c)

############### SET FLAGS ########################
INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(liblogd_pkgs REQUIRED libsystemd-journal dlog)

FOREACH(flag ${liblogd_pkgs_LDFLAGS})
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

FOREACH(flag ${liblogd_pkgs_CFLAGS})
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

############### TARGET ###########################
ADD_LIBRARY(${LIB_LOGD} SHARED ${LIB_SOURCE})
TARGET_LINK_LIBRARIES(${LIB_LOGD} systemd-journal)

############### INSTALL ##########################
INSTALL(TARGETS ${LIB_LOGD} DESTINATION lib)
INSTALL(FILES logd.h DESTINATION include/${LIB_LOGD})
INSTALL(FILES lib${LIB_LOGD}.pc DESTINATION lib/pkgconfig)

############### SUBDIRS ##########################
ADD_SUBDIRECTORY(tests)
