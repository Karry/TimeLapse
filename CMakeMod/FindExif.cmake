# - Try to find LibExif
# Once done this will define
#  Exif_FOUND - System has LibExif
#  Exif_INCLUDE_DIRS - The LibExif include directories
#  Exif_LIBRARIES - The libraries needed to use LibExif
#  Exif_DEFINITIONS - Compiler switches required for using LibExif

find_package(PkgConfig)
pkg_check_modules(PC_Exif QUIET libExif)
set(Exif_DEFINITIONS ${PC_Exif_CFLAGS_OTHER})

find_path(Exif_INCLUDE_DIR exif-utils.h
    HINTS ${PC_Exif_INCLUDEDIR} ${PC_Exif_INCLUDE_DIRS}
    PATH_SUFFIXES libexif )

find_library(Exif_LIBRARY NAMES exif
    HINTS ${PC_Exif_LIBDIR} ${PC_Exif_LIBRARY_DIRS} )

set(Exif_LIBRARIES ${Exif_LIBRARY} )
set(Exif_INCLUDE_DIRS ${Exif_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set Exif_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Exif  DEFAULT_MSG
    Exif_LIBRARY Exif_INCLUDE_DIR)

mark_as_advanced(Exif_INCLUDE_DIR Exif_LIBRARY )
