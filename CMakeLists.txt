cmake_minimum_required(VERSION 3.20)
project(kickcam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find conan-generated CMake package configs in the build directory
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR})

option(BUILD_UNIT_TESTS "Build unit tests" OFF)
option(ENABLE_TSAN      "Enable ThreadSanitizer" OFF)
option(ENABLE_ASAN      "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN     "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_WERROR    "Treat warnings as errors (CI)" OFF)

add_compile_options(-Wall -Wextra)
if(ENABLE_WERROR)
    add_compile_options(-Werror)
endif()

# TSAN cannot share a process with ASAN/UBSAN runtimes; ASAN+UBSAN together is fine.
if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_UBSAN))
    message(FATAL_ERROR "ENABLE_TSAN cannot be combined with ENABLE_ASAN or ENABLE_UBSAN")
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -g)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -g)
    add_link_options(-fsanitize=undefined)
endif()

# --- Library ---
find_package(tinyxml2 REQUIRED CONFIG)

add_library(kickcam STATIC
    src/Error.cc
    src/genapi/NodeBase.cc
    src/genapi/NodeMap.cc
    src/genapi/Nodes.cc
    src/genapi/Parser.cc
    src/genapi/RegisterNodes.cc
    src/genapi/Value.cc
)

include(GNUInstallDirs)
target_include_directories(kickcam
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(kickcam PRIVATE tinyxml2::tinyxml2)
target_compile_features(kickcam PUBLIC cxx_std_23)

# The wheel folds the static library into the Python module, a shared object.
if(SKBUILD)
    set_target_properties(kickcam PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# --- Version header (generated from git: cmake/version.cmake) ---
include(cmake/version.cmake)

# --- Install, pkg-config, find_package ---
include(cmake/install.cmake)

# --- Tests ---
if(BUILD_UNIT_TESTS)
    find_package(GTest REQUIRED CONFIG)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Python bindings (built only when invoked via scikit-build-core) ---
if(SKBUILD)
    add_subdirectory(py_bindings)
endif()
