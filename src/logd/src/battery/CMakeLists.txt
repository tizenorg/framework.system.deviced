CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(logd_battery)

SET(BATTERY_CALIBRATION_SOURCES
	calibration.c)

SET(POWER_MONITOR_SOURCES
	power-monitor.cpp)

############### SET FLAGS ########################
INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(logd_battery_pkgs REQUIRED vconf libsystemd-journal)

FOREACH(flag ${logd_battery_pkgs_LDFLAGS})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

FOREACH(flag ${logd_battery_pkgs_CFLAGS})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lrt -std=c++0x")

############### TARGET ###########################
ADD_EXECUTABLE(${PROJECT_NAME}_calibration ${BATTERY_CALIBRATION_SOURCES})
ADD_EXECUTABLE(logd_power_monitor ${POWER_MONITOR_SOURCES})
TARGET_LINK_LIBRARIES(logd_power_monitor ${LIB_LOGD_DB} ${LIB_LOGD})

############### INSTALL ##########################
INSTALL(TARGETS ${PROJECT_NAME}_calibration
	DESTINATION bin)
INSTALL(TARGETS logd_power_monitor
	DESTINATION bin)
