
if (NOT DEFINED TEST_DATA_DIR)
    set(TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/test/data")
endif()

include(CTest)

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
