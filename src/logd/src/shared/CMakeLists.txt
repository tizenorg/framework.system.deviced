CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

############### SET FLAGS ########################
INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(shared_pkgs REQUIRED dlog)

FOREACH(flag ${shared_pkgs_LDFLAGS})
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

FOREACH(flag ${shared_pkgs_CFLAGS})
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

############### TARGET ###########################
ADD_LIBRARY(netlink STATIC netlink.c)
ADD_LIBRARY(task-stats STATIC task-stats.c)
ADD_LIBRARY(proc-events STATIC proc-events.c)
ADD_LIBRARY(socket-helper STATIC socket-helper.c)

TARGET_LINK_LIBRARIES(task-stats netlink)
