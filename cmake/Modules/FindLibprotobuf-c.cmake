# - Try to find libprotobuf-c
# Once done this will define
#  LIBPROTOBUFC_FOUND          - System has libprotobuf-c
#  LIBPROTOBUFC_INCLUDE_DIR    - The libprotobuf-c include directories
#  LIBPROTOBUFC_LIBRARY        - The libraries needed to use libprotobuf-c

find_path(LIBPROTOBUFC_INCLUDE_DIR protobuf-c)
find_library(LIBPROTOBUFC_LIBRARY protobuf-c PATH_SUFFIXES lib64)

if(LIBPROTOBUFC_INCLUDE_DIR)
  file(STRINGS "${LIBPROTOBUFC_INCLUDE_DIR}/protobuf-c/protobuf-c.h"
      LIBPROTOBUFC_VERSION REGEX "^#define[ \t]+PROTOBUF_C_VERSION[ \t]+[\"0-9]+")
      string(REGEX REPLACE "[^0-9.]+" "" LIBPROTOBUFC_VERSION "${LIBPROTOBUFC_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBPROTOBUFC_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libprotobuf-c REQUIRED_VARS
                                  LIBPROTOBUFC_LIBRARY LIBPROTOBUFC_INCLUDE_DIR
                                  VERSION_VAR LIBPROTOBUFC_VERSION)

mark_as_advanced(LIBPROTOBUFC_INCLUDE_DIR LIBPROTOBUFC_LIBRARY)
