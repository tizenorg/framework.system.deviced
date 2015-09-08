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
SET(CONFDIR "/etc/${PROJECT_NAME}")
SET(VERSION 0.1.0)

SET(SRCS
	src/battery/config.c
	src/battery/lowbat-handler.c
	src/core/buxton-helper.c
	src/core/device-idler.c
	src/core/device-notifier.c
	src/core/main.c
	src/core/launch.c
	src/core/devices.c
	src/core/sig-handler.c
	src/core/log.c
	src/core/device-change-handler.c
	src/core/common.c
	src/core/config-parser.c
	src/core/execute.c
	src/core/edbus-handler.c
	src/core/power-supply.c
	src/earjack/earjack.c
	src/proc/cpu-info.c
	src/board/board-info.c
	src/proc/proc-handler.c
	src/storage/storage.c
	src/ta/ta-handler.c
	src/time/time-handler.c
	src/ticker/ticker.c
	src/testmode/testmode.c
)

IF(USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/cool-down/cool-down-micro.c
)
ELSE()
SET(SRCS ${SRCS}
	src/cool-down/cool-down.c
)
ENDIF()

SET(SRCS ${SRCS}
	src/hdmi-cec/libcec.c
	src/hdmi-cec/cec.c
)
SET(SRCS ${SRCS}
	src/gpio/gpio.c
	src/gpio/buzzer.c
	src/gpio/hall.c
	src/gpio/sim.c
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

#device sleep
SET(SRCS ${SRCS}
	src/sleep/sleep.c
)

SET(SRCS ${SRCS}
	src/apps/apps.c
	src/apps/cool-down.c
	src/apps/lowbat.c
	src/apps/poweroff.c
	src/apps/lowmem.c
	src/pmqos/pmqos.c
	src/pmqos/pmqos-plugin.c
	src/telephony/telephony.c
)

IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/battery/battery-time.c
	src/extcon/extcon.c
	src/hall/hall-handler.c
	src/apps/launch.c
	src/apps/mmc.c
	src/apps/usb.c
	src/apps/usbotg.c
	src/apps/system.c
)

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
	src/mmc/exfat.c
	src/mmc/vfat.c
)

SET(SRCS ${SRCS}
	src/ode/ode.c
	src/ode/noti.c
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
	src/display/display-actor.c
	src/display/auto-brightness.c
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
	src/display/smartstay.c
)
ENDIF(USE_MICRO_DD)

SET(SRCS ${SRCS}
	src/led/ir.c
	src/led/torch.c
	src/led/pattern.c
	src/led/noti.c
)

SET(SRCS ${SRCS}
	src/pass/pass.c
	src/pass/pass-core.c
	src/pass/pass-gov-step.c
	src/pass/pass-gov-radiation.c
	src/pass/pass-plugin.c
)

SET(SRCS ${SRCS}
	src/touch/touchscreen.c)

IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/touch/touch.c
	src/touch/touchkey.c
	src/touch/touch-controller.c
	src/touch/touch-plugin.c)


SET(SRCS ${SRCS}
	src/led/rgb.c
	src/led/conf.c
)
ENDIF(NOT USE_MICRO_DD)

SET(SRCS ${SRCS}
	src/control/control.c
)

SET(SRCS ${SRCS}
	src/haptic/haptic.c
	src/haptic/external.c
	src/haptic/standard.c
	src/haptic/emulator.c)

# usb client
SET(SRCS ${SRCS}
	src/usb/usb-common.c
	src/usb/usb-client.c
	src/usb/usb-client-event-sdk.c
	src/usb/usb-client-config.c
	src/usb/usb-client-set.c
	src/usb/usb-client-control.c
	src/usb/usb-client-dbus.c
)
IF(USE_MICRO_DD)
SET(SRCS ${SRCS} src/usb/usb-client-mode-micro.c)
ELSE(USE_MICRO_DD)
SET(SRCS ${SRCS} src/usb/usb-client-mode.c)
ENDIF(USE_MICRO_DD)

