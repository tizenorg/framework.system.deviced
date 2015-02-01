CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(deviced-auto-test C)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)

IF("$ENV{CFLAGS}" MATCHES "-DMICRO_DD")
	OPTION(USE_MICRO_DD "Use Micro DD" ON)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_ENGINEER_MODE")
    OPTION(USE_ENGINEER_MODE "Use Engineer mode" ON)
ENDIF()

SET(SRCS
	test.c
	main.c
	udev.c
	archive.c
	boot.c
	cpu-info.c
	board-info.c
	time.c
	result.c
	cool-down.c
)

# extcon test
SET(SRCS ${SRCS}
	power-supply.c
	storage.c
	usb.c
	hdmi.c
)

IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	cradle.c
	earjack.c
	keyboard.c
	tvout.c
)
ENDIF(NOT USE_MICRO_DD)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED edbus libarchive)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

IF(USE_ENGINEER_MODE)
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")
ELSE()
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer")
ENDIF()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
MESSAGE("FLAGS: ${CMAKE_C_FLAGS}")

IF("$ENV{CFLAGS}" MATCHES "-DMICRO_DD")
	OPTION(USE_MICRO_DD "Use Micro DD" ON)
ENDIF()

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DENABLE_TEST_DLOG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-larchive" shared)

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

ADD_SUBDIRECTORY(activation)
