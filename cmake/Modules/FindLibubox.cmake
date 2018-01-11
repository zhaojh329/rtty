# - Try to find libubox
# Once done this will define
#  LIBUBOX_FOUND        - System has libubox
#  LIBUBOX_INCLUDE_DIR  - The libubox include directories
#  LIBUBOX_LIBRARY      - The libraries needed to use libubox

find_path(LIBUBOX_INCLUDE_DIR uloop.h PATH_SUFFIXES libubox)
find_library(LIBUBOX_LIBRARY ubox PATH_SUFFIXES lib64)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBUBOX_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libubox REQUIRED_VARS
                                  LIBUBOX_LIBRARY LIBUBOX_INCLUDE_DIR
                                  VERSION_VAR LIBUBOX_VERSION)

mark_as_advanced(LIBUBOX_INCLUDE_DIR LIBUBOX_LIBRARY)
