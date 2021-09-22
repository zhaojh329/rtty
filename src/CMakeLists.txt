# The version number.
set(RTTY_VERSION_MAJOR 8)
set(RTTY_VERSION_MINOR 0)
set(RTTY_VERSION_PATCH 0)

# Check the third party Libraries
find_package(Libev REQUIRED)

aux_source_directory(. SRCS)
aux_source_directory(log SRCS)
aux_source_directory(buffer SRCS)

add_executable(rtty ${SRCS})
target_compile_definitions(rtty PRIVATE _GNU_SOURCE)
target_compile_options(rtty PRIVATE -O -Wall -Werror --std=gnu99)
target_include_directories(rtty PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/buffer ${LIBEV_INCLUDE_DIR})
target_link_libraries(rtty PRIVATE ${LIBEV_LIBRARY} util crypt m)

add_subdirectory(ssl)

if(SSL_SUPPORT)
    target_link_libraries(rtty PRIVATE ${SSL_TARGET})
endif()

# configure a header file to pass some of the CMake settings to the source code
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(
    TARGETS rtty
    DESTINATION bin
)
