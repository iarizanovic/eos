# Try to find libncurses
# Once done, this will define
#
# NCURSES_FOUND           - system has libncurses
# NCURSES_INCLUDE_DIRS    - libncurses include directories
# NCURSES_LIBRARIES       - ncurses library
#
# and the following imported targets
#
# NCURSES::NCURSES

find_package(PkgConfig)
pkg_check_modules(PC_ncurses QUIET ncurses)
set(NCURSES_VERSION ${PC_ncurses_VERSION})

find_path(NCURSES_INCLUDE_DIR
  NAMES curses.h
  HINTS ${NCURSES_ROOT} ${PC_ncurses_INCLUDEDIR} ${PC_ncurses_INCLUDE_DIRS}
  PATH_SUFFIXES include)

find_library(NCURSES_LIBRARY
  NAMES ncurses
  HINTS ${NCURSES_ROOT} ${PC_ncurses_LIBDIR} ${PC_ncurses_LIBRARY_DIRS}
  PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ncurses
  REQUIRED_VARS NCURSES_LIBRARY NCURSES_INCLUDE_DIR
  VERSION_VAR NCURSES_VERSION)

mark_as_advanced(NCURSES_FOUND NCURSES_LIBRARY NCURSES_INCLUDE_DIR)

if (NCURSES_FOUND AND NOT TARGET NCURSES::NCURSES)
  add_library(NCURSES::NCURSES UNKNOWN IMPORTED)
  set_target_properties(NCURSES::NCURSES PROPERTIES
    IMPORTED_LOCATION "${NCURSES_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${NCURSES_INCLUDE_DIR}")
endif()

set(NCURSES_INCLUDE_DIRS ${NCURSES_INCLUDE_DIR})
set(NCURSES_LIBRARIES ${NCURSES_LIBRARY})
unset(NCURSES_INCLUDE_DIR)
unset(NCURSES_LIBRARY)
