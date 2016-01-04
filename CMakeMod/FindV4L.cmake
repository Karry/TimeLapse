# - Try to find LibV4L
# Once done this will define
#  LIBV4L_FOUND - System has LibV4L
#  LIBV4L_INCLUDE_DIRS - The LibV4L include directories
#  LIBV4LCONVERT_LIBRARY
#  LIBV4L_LIBRARIES - The libraries needed to use LibV4L
#  LIBV4L_DEFINITIONS - Compiler switches required for using LibV4L

find_package(PkgConfig)
pkg_check_modules(PC_LIBV4L QUIET libv4l)
set(LIBV4L_DEFINITIONS ${PC_LIBV4L_CFLAGS_OTHER})

find_path(LIBV4L_INCLUDE_DIR libv4l2.h libv4lconvert.h
          HINTS ${PC_LIBV4L_INCLUDEDIR} ${PC_LIBV4L_INCLUDE_DIRS}
          PATH_SUFFIXES libv4l )

find_library(LIBV4L_LIBRARY NAMES v4l2 
             HINTS ${PC_LIBV4L_LIBDIR} ${PC_LIBV4L_LIBRARY_DIRS} )
find_library(LIBV4LCONVERT_LIBRARY NAMES v4lconvert
             HINTS ${PC_LIBV4L_LIBDIR} ${PC_LIBV4L_LIBRARY_DIRS} )

set(LIBV4L_LIBRARIES ${LIBV4L_LIBRARY} ${LIBV4LCONVERT_LIBRARY})
set(LIBV4L_INCLUDE_DIRS ${LIBV4L_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBV4L_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibV4L  DEFAULT_MSG
                                  LIBV4L_LIBRARY LIBV4LCONVERT_LIBRARY LIBV4L_INCLUDE_DIR)

mark_as_advanced(LIBV4L_INCLUDE_DIR LIBV4L_LIBRARY )
