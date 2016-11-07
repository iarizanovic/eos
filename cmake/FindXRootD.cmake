# Try to find XROOTD
# Once done, this will define
#
# XROOTD_FOUND               - system has XRootD
# XROOTD_INCLUDE_DIR         - XRootD include directories
# XROOTD_LIB_DIR             - libraries needed to use XRootD
# XROOTD_PRIVATE_INCLUDE_DIR - XRootD private include directory
#
# XROOTD_DIR may be defined as a hint for where to look

include(FindPackageHandleStandardArgs)

if(XROOTD_INCLUDE_DIRS AND XROOTD_LIBRARIES)
  set(XROOTD_FIND_QUIETLY TRUE)
else()
  find_path(
    XROOTD_INCLUDE_DIR
    NAMES XrdVersion.hh
    HINTS ${XROOTD_ROOT_DIR} $ENV{XROOTD_ROOT_DIR}
    PATH_SUFFIXES include/xrootd)

  find_path(
    XROOTD_PRIVATE_INCLUDE_DIR
    NAMES XrdOss/XrdOssApi.hh
    HINTS ${XROOTD_ROOT_DIR} $ENV{XROOTD_ROOT_DIR}
    PATH_SUFFIXES include/xrootd/private)

  find_library(
    XROOTD_UTILS_LIBRARY
    NAMES XrdUtils
    HINTS ${XROOTD_ROOT_DIR} $ENV{XROOTD_ROOT_DIR}
    PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

  find_library(
    XROOTD_SERVER_LIBRARY
    NAMES XrdServer
    HINTS ${XROOTD_ROOT_DIR} $ENV{XROOTD_ROOT_DIR}
    PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

  find_library(
    XROOTD_CL_LIBRARY
    NAMES XrdCl
    HINTS ${XROOTD_ROOT_DIR} $ENV{XROOTD_ROOT_DIR}
    PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

  find_library(
    XROOTD_POSIX_LIBRARY
    NAMES XrdPosix
    HINTS ${XROOTD_ROOT_DIR} $ENV{XROOTD_ROOT_DIR}
    PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

  set(XROOTD_INCLUDE_DIRS ${XROOTD_INCLUDE_DIR} ${XROOTD_PRIVATE_INCLUDE_DIR})

  set(
    XROOTD_LIBRARIES
    ${XROOTD_SERVER_LIBRARY}
    ${XROOTD_CL_LIBRARY}
    ${XROOTD_UTILS_LIBRARY}
    ${XROOTD_POSIX_LIBRARY})

  find_package_handle_standard_args(
    XRootD
    DEFAULT_MSG
    XROOTD_SERVER_LIBRARY
    XROOTD_UTILS_LIBRARY
    XROOTD_CL_LIBRARY
                                          XROOTD_INCLUDE_DIR
    XROOTD_PRIVATE_INCLUDE_DIR)

  mark_as_advanced(
    XROOTD_SERVER_LIBRARY
    XROOTD_UTILS_LIBRARY
    XROOTD_CL_LIBRARY
    XROOTD_INCLUDE_DIR
    XROOTD_PRIVATE_INCLUDE_DIR)
endif()
