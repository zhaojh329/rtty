# - Try to find libuwsc
# Once done this will define
#  LIBUWSC_FOUND          - System has libuwsc
#  LIBUWSC_INCLUDE_DIR    - The libuwsc include directories
#  LIBUWSC_LIBRARY        - The libraries needed to use libuwsc

find_path(LIBUWSC_INCLUDE_DIR uwsc)
find_library(LIBUWSC_LIBRARY uwsc PATH_SUFFIXES lib64)

if(LIBUWSC_INCLUDE_DIR)
  file(STRINGS "${LIBUWSC_INCLUDE_DIR}/uwsc/uwsc.h"
      LIBUWSC_VERSION_MAJOR REGEX "^#define[ \t]+UWSC_VERSION_MAJOR[ \t]+[0-9]+")
  file(STRINGS "${LIBUWSC_INCLUDE_DIR}/uwsc/uwsc.h"
      LIBUWSC_VERSION_MINOR REGEX "^#define[ \t]+UWSC_VERSION_MINOR[ \t]+[0-9]+")
  string(REGEX REPLACE "[^0-9]+" "" LIBUWSC_VERSION_MAJOR "${LIBUWSC_VERSION_MAJOR}")
  string(REGEX REPLACE "[^0-9]+" "" LIBUWSC_VERSION_MINOR "${LIBUWSC_VERSION_MINOR}")
  set(LIBUWSC_VERSION "${LIBUWSC_VERSION_MAJOR}.${LIBUWSC_VERSION_MINOR}")
  unset(LIBUWSC_VERSION_MINOR)
  unset(LIBUWSC_VERSION_MAJOR)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBUWSC_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libuwsc REQUIRED_VARS
                                  LIBUWSC_LIBRARY LIBUWSC_INCLUDE_DIR
                                  VERSION_VAR LIBUWSC_VERSION)

mark_as_advanced(LIBUWSC_INCLUDE_DIR LIBUWSC_LIBRARY)