# usb host
IF(NOT USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/usb/usb-common.c
	src/usb/usb-host.c
	src/usb/usb-host-dbus.c
	src/usb/usb-host-storage.c
	src/usb/usb-host-storage-vfat.c
	src/usb/usb-host-storage-exfat.c
	src/usb/usb-host-hid.c
	src/usb/usb-host-camera.c
	src/usb/usb-host-printer.c
	src/usb/usb-host-naming.c
)
ENDIF(NOT USE_MICRO_DD)

# powersaver mode
SET(SRCS ${SRCS}
	src/powersaver/powersaver.c
)
IF(USE_MICRO_DD)
SET(SRCS ${SRCS}
	src/powersaver/powersaver-micro.c
)
ENDIF(USE_MICRO_DD)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)

SET(PKG_MODULES
	libbuxton
	ecore
	edbus
	eina
	vconf
	dlog
	device-node
	capi-base-common
	journal
	storage
	sensor
	tapi
)

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
ADD_DEFINITIONS("-DENABLE_PM_LOG")
IF(USE_ARM)
	ADD_DEFINITIONS("-DTARGET")
ELSEIF(USE_EMULATOR)
	ADD_DEFINITIONS("-DEMULATOR")
ENDIF()
ADD_DEFINITIONS("-DDEBUG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-lm" "-ludev" "-ledbus" "-lstorage" shared)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/deviced/ DESTINATION include/${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*_doc.h" EXCLUDE
		PATTERN "*.h")

CONFIGURE_FILE(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)

IF(NOT USE_MICRO_DD)
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/led/led.conf DESTINATION /etc/deviced)
ENDIF(NOT USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dump_pm.sh DESTINATION /opt/etc/dump.d/module.d)

IF(USE_EMULATOR AND USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-emul-wearable.conf DESTINATION /etc/deviced RENAME display.conf)
ELSEIF(USE_EMULATOR)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-emul-mobile.conf DESTINATION /etc/deviced RENAME display.conf)
ELSEIF(USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-wearable.conf DESTINATION /etc/deviced RENAME display.conf)
ELSE()
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/display/display-mobile.conf DESTINATION /etc/deviced RENAME display.conf)
ENDIF()
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/mmc/mmc.conf DESTINATION /etc/deviced RENAME mmc.conf)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/pmqos/pmqos.conf DESTINATION /etc/deviced RENAME pmqos.conf)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/battery/battery.conf DESTINATION /etc/deviced RENAME battery.conf)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/storage/storage.conf DESTINATION /etc/deviced RENAME storage.conf)

IF(USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/pass/pass-micro.conf DESTINATION /etc/deviced RENAME pass.conf)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/haptic/haptic-micro.conf DESTINATION /etc/deviced RENAME haptic.conf)
ELSE()
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/pass/pass.conf DESTINATION /etc/deviced RENAME pass.conf)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/haptic/haptic-mobile.conf DESTINATION /etc/deviced RENAME haptic.conf)
ENDIF(USE_MICRO_DD)

INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/scripts/deviced-pre.sh DESTINATION bin)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/packaging/${PROJECT_NAME}.efl DESTINATION /etc/smack/accesses.d)
IF(USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/usb-client-configuration-micro.conf  DESTINATION ${CONFDIR} RENAME usb-client-configuration.conf)
ELSE(USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/usb-client-configuration.conf  DESTINATION ${CONFDIR})
ENDIF(USE_MICRO_DD)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/usb-client-operation.conf  DESTINATION ${CONFDIR})

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
IF(NOT USE_MICRO_DD)
	ADD_SUBDIRECTORY(src/fsck-msdos)
	ADD_SUBDIRECTORY(src/newfs-msdos)
ENDIF(NOT USE_MICRO_DD)
ADD_SUBDIRECTORY(src/devicectl)
ADD_SUBDIRECTORY(src/auto-test)

ENDIF(BUILD_EXECUTABLE)
