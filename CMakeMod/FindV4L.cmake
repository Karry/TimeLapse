# - Try to find LibV4L
# Once done this will define
#  V4L_FOUND - System has LibV4L
#  V4L_INCLUDE_DIRS - The LibV4L include directories
#  V4LCONVERT_LIBRARY
#  V4L_LIBRARIES - The libraries needed to use LibV4L
#  V4L_DEFINITIONS - Compiler switches required for using LibV4L

find_package(PkgConfig)
pkg_check_modules(PC_V4L QUIET libv4l)
set(V4L_DEFINITIONS ${PC_V4L_CFLAGS_OTHER})

find_path(V4L_INCLUDE_DIR libv4l2.h libv4lconvert.h
          HINTS ${PC_V4L_INCLUDEDIR} ${PC_V4L_INCLUDE_DIRS}
          PATH_SUFFIXES libv4l )

find_library(V4L_LIBRARY NAMES v4l2
             HINTS ${PC_V4L_LIBDIR} ${PC_V4L_LIBRARY_DIRS} )
find_library(V4LCONVERT_LIBRARY NAMES v4lconvert
             HINTS ${PC_V4L_LIBDIR} ${PC_V4L_LIBRARY_DIRS} )

set(V4L_LIBRARIES ${V4L_LIBRARY} ${V4LCONVERT_LIBRARY})
set(V4L_INCLUDE_DIRS ${V4L_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set V4L_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(V4L  DEFAULT_MSG
                                  V4L_LIBRARY V4LCONVERT_LIBRARY V4L_INCLUDE_DIR)

mark_as_advanced(V4L_INCLUDE_DIR V4L_LIBRARY )
