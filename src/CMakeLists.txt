add_definitions(-O -Wall -Werror --std=gnu99 -D_GNU_SOURCE)

# The version number.
set(RTTY_VERSION_MAJOR 7)
set(RTTY_VERSION_MINOR 1)
set(RTTY_VERSION_PATCH 4)

# Check the third party Libraries
find_package(Libev REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/buffer ${LIBEV_INCLUDE_DIR})
set(EXTRA_LIBS ${LIBEV_LIBRARY} util crypt m)

add_definitions(-DRTTY_FILE_UNIX_SOCKET_S="/var/run/rttyfile.sock")
add_definitions(-DRTTY_FILE_UNIX_SOCKET_C="/var/run/rttyfile_c.sock")

set(RTTY_SSL_SUPPORT_CONFIG 1)
option(RTTY_SSL_SUPPORT "SSL support" ON)

option(RTTY_USE_OPENSSL "Force select OpenSSL" OFF)
option(RTTY_USE_WOLFSSL "Force select WolfSSL(CyaSSL)" OFF)
option(RTTY_USE_MBEDTLS "Force select MbedTLS(PolarSSL)" OFF)

set(SSL_NAME "")
set(RTTY_HAVE_OPENSSL_CONFIG 0)
set(RTTY_HAVE_WOLFSSL_CONFIG 0)
set(RTTY_HAVE_MBEDTLS_CONFIG 0)

if(NOT RTTY_SSL_SUPPORT)
    set(RTTY_SSL_SUPPORT_CONFIG 0)
else()
    find_package(PkgConfig QUIET)
    pkg_check_modules(OPENSSL QUIET openssl)
    if (NOT OPENSSL_FOUND)
        find_package(OpenSSL)
    endif()
    find_package(WolfSSL)
    find_package(MbedTLS)
    find_package(ZLIB)

    if(RTTY_USE_OPENSSL)
        if (NOT OPENSSL_FOUND)
            set(RTTY_SSL_SUPPORT OFF)
            message(WARNING "Force select OpenSSL, but not found it")
        endif()
    elseif(RTTY_USE_WOLFSSL)
        if (NOT WOLFSSL_FOUND)
            set(RTTY_SSL_SUPPORT OFF)
            message(WARNING "Force select WolfSSL(CyaSSL), but not found it")
        endif()
    elseif(RTTY_USE_MBEDTLS)
        if (NOT MBEDTLS_FOUND)
            set(RTTY_SSL_SUPPORT OFF)
            message(WARNING "Force select MbedTLS(PolarSSL), but not found it")
        endif()
    elseif(OPENSSL_FOUND)
        set(RTTY_USE_OPENSSL ON)
    elseif(WOLFSSL_FOUND)
        set(RTTY_USE_WOLFSSL ON)
    elseif(MBEDTLS_FOUND)
        set(RTTY_USE_MBEDTLS ON)
    else()
        set(RTTY_SSL_SUPPORT OFF)
        message(WARNING "No available SSL libraries found")
    endif()

    if(RTTY_USE_OPENSSL)
        set(SSL_NAME "OpenSSL")
        set(SSL_INC ${OPENSSL_INCLUDE_DIR})
        set(SSL_LIB ${OPENSSL_LIBRARIES})
        set(RTTY_HAVE_OPENSSL_CONFIG 1)
    elseif(RTTY_USE_WOLFSSL)
        set(SSL_NAME "WolfSSL(CyaSSL)")
        set(SSL_INC ${WOLFSSL_INCLUDE_DIR})
        set(SSL_LIB ${WOLFSSL_LIBRARIES})
        set(RTTY_HAVE_WOLFSSL_CONFIG 1)
    elseif(RTTY_USE_MBEDTLS)
        set(SSL_NAME "MbedTLS(PolarSSL)")
        set(SSL_INC ${MBEDTLS_INCLUDE_DIR})
        if(ZLIB_FOUND)
            set(SSL_LIB ${MBEDTLS_LIBRARIES} ${ZLIB_LIBRARIES})
        else()
            set(SSL_LIB ${MBEDTLS_LIBRARIES}) 
        endif()
        set(RTTY_HAVE_MBEDTLS_CONFIG 1)
    endif()

    if(RTTY_SSL_SUPPORT)
        include_directories(${SSL_INC})
        list(APPEND EXTRA_LIBS ${SSL_LIB})
        message(STATUS "Select ${SSL_NAME} as the SSL backend")
    else()
        set(RTTY_SSL_SUPPORT_CONFIG 0)
    endif()
endif()

add_executable(rtty main.c utils.c buffer/buffer.c log.c net.c net.h rtty.c rtty.h command.c
        file.c upfile.c upfile.h downfile.c downfile.h ssl.c ssl.h)
target_link_libraries(rtty ${EXTRA_LIBS})

# configure a header file to pass some of the CMake settings to the source code
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(
    TARGETS rtty
    RUNTIME DESTINATION bin
)
