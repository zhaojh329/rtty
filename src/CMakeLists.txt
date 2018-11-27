add_definitions(-O -Wall -Werror --std=gnu99 -D_GNU_SOURCE)

# The version number.
set(RTTY_VERSION_MAJOR 6)
set(RTTY_VERSION_MINOR 3)
set(RTTY_VERSION_PATCH 1)

# Check the third party Libraries
find_package(Libev REQUIRED)
find_package(Libuwsc 3.0 REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LIBUWSC_INCLUDE_DIR} ${LIBEV_INCLUDE_DIR})
set(EXTRA_LIBS ${LIBUWSC_LIBRARY} ${LIBEV_LIBRARY} util crypt m)

add_executable(rtty main.c utils.c json.c command.c file.c)
target_link_libraries(rtty ${EXTRA_LIBS})

# configure a header file to pass some of the CMake settings to the source code
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(
    TARGETS rtty
    RUNTIME DESTINATION bin
)
