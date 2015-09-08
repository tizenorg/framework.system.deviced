SET(SYSTEM_DBUS_SERVICE_DIR "${PREFIX}/share/dbus-1/services")

INSTALL(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/org.tizen.system.deviced-auto-test.service
        DESTINATION
        ${SYSTEM_DBUS_SERVICE_DIR}
)
