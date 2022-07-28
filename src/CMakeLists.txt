
# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/CMakeMod")
list(APPEND CMAKE_MODULE_PATH "../CMakeMod")

FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Core Multimedia)
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS Magick++ MagickCore)
FIND_PACKAGE(VidStab REQUIRED) 
FIND_PACKAGE(V4L REQUIRED) 
FIND_PACKAGE(GPHOTO2 REQUIRED)
FIND_PACKAGE(Exif REQUIRED)

set(DESCRIBE_CMD "git describe --tags --dirty=+dirty --match 'v[0-9]*' | sed -E 's/^v//' ")
execute_process(
	COMMAND bash -c "${DESCRIBE_CMD}"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE VERSION_TIMELAPSE
	OUTPUT_STRIP_TRAILING_WHITESPACE
	)

if(NOT VERSION_TIMELAPSE)
	message(WARNING "Cannot evaluate version with git describe")
	set(VERSION_TIMELAPSE "?")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/timelapse_version.h.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/TimeLapse/timelapse_version.h")

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
  ${ImageMagick_INCLUDE_DIRS}
  ${VIDSTAB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR} # timelapse_version.h
  ${V4L_INCLUDE_DIRS}
  ${GPHOTO2_INCLUDE_DIR}
  ${Exif_INCLUDE_DIRS})

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
message(STATUS "GPHOTO2_FOUND:            ${GPHOTO2_FOUND}")
message(STATUS "GPHOTO2_INCLUDE_DIR:      ${GPHOTO2_INCLUDE_DIR}")
message(STATUS "GPHOTO2_LIBRARIES:        ${GPHOTO2_LIBRARIES}")
message(STATUS "Exif_FOUND:               ${Exif_FOUND}")
message(STATUS "Exif_INCLUDE_DIR:         ${Exif_INCLUDE_DIR}")
message(STATUS "Exif_LIBRARIES:           ${Exif_LIBRARIES}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(timelapse_HEADERS
	TimeLapse/black_hole_device.h
	TimeLapse/capture.h
	TimeLapse/input_image_info.h
	TimeLapse/error_message_helper.h

	TimeLapse/pipeline.h
	TimeLapse/pipeline_handler.h
	TimeLapse/pipeline_frame_mapping.h
	TimeLapse/pipeline_frame_prepare.h
	TimeLapse/pipeline_video_assembly.h
	TimeLapse/pipeline_write_frame.h
	TimeLapse/pipeline_resize_frame.h
	TimeLapse/pipeline_source.h
	TimeLapse/pipeline_deflicker.h
	TimeLapse/pipeline_stab.h
	TimeLapse/pipeline_cpt_v4l.h
	TimeLapse/pipeline_cpt_gphoto2.h
	TimeLapse/pipeline_cpt_qcamera.h
	TimeLapse/pipeline_cpt.h)

set(timelapse_SRCS
    black_hole_device.cpp
	capture.cpp
    input_image_info.cpp
    pipeline_handler.cpp
    pipeline_frame_mapping.cpp
    pipeline_frame_prepare.cpp
    pipeline_resize_frame.cpp
    pipeline_video_assembly.cpp
    pipeline_write_frame.cpp
    pipeline_source.cpp
    pipeline_deflicker.cpp
    pipeline_stab.cpp
    pipeline_cpt_v4l.cpp
    pipeline_cpt_gphoto2.cpp
    pipeline_cpt_qcamera.cpp
    pipeline_cpt.cpp
    pipeline.cpp
	timelapse.cpp)

set(timelapse_assembly_SRCS
	timelapse_assembly.h
    timelapse_assembly.cpp)

set(timelapse_deflicker_SRCS
	timelapse_deflicker.h
    timelapse_deflicker.cpp)

set(timelapse_stabilize_SRCS
	timelapse_stabilize.h
    timelapse_stabilize.cpp)

set(timelapse_capture_SRCS
	timelapse_capture.h
    timelapse_capture.cpp)

add_library(timelapse SHARED
	${timelapse_SRCS}
	${timelapse_HEADERS})

add_executable(timelapse_assembly
	${timelapse_assembly_SRCS})

add_executable(timelapse_deflicker
	${timelapse_deflicker_SRCS})

add_executable(timelapse_stabilize
	${timelapse_stabilize_SRCS})

add_executable(timelapse_capture
	${timelapse_capture_SRCS})

set_property(TARGET timelapse PROPERTY CXX_STANDARD 17)
set_property(TARGET timelapse_assembly PROPERTY CXX_STANDARD 17)
set_property(TARGET timelapse_deflicker PROPERTY CXX_STANDARD 17)
set_property(TARGET timelapse_stabilize PROPERTY CXX_STANDARD 17)
set_property(TARGET timelapse_capture PROPERTY CXX_STANDARD 17)

#Link the timelapse_assembly executable to the Qt 5 libraries.
target_link_libraries(timelapse
	Qt5::Core
	Qt5::Multimedia
	${ImageMagick_LIBRARIES} # Magick++-6.Q16
	${VIDSTAB_LIBRARIES}
	${V4L_LIBRARIES}
	${GPHOTO2_LIBRARIES}
	${Exif_LIBRARIES})

target_link_libraries(timelapse_assembly
	timelapse
	Qt5::Core
	${ImageMagick_LIBRARIES} # Magick++-6.Q16
	${GPHOTO2_LIBRARIES})

target_link_libraries(timelapse_deflicker
  	timelapse
  	Qt5::Core
  	${ImageMagick_LIBRARIES} # Magick++-6.Q16
	)

target_link_libraries(timelapse_stabilize
	timelapse
	Qt5::Core
	${ImageMagick_LIBRARIES} # Magick++-6.Q16
	${VIDSTAB_LIBRARIES})

target_link_libraries(timelapse_capture
	timelapse
	Qt5::Core
	${ImageMagick_LIBRARIES} # Magick++-6.Q16
	${V4L_LIBRARIES}
	${GPHOTO2_LIBRARIES})

install(TARGETS timelapse timelapse_assembly timelapse_deflicker timelapse_stabilize timelapse_capture
	RUNTIME DESTINATION bin COMPONENT applications
	LIBRARY DESTINATION lib COMPONENT libraries
	ARCHIVE DESTINATION lib/static COMPONENT libraries
	)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/TimeLapse
	DESTINATION include
	FILES_MATCHING PATTERN "*.h")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/TimeLapse/timelapse_version.h"
	DESTINATION include/TimeLapse)
