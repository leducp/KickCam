add_executable(kickcam_unit
    unit/version-t.cc
    unit/Error-t.cc
    unit/genapi/helpers.cc
    unit/genapi/Formula-t.cc
    unit/genapi/Node-t.cc
    unit/genapi/Nodes-t.cc
    unit/genapi/Parser-t.cc
    unit/genapi/RegisterNodes-t.cc
    unit/genapi/SwissKnifeNodes-t.cc
)
target_link_libraries(kickcam_unit PRIVATE kickcam GTest::gmock_main)
target_include_directories(kickcam_unit PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(kickcam_unit
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME kickcam COMMAND kickcam_unit WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
