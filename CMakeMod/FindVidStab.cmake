# - Try to find LibVidStab
# Once done this will define
#  VIDSTAB_FOUND - System has LibVidStab
#  VIDSTAB_INCLUDE_DIRS - The LibVidStab include directories
#  VIDSTAB_LIBRARIES - The libraries needed to use LibVidStab
#  VIDSTAB_DEFINITIONS - Compiler switches required for using LibVidStab

find_package(PkgConfig)
pkg_check_modules(PC_VIDSTAB QUIET libvidstab)
set(VIDSTAB_DEFINITIONS ${PC_VIDSTAB_CFLAGS_OTHER})

find_path(VIDSTAB_INCLUDE_DIR vid.stab/libvidstab.h
          HINTS ${PC_VIDSTAB_INCLUDEDIR} ${PC_VIDSTAB_INCLUDE_DIRS}
          PATH_SUFFIXES libvidstab )

find_library(VIDSTAB_LIBRARY NAMES vidstab libvidstab
             HINTS ${PC_VIDSTAB_LIBDIR} ${PC_VIDSTAB_LIBRARY_DIRS} )

set(VIDSTAB_LIBRARIES ${VIDSTAB_LIBRARY} )
set(VIDSTAB_INCLUDE_DIRS ${VIDSTAB_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set VIDSTAB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(VidStab  DEFAULT_MSG
                                  VIDSTAB_LIBRARY VIDSTAB_INCLUDE_DIR)

mark_as_advanced(VIDSTAB_INCLUDE_DIR VIDSTAB_LIBRARY )
