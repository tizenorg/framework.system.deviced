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
# -DMICRO_DD - for tizenw project
# -DTIZEN_ENGINEER_MODE - 
########################################################
IF("$ENV{CFLAGS}" MATCHES "-DMICRO_DD")
	OPTION(USE_MICRO_DD "Use Micro DD" ON)
ENDIF()

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
# Deviced CMakeLists.txt
########################################################
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}/bin")
SET(LIBDIR "${PREFIX}/lib")
SET(INCLUDEDIR "${PREFIX}/include/${PROJECT_NAME}")
SET(DATADIR "${PREFIX}/share/${PROJECT_NAME}")
SET(VERSION 0.1.0)

SET(SRCS
	src/battery/config.c
	src/battery/lowbat-handler.c
	src/battery/battery.c
	src/core/device-notifier.c
	src/core/main.c
	src/core/sysnoti.c
	src/core/launch.c
	src/core/queue.c
	src/core/core.c
	src/core/devices.c
	src/core/sig-handler.c
	src/core/device-change-handler.c
	src/core/predefine.c
	src/core/common.c
	src/core/config-parser.c
	src/core/execute.c
	src/core/edbus-handler.c
	src/core/power-supply.c
	src/core/lowstorage.c
	src/proc/cpu-info.c
	src/board/board-info.c
	src/proc/proc-handler.c
	src/ta/ta-handler.c
	src/time/time-handler.c
	src/ticker/ticker.c
	src/testmode/testmode.c
)

IF(USE_SYSTEMD_SHUTDOWN)
SET(SRCS ${SRCS}
	src/power/systemd-power.c
)
ELSE()
SET(SRCS ${SRCS}
	src/power/power-handler.c
)
ENDIF()

IF(NOT USE_MICRO_DD)
    IF(USE_EMULATOR)
        ADD_DEFINITIONS("-DMOBILE_EMULATOR")
        SET(SRCS ${SRCS}
            src/core/noti.c
        )
    ENDIF(USE_EMULATOR)
ENDIF(NOT USE_MICRO_DD)

SET(SRCS ${SRCS}
	src/apps/apps.c
	src/apps/lowbat.c
	src/apps/poweroff.c
	src/pmqos/pmqos.c
	src/pmqos/pmqos-plugin.c
)

IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/battery/battery-time.c
	src/extcon/extcon.c
	src/cpu/cpu-handler.c
	src/hall/hall-handler.c
	src/proc/pmon-handler.c
	src/apps/launch.c
	src/apps/lowmem.c
	src/apps/mmc.c
	src/apps/usb.c
	src/apps/usbotg.c
	src/apps/system.c
)

IF(NOT USE_SYSTEMD_SHUTDOWN)
SET(SRCS ${SRCS}
	src/telephony/telephony.c
)
ENDIF()

SET(SRCS ${SRCS}
	src/mmc/mmc-handler.c
	src/mmc/uevent.c
	src/mmc/config.c
	src/mmc/cprm.c
	src/mmc/app2ext.c
)

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARC")
IF(USE_EMULATOR)
SET(SRCS ${SRCS}
	src/mmc/ext4.c
	)
ENDIF(USE_EMULATOR)

SET(SRCS ${SRCS}
	src/mmc/vfat.c
)
ENDIF(NOT USE_MICRO_DD)

# display(pm)
SET(SRCS ${SRCS}
	src/display/core.c
	src/display/display-dbus.c
	src/display/hbm.c
	src/display/device-interface.c
	src/display/lock-detector.c
	src/display/poll.c
	src/display/setting.c
	src/display/display-ops.c
)

IF(USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/display/alpm.c
	src/display/key-filter-micro.c
)
ELSE()
SET(SRCS ${SRCS}
	src/display/brightness.c
	src/display/key-filter.c
	src/display/enhance.c
	src/display/auto-brightness.c
	src/display/smartstay.c
)
ENDIF(USE_MICRO_DD)

SET(SRCS ${SRCS}
	src/led/ir.c
	src/led/torch.c
	src/led/pattern.c
	src/led/noti.c
	src/led/xml.c
)

IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/touch/touch.c
	src/touch/touch-controller.c
	src/touch/touch-plugin.c)


SET(SRCS ${SRCS}
	src/led/rgb.c
)
ENDIF(NOT USE_MICRO_DD)

SET(SRCS ${SRCS}
	src/control/control.c
)

SET(SRCS ${SRCS}
	src/haptic/haptic.c
	src/haptic/standard.c
	src/haptic/external.c
	src/haptic/emulator.c)

# usb client
SET(SRCS ${SRCS}
	src/usb/usb-common.c
	src/usb/usb-client.c
	src/usb/usb-client-xml.c
	src/usb/usb-client-set.c
	src/usb/usb-client-control.c
	src/usb/usb-client-dbus.c
)

