CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(deviced C)

########################################################
# Generation options:
# -DBUILD_DOC_ONLY - only doxygen documentation is build
# -DBUILD_DOC - build also doxygen documentation
#
# NOTE:
# Remember to add all directories with files to DOC_SRC_DIRS_IN list
#
########################################################

IF(BUILD_DOC_ONLY)
    SET(BUILD_EXECUTABLE FALSE)
    SET(BUILD_DOC TRUE)
ELSE(BUILD_DOC_ONLY)
    SET(BUILD_EXECUTABLE TRUE)
ENDIF(BUILD_DOC_ONLY)

IF(BUILD_DOC)
    SET( DOC_SRC_DIRS_IN
        ${CMAKE_SOURCE_DIR}/src/deviced
    )
    FOREACH(doc_dir ${DOC_SRC_DIRS_IN})
        SET(DOC_SRC_DIRS "${DOC_SRC_DIRS} ${doc_dir}")
    ENDFOREACH(doc_dir)

    FIND_PACKAGE(Doxygen REQUIRED)

    get_filename_component( DOXYGEN_DOC_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} PATH)

    #adjust the doxygen configuration for this project
    configure_file(${CMAKE_SOURCE_DIR}/doxygen/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile @ONLY)

    #build the documentation
    add_custom_target( doc ALL
        ${DOXYGEN_EXECUTABLE}
        ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile
        WORKING_DIRECTORY ${CMAKE_DOXYGEN_DIRECTORY}
        COMMENT "Generating documentation with Doxygen" VERBATIM
    )

ENDIF(BUILD_DOC)

IF(BUILD_EXECUTABLE)

########################################################
# Build options:
# -DTIZEN_ENGINEER_MODE -
########################################################
IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_ENGINEER_MODE")
	OPTION(USE_ENGINEER_MODE "Use Engineer mode" ON)
ENDIF()

IF("${ARCH}" STREQUAL "emulator")
	OPTION(USE_EMULATOR "Use Emulator" ON)
ELSEIF("${ARCH}" STREQUAL "arm")
	OPTION(USE_ARM "Use Arm" ON)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DSYSTEMD_SHUTDOWN")
	OPTION(USE_SYSTEMD_SHUTDOWN "Use systemd shutdown" ON)
ENDIF()

########################################################
# Deviced Macros
########################################################
MACRO(ADD_SOURCE DIR OUT)
	FILE(GLOB ALL_SRCS "${DIR}/*.c")
	FOREACH(SRC ${ALL_SRCS})
		IF(NOT ${SRC} MATCHES "mobile|wearable|tv|none")
			SET(D_SRCS ${D_SRCS} ${SRC})
		ENDIF()
	ENDFOREACH()
	FILE(GLOB S_SRCS "${DIR}/*-${PROFILE}.c")
	IF(DEFINED S_SRCS)
		SET(D_SRCS ${D_SRCS} ${S_SRCS})
	ENDIF()
	SET(${OUT} ${D_SRCS})
ENDMACRO()

MACRO(INSTALL_CONF DIR CONF)
	SET(T_CONF ${DIR}/${CONF}-${PROFILE}.conf)
	SET(E_CONF ${DIR}/${CONF}-emul-${PROFILE}.conf)
	IF(USE_EMULATOR AND EXISTS ${E_CONF})
		SET(T_CONF ${E_CONF})
	ENDIF()
	IF(NOT EXISTS ${T_CONF})
		SET(T_CONF ${DIR}/${CONF}.conf)
	ENDIF()
	IF(DEFINED T_CONF)
		INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${T_CONF}
			DESTINATION /etc/deviced
			RENAME ${CONF}.conf)
	ENDIF()
ENDMACRO()

########################################################
# Deviced CMakeLists.txt
########################################################
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}/bin")
SET(LIBDIR "${PREFIX}/lib")
SET(INCLUDEDIR "${PREFIX}/include/${PROJECT_NAME}")
SET(DATADIR "${PREFIX}/share/${PROJECT_NAME}")
SET(CONFDIR "/etc/${PROJECT_NAME}")
SET(VERSION 0.1.0)

SET(SRCS
	src/apps/apps.c
	src/control/control.c
	src/core/common.c
	src/core/config-parser.c
	src/core/device-nodes.c
	src/core/device-idler.c
	src/core/device-notifier.c
	src/core/devices.c
	src/core/edbus-handler.c
	src/core/execute.c
	src/core/launch.c
	src/core/log.c
	src/core/main.c
	src/core/sig-handler.c
	src/core/udev.c
	src/core/weaks.c
	src/proc/cpu-info.c
	src/proc/proc-handler.c
	src/sleep/sleep.c
	src/time/time-handler.c
)

