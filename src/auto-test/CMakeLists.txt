CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(deviced-auto-test C)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)

IF("${PROFILE}" STREQUAL "wearable")
	SET(PROFILE wearable)
ELSEIF("${PROFILE}" STREQUAL "tv")
	SET(PROFILE tv)
ELSE()
	SET(PROFILE mobile)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_ENGINEER_MODE")
    OPTION(USE_ENGINEER_MODE "Use Engineer mode" ON)
ENDIF()

SET(SRCS
	test.c
	main.c
	udev.c
	boot.c
	cpu-info.c
	time.c
	result.c
	cool-down.c
	haptic.c
)

# extcon test
SET(SRCS ${SRCS}
	power-supply.c
	storage.c
	usb.c
	hdmi.c
	earjack.c
)

IF(NOT PROFILE STREQUAL wearable)
SET(SRCS ${SRCS}
	cradle.c
	keyboard.c
	tvout.c
)
ENDIF(NOT PROFILE STREQUAL wearable)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED edbus)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed -pie")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DENABLE_TEST_DLOG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} shared)

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/systemd/org.tizen.system.deviced-auto-test.service
		DESTINATION share/dbus-1/system-services)
INSTALL(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/systemd/deviced-auto-test.service
		DESTINATION lib/systemd/system)
