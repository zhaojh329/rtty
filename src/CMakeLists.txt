# The version number.
set(RTTY_VERSION_MAJOR 9)
set(RTTY_VERSION_MINOR 0)
set(RTTY_VERSION_PATCH 2)

option(KCP_SUPPORT "KCP support" ON)

# Check the third party Libraries
find_package(Libev REQUIRED)

aux_source_directory(log SRCS)
aux_source_directory(buffer SRCS)

add_subdirectory(ssl)

list(APPEND SRCS main.c rtty.c net.c utils.c file.c filectl.c command.c http.c)

if(KCP_SUPPORT)
    if (NOT SSL_SUPPORT)
        message(WARNING "SSL Not enabled, disable KCP")
        set(KCP_SUPPORT OFF)
    else()
        list(APPEND SRCS kcp.c crc32.c crypt.c)
    endif()
endif()

add_executable(rtty ${SRCS})
target_compile_definitions(rtty PRIVATE _GNU_SOURCE)
target_compile_options(rtty PRIVATE -O -Wall -Werror --std=gnu99)
target_include_directories(rtty PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/buffer ${LIBEV_INCLUDE_DIR})
target_link_libraries(rtty PRIVATE ${LIBEV_LIBRARY} util crypt m)

if(SSL_SUPPORT)
    target_compile_definitions(rtty PRIVATE ${SSL_DEFINE})
    target_link_libraries(rtty PRIVATE ${SSL_TARGET})
endif()

if(KCP_SUPPORT)
    add_subdirectory(kcp)
    target_link_libraries(rtty PRIVATE kcp)
endif()

# configure a header file to pass some of the CMake settings to the source code
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(
    TARGETS rtty
    DESTINATION bin
)