IF(BATTERY_MODULE STREQUAL on)
	ADD_SOURCE(src/battery BATTERY_SRCS)
	SET(SRCS ${SRCS} ${BATTERY_SRCS})
ENDIF()

IF(BLOCK_MODULE STREQUAL on)
	ADD_SOURCE(src/block BLOCK_SRCS)
	SET(SRCS ${SRCS} ${BLOCK_SRCS})

	ADD_SOURCE(src/mmc MMC_SRCS)
	SET(SRCS ${SRCS} ${MMC_SRCS})
ENDIF()

IF(BLUETOOTH_MODULE STREQUAL on)
	ADD_SOURCE(src/bluetooth BLUETOOTH_SRCS)
	SET(SRCS ${SRCS} ${BLUETOOTH_SRCS})
ENDIF()

IF(EXTCON_MODULE STREQUAL on)
	ADD_SOURCE(src/extcon EXTCON_SRCS)
	SET(SRCS ${SRCS} ${EXTCON_SRCS})
ENDIF()

IF(HAPTIC_MODULE STREQUAL on)
	ADD_SOURCE(src/haptic HAPTIC_SRCS)
	SET(SRCS ${SRCS} ${HAPTIC_SRCS})
ENDIF()

IF(HDMI_CEC_MODULE STREQUAL on)
	ADD_SOURCE(src/hdmi-cec HDMI_CEC_SRCS)
	SET(SRCS ${SRCS} ${HDMI_CEC_SRCS})
ENDIF()

IF(LED_MODULE STREQUAL on)
	ADD_SOURCE(src/led LED_SRCS)
	SET(SRCS ${SRCS} ${LED_SRCS})
ENDIF()

IF(PASS_MODULE STREQUAL on)
	ADD_SOURCE(src/pass PASS_SRCS)
	SET(SRCS ${SRCS} ${PASS_SRCS})
ENDIF()

IF(PMQOS_MODULE STREQUAL on)
	ADD_SOURCE(src/pmqos PMQOS_SRCS)
	SET(SRCS ${SRCS} ${PMQOS_SRCS})
ENDIF()

IF(POWERSAVER_MODULE STREQUAL on)
	ADD_SOURCE(src/powersaver POWERSAVER_SRCS)
	SET(SRCS ${SRCS} ${POWERSAVER_SRCS})
ENDIF()

IF(STORAGE_MODULE STREQUAL on)
	ADD_SOURCE(src/storage STORAGE_SRCS)
	SET(SRCS ${SRCS} ${STORAGE_SRCS})
ENDIF()

IF(TELEPHONY_MODULE STREQUAL on)
	ADD_SOURCE(src/telephony TELEPHONY_SRCS)
	SET(SRCS ${SRCS} ${TELEPHONY_SRCS})
ENDIF()

IF(TOUCH_MODULE STREQUAL on)
	ADD_SOURCE(src/touch TOUCH_SRCS)
	SET(SRCS ${SRCS} ${TOUCH_SRCS})

	ADD_SOURCE(src/touchscreen TOUCHSCREEN_SRCS)
	SET(SRCS ${SRCS} ${TOUCHSCREEN_SRCS})
ENDIF()

IF(TZIP_MODULE STREQUAL on)
	ADD_SOURCE(src/tzip TZIP_SRCS)
	SET(SRCS ${SRCS} ${TZIP_SRCS})
ENDIF()

IF(POWER_MODULE STREQUAL on)
	IF(USE_SYSTEMD_SHUTDOWN)
		SET(SRCS ${SRCS} src/power/systemd-power.c)
	ELSE()
		SET(SRCS ${SRCS} src/power/power-handler.c)
	ENDIF()
ENDIF()

IF(DISPLAY_MODULE STREQUAL on)
	ADD_SOURCE(src/display DISPLAY_SRCS)
	SET(SRCS ${SRCS} ${DISPLAY_SRCS})
ENDIF()

# usb client
IF(${USB_MODULE} STREQUAL on)
	ADD_SOURCE(src/usb USB_SRCS)
	SET(SRCS ${SRCS} ${USB_SRCS})
ENDIF()

# usb host
IF(${USBHOST_MODULE} STREQUAL on)
	ADD_SOURCE(src/usbhost USBHOST_SRCS)
	SET(SRCS ${SRCS} ${USBHOST_SRCS})
ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)

