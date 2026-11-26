cmake_minimum_required(VERSION 3.15)
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
# INTERFACE until the first sources land (docs/PLAN.md, phase 1).
add_library(kickcam INTERFACE)

include(GNUInstallDirs)
target_include_directories(kickcam INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

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
