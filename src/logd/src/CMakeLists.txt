CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(LIB_LOGD "logd")
SET(LIB_LOGD_DB "logd-db")
############### SET FLAGS ########################
SET(COMMON_FLAGS "-DVERSION=\"${VERSION}\" -Wall -Werror -Wextra -Wno-unused-parameter -Wunused-macros -Wformat=2 -Wshadow -Wstrict-overflow=1 -g -fvisibility=hidden -fPIC -Wl,-lrt")

#SET(COMMON_FLAGS "${COMMON_FLAGS} -DUSE_TASKSTATS")

SET(CMAKE_C_FLAGS "-std=gnu99 ${COMMON_FLAGS}")
SET(CMAKE_CXX_FLAGS "-std=gnu++0x ${COMMON_FLAGS}")

IF ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	SET(DEBUG_ENABLED 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-4 -O0")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gdwarf-4 -O0")
	SET(CMAKE_VERBOSE_MAKEFILE ON)
ENDIF()

############### INCLUDE ##########################
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/logd/src/liblogd)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/logd/src/liblogd-db)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/logd/src/shared)

############### SUBDIRS ##########################
ADD_SUBDIRECTORY(shared)
ADD_SUBDIRECTORY(battery)
ADD_SUBDIRECTORY(liblogd-db)
ADD_SUBDIRECTORY(liblogd)
