CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

############### TARGET ###########################
ADD_EXECUTABLE(logd_foreach_test foreach-test.c)
ADD_EXECUTABLE(logd_padvisor_test padvisor-test.c)
TARGET_LINK_LIBRARIES(logd_foreach_test ${LIB_LOGD_DB} ${LIB_LOGD})
TARGET_LINK_LIBRARIES(logd_padvisor_test ${LIB_LOGD_DB} ${LIB_LOGD})

############### INSTALL ##########################
INSTALL(TARGETS logd_foreach_test DESTINATION bin)
INSTALL(TARGETS logd_padvisor_test DESTINATION bin)