SET(PKG_MODULES
	ecore
	eina
	edbus
	vconf
	dlog
	libudev
	device-node
	capi-base-common
	gio-2.0
	eventsystem
)

IF(DISPLAY_MODULE STREQUAL on)
	SET(PKG_MODULES ${PKG_MODULES} libinput capi-system-sensor)
ENDIF()
IF(STORAGE_MODULE STREQUAL on)
	SET(PKG_MODULES ${PKG_MODULES} storage)
ENDIF()
IF(TELEPHONY_MODULE STREQUAL on)
	SET(PKG_MODULES ${PKG_MODULES} tapi)
ENDIF()
IF(TZIP_MODULE STREQUAL on)
	SET(PKG_MODULES ${PKG_MODULES} minizip fuse)
ENDIF()

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED ${PKG_MODULES})

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden -Werror")
IF(USE_ENGINEER_MODE)
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")
ELSE()
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer")
ENDIF()
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -lrt -fPIE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")
ADD_DEFINITIONS("-DLIBDIR=\"${LIBDIR}\"")
ADD_DEFINITIONS("-DENABLE_KEY_FILTER")
ADD_DEFINITIONS("-DENABLE_X_LCD_ONOFF")
ADD_DEFINITIONS("-DENABLE_DEVICED_DLOG")
ADD_DEFINITIONS("-DENABLE_LIBDEVICED_DLOG")
ADD_DEFINITIONS("-DPROFILE=\"${PROFILE}\"")
IF(USE_ARM)
	ADD_DEFINITIONS("-DTARGET")
ELSEIF(USE_EMULATOR)
	ADD_DEFINITIONS("-DEMULATOR")
ENDIF()
ADD_DEFINITIONS("-DDEBUG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-lm" shared)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/deviced/ DESTINATION include/${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*_doc.h" EXCLUDE
		PATTERN "*.h")

IF(BATTERY_MODULE STREQUAL on)
	INSTALL_CONF(src/battery battery)
ENDIF()
IF(BLOCK_MODULE STREQUAL on)
	INSTALL_CONF(src/block block)
	INSTALL_CONF(src/mmc mmc)
	INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/mmc-smack-label DESTINATION bin)
ENDIF()
IF(DISPLAY_MODULE STREQUAL on)
	INSTALL_CONF(src/display display)
ENDIF()
IF(HAPTIC_MODULE STREQUAL on)
	INSTALL_CONF(src/haptic haptic)
ENDIF()
IF(LED_MODULE STREQUAL on)
	INSTALL_CONF(src/led led)
ENDIF()
IF(PASS_MODULE STREQUAL on)
	INSTALL_CONF(src/pass pass)
ENDIF()
IF(PMQOS_MODULE STREQUAL on)
	INSTALL_CONF(src/pmqos pmqos)
ENDIF()
IF(POWER_MODULE STREQUAL on)
	INSTALL_CONF(src/power power)
ENDIF()
IF(STORAGE_MODULE STREQUAL on)
	INSTALL_CONF(src/storage storage)
ENDIF()

# USB connection
IF(${USB_MODULE} STREQUAL on)
	INSTALL_CONF(src/usb usb-client-configuration)
	INSTALL_CONF(src/usb usb-client-operation)

	# USB (Manual setting)
	IF(USE_ENGINEER_MODE)
		INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/direct_set_debug.sh  DESTINATION ${EXEC_PREFIX})
		INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/set_usb_debug.sh     DESTINATION ${EXEC_PREFIX})
	ENDIF(USE_ENGINEER_MODE)
ENDIF()

IF(NOT PROFILE STREQUAL wearable)
	INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/movi_format.sh DESTINATION bin)
ENDIF(NOT PROFILE STREQUAL wearable)

CONFIGURE_FILE(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/systemd/ DESTINATION lib/systemd/system
		FILES_MATCHING
		PATTERN "*.service"
		PATTERN "*.socket"
		PATTERN "sdb-prestart.*" EXCLUDE
		PATTERN "org.tizen.*" EXCLUDE)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/packaging/${PROJECT_NAME}.efl DESTINATION /etc/smack/accesses.d)

ADD_SUBDIRECTORY(src/shared)
ADD_SUBDIRECTORY(src/libdeviced)
ADD_SUBDIRECTORY(src/devicectl)
ADD_SUBDIRECTORY(src/auto-test)
IF(BLOCK_MODULE STREQUAL on)
	ADD_SUBDIRECTORY(src/fsck-msdos)
	ADD_SUBDIRECTORY(src/newfs-msdos)
ENDIF()

ENDIF(BUILD_EXECUTABLE)
