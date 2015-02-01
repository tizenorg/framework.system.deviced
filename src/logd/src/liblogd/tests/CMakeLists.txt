CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

############### TARGET ###########################
ADD_EXECUTABLE(store-event-test store-event-test.c)
TARGET_LINK_LIBRARIES(store-event-test ${LIB_LOGD})

############### INSTALL ##########################
INSTALL(TARGETS store-event-test DESTINATION bin)