# usb host
IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/usb/usb-common.c
	src/usb/usb-host.c
	src/usb/usb-host-dbus.c
	src/usb/usb-host-storage.c
	src/usb/usb-host-storage-vfat.c
	src/usb/usb-host-hid.c
	src/usb/usb-host-camera.c
	src/usb/usb-host-printer.c
	src/usb/usb-host-naming.c
)
ENDIF(NOT USE_MICRO_DD)

# powersaver mode
IF(USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/powersaver/powersaver.c
)
ENDIF(USE_MICRO_DD)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/logd/src/shared)

SET(PKG_MODULES
	ecore
	ecore-file
	ecore-x
	edbus
	eina
	vconf
	dlog
	device-node
	libxml-2.0
	capi-base-common
)

IF(NOT USE_MICRO_DD)
SET(PKG_MODULES ${PKG_MODULES}
	tapi
	modem
	sensor
)
ENDIF()

IF(USE_TRUSTZONE_QUALCOMM)
SET(PKG_MODULES ${PKG_MODULES}
	tee-qsee
)
ENDIF()

IF(NOT USE_MICRO_DD)
    IF(USE_EMULATOR)
        SET(PKG_MODULES ${PKG_MODULES}
            heynoti
        )
    ENDIF(USE_EMULATOR)
ENDIF(NOT USE_MICRO_DD)
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
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -lrt")
MESSAGE("FLAGS: ${CMAKE_C_FLAGS}")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")
ADD_DEFINITIONS("-DLIBDIR=\"${LIBDIR}\"")
ADD_DEFINITIONS("-DENABLE_KEY_FILTER")
ADD_DEFINITIONS("-DENABLE_X_LCD_ONOFF")
ADD_DEFINITIONS("-DENABLE_DEVICED_DLOG")
ADD_DEFINITIONS("-DENABLE_LIBDEVICED_DLOG")
ADD_DEFINITIONS("-DENABLE_PM_LOG")
IF(USE_ARM)
	ADD_DEFINITIONS("-DTARGET")
ELSEIF(USE_EMULATOR)
	ADD_DEFINITIONS("-DEMULATOR")
ENDIF()
ADD_DEFINITIONS("-DDEBUG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-lm" "-ludev" "-ledbus" shared)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/deviced/ DESTINATION include/${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*_doc.h" EXCLUDE
		PATTERN "*.h")

CONFIGURE_FILE(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)

IF(USE_ARM AND NOT USE_MICRO_DD)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/haptic/HW_touch_30ms_sharp.ivt DESTINATION ${DATADIR})
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/led/led.xml DESTINATION ${DATADIR})
ENDIF(USE_ARM AND NOT USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dump_pm.sh DESTINATION /opt/etc/dump.d/module.d)

IF(USE_TRUSTZONE_QUALCOMM)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-msm.conf DESTINATION /etc/deviced RENAME display.conf)
ELSEIF(USE_EMULATOR)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-emul.conf DESTINATION /etc/deviced RENAME display.conf)
ELSE()
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-exynos.conf DESTINATION /etc/deviced RENAME display.conf)
ENDIF()
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/mmc/mmc.conf DESTINATION /etc/deviced RENAME mmc.conf)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/pmqos/pmqos.conf DESTINATION /etc/deviced RENAME pmqos.conf)

IF(USE_MICRO_DD)
    IF(USE_EMULATOR)
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/battery/battery-micro-emul.conf DESTINATION /etc/deviced RENAME battery.conf)
    ELSE(USR_EMULATOR)
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/battery/battery-micro.conf DESTINATION /etc/deviced RENAME battery.conf)
    ENDIF(USE_EMULATOR)
ELSE(USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/battery/battery.conf DESTINATION /etc/deviced RENAME battery.conf)
ENDIF(USE_MICRO_DD)

INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/scripts/deviced-pre.sh DESTINATION bin)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/packaging/${PROJECT_NAME}.rule DESTINATION /etc/smack/accesses2.d)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/configurations/conf-supported.xml      DESTINATION ${DATADIR}/usb-configurations)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/configurations/usb-configurations.xml  DESTINATION ${DATADIR}/usb-configurations)

IF(NOT USE_MICRO_DD)
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/movi_format.sh DESTINATION bin)
	INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/mmc-smack-label DESTINATION bin)
ENDIF(NOT USE_MICRO_DD)

# USB (data-router)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/start_dr.sh  DESTINATION ${EXEC_PREFIX})

# USB (Manual setting)
IF(USE_ENGINEER_MODE)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/direct_set_debug.sh  DESTINATION ${EXEC_PREFIX})
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/set_usb_debug.sh     DESTINATION ${EXEC_PREFIX})
ENDIF()

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(src/libdeviced)
ADD_SUBDIRECTORY(src/devicectl)
ADD_SUBDIRECTORY(src/auto-test)

ENDIF(BUILD_EXECUTABLE)
