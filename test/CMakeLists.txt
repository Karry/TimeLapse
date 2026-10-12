
if (NOT DEFINED TEST_DATA_DIR)
    set(TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/test/data")
endif()

include(CTest)

if(Catch2_FOUND)
    add_executable(deflicker_composition_test deflicker_composition_test.cpp)
    set_property(TARGET deflicker_composition_test PROPERTY CXX_STANDARD 17)
    target_include_directories(deflicker_composition_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(deflicker_composition_test
        PRIVATE timelapse Qt5::Core ${ImageMagick_LIBRARIES} Catch2::Catch2WithMain)
    add_test(NAME deflicker_composition_test COMMAND deflicker_composition_test)
endif()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C Debug)

add_custom_target(checkVerbose
    COMMAND ${CMAKE_CTEST_COMMAND} -V --output-on-failure -C Debug)

add_test(NAME "timelapse_stabilize_test"
    COMMAND $<TARGET_FILE:timelapse_stabilize> --verbose --output stab "${TEST_DATA_DIR}/sunrise"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME "timelapse_deflicker_test"
    COMMAND $<TARGET_FILE:timelapse_deflicker> --verbose --debug-view --output deflicker "${TEST_DATA_DIR}/sunrise"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME "timelapse_assembly_test"
    COMMAND $<TARGET_FILE:timelapse_assembly> --verbose --force --length 5 --blend-frames "${TEST_DATA_DIR}/sunrise"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME "timelapse_assembly_dryrun_test"
    COMMAND $<TARGET_FILE:timelapse_assembly> --verbose --force --dryrun --length 5 --blend-frames "${TEST_DATA_DIR}/sunrise"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
