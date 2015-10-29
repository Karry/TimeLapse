# - Try to find LibVidStab
# Once done this will define
#  LIBVIDSTAB_FOUND - System has LibVidStab
#  LIBVIDSTAB_INCLUDE_DIRS - The LibVidStab include directories
#  LIBVIDSTAB_LIBRARIES - The libraries needed to use LibVidStab
#  LIBVIDSTAB_DEFINITIONS - Compiler switches required for using LibVidStab

find_package(PkgConfig)
pkg_check_modules(PC_LIBVIDSTAB QUIET libvidstab)
set(LIBVIDSTAB_DEFINITIONS ${PC_LIBVIDSTAB_CFLAGS_OTHER})

find_path(LIBVIDSTAB_INCLUDE_DIR vid.stab/libvidstab.h
          HINTS ${PC_LIBVIDSTAB_INCLUDEDIR} ${PC_LIBVIDSTAB_INCLUDE_DIRS}
          PATH_SUFFIXES libvidstab )

find_library(LIBVIDSTAB_LIBRARY NAMES vidstab libvidstab
             HINTS ${PC_LIBVIDSTAB_LIBDIR} ${PC_LIBVIDSTAB_LIBRARY_DIRS} )

set(LIBVIDSTAB_LIBRARIES ${LIBVIDSTAB_LIBRARY} )
set(LIBVIDSTAB_INCLUDE_DIRS ${LIBVIDSTAB_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBVIDSTAB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibVidStab  DEFAULT_MSG
                                  LIBVIDSTAB_LIBRARY LIBVIDSTAB_INCLUDE_DIR)

mark_as_advanced(LIBVIDSTAB_INCLUDE_DIR LIBVIDSTAB_LIBRARY )
