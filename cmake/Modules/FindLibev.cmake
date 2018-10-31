# - Try to find libev
# Once done this will define
#  LIBEV_FOUND          - System has libev
#  LIBEV_INCLUDE_DIR    - The libev include directories
#  LIBEV_LIBRARY        - The libraries needed to use libev

find_path(LIBEV_INCLUDE_DIR ev.h PATH_SUFFIXES libev)
find_library(LIBEV_LIBRARY ev PATH_SUFFIXES lib64)

if(LIBEV_INCLUDE_DIR)
  file(STRINGS "${LIBEV_INCLUDE_DIR}/ev.h"
      LIBEV_VERSION_MAJOR REGEX "^#define[ \t]+EV_VERSION_MAJOR[ \t]+[0-9]+")
  file(STRINGS "${LIBEV_INCLUDE_DIR}/ev.h"
      LIBEV_VERSION_MINOR REGEX "^#define[ \t]+EV_VERSION_MINOR[ \t]+[0-9]+")
  string(REGEX REPLACE "[^0-9]+" "" LIBEV_VERSION_MAJOR "${LIBEV_VERSION_MAJOR}")
  string(REGEX REPLACE "[^0-9]+" "" LIBEV_VERSION_MINOR "${LIBEV_VERSION_MINOR}")
  set(LIBEV_VERSION "${LIBEV_VERSION_MAJOR}.${LIBEV_VERSION_MINOR}")
  unset(LIBEV_VERSION_MINOR)
  unset(LIBEV_VERSION_MAJOR)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBEV_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libev REQUIRED_VARS
                                  LIBEV_LIBRARY LIBEV_INCLUDE_DIR
                                  VERSION_VAR LIBEV_VERSION)

mark_as_advanced(LIBEV_INCLUDE_DIR LIBEV_LIBRARY)
