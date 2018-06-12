add_definitions(-O -Wall -Werror --std=gnu99 -D_GNU_SOURCE)

# The version number.
set(RTTY_VERSION_MAJOR 5)
set(RTTY_VERSION_MINOR 0)
set(RTTY_VERSION_PATCH 0)

# Check the third party Libraries
find_package(Libubox REQUIRED)
find_package(Libuwsc 1.2 REQUIRED)
find_package(Libprotobuf-c 1.2 REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LIBUWSC_INCLUDE_DIR} ${LIBUBOX_INCLUDE_DIR} ${LIBPROTOBUFC_INCLUDE_DIR})
set(EXTRA_LIBS ${LIBUWSC_LIBRARY} ${LIBUBOX_LIBRARY} ${LIBPROTOBUFC_LIBRARY} blobmsg_json util crypt)

add_executable(rtty main.c utils.c command.c rtty.pb-c.c message.c)
target_link_libraries(rtty ${EXTRA_LIBS})

# configure a header file to pass some of the CMake settings to the source code
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(
    TARGETS rtty
    RUNTIME DESTINATION bin
)
