cmake_minimum_required(VERSION 3.10)

project(timelapse)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/CMakeMod")
list(APPEND CMAKE_MODULE_PATH "CMakeMod")

FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Core Multimedia)
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS Magick++ MagickCore)
FIND_PACKAGE(VidStab REQUIRED)
FIND_PACKAGE(V4L REQUIRED)
FIND_PACKAGE(GPHOTO2 REQUIRED)

# enable warnings
ADD_DEFINITIONS( -Wall -Wextra -pedantic -fPIC)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC)
    set(TIMELAPS_WARNING_FLAGS "-Wall -pedantic -Winit-self -Wno-overloaded-virtual -Wcast-qual -Wwrite-strings -Wextra -Wno-long-long")
    if(EMSCRIPTEN)
        set(TIMELAPS_WARNING_FLAGS "${TIMELAPS_WARNING_FLAGS} -Wno-warn-absolute-paths")
    elseif(NOT APPLE)
        set(TIMELAPS_WARNING_FLAGS "${TIMELAPS_WARNING_FLAGS} -Wno-unused-but-set-parameter")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TIMELAPS_WARNING_FLAGS}")
endif()

# QT5 definitions
ADD_DEFINITIONS(${Qt5Core_DEFINITIONS})

# read and setup ImageMagick cflags (these flags are not setup by FindImageMagick.cmake)
FIND_PACKAGE(PkgConfig REQUIRED)
pkg_check_modules(IM "ImageMagick")
# ImageMagick compile time configuration
# ADD_DEFINITIONS(-DMAGICKCORE_QUANTUM_DEPTH=16) # Number of bits in a pixel Quantum (8/16/32/64)
# ADD_DEFINITIONS(-DMAGICKCORE_HDRI_SUPPORT=0) # colorHistogram fails with SIGSEGV when this option is enabled
ADD_DEFINITIONS(${IM_CFLAGS})

add_subdirectory(src)

enable_testing()
add_subdirectory(test)

## for debug
message(STATUS "Module path:              ${CMAKE_MODULE_PATH}")
message(STATUS "Qt5Core_INCLUDE_DIRS:     ${Qt5Core_INCLUDE_DIRS}")
message(STATUS "Qt5Core_DEFINITIONS:      ${Qt5Core_DEFINITIONS}")
message(STATUS "ImageMagick_INCLUDE_DIRS: ${ImageMagick_INCLUDE_DIRS}")
message(STATUS "ImageMagick_LIBRARIES:    ${ImageMagick_LIBRARIES}")
message(STATUS "IM_CFLAGS:                ${IM_CFLAGS}")
message(STATUS "VIDSTAB_INCLUDE_DIRS:     ${VIDSTAB_INCLUDE_DIRS}")
message(STATUS "VIDSTAB_LIBRARIES:        ${VIDSTAB_LIBRARIES}")
message(STATUS "VIDSTAB_FOUND:            ${VIDSTAB_FOUND}")
message(STATUS "V4L_INCLUDE_DIRS:         ${V4L_INCLUDE_DIRS}")
message(STATUS "V4L_LIBRARIES:            ${V4L_LIBRARIES}")
message(STATUS "V4L_FOUND:                ${V4L_FOUND}")
message(STATUS "V4L_DEFINITIONS:          ${V4L_DEFINITIONS}")
message(STATUS "GPHOTO2_FOUND:            ${GPHOTO2_FOUND}")
message(STATUS "GPHOTO2_INCLUDE_DIR:      ${GPHOTO2_INCLUDE_DIR}")
message(STATUS "GPHOTO2_LIBRARIES:        ${GPHOTO2_LIBRARIES}")
