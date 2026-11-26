add_executable(kickcam_unit
    unit/version-t.cc
)
target_link_libraries(kickcam_unit PRIVATE kickcam GTest::gmock_main)
set_target_properties(kickcam_unit
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME kickcam COMMAND kickcam_unit WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
