# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.25)

project(FrenchcoreMachine VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FCM_BUILD_TESTS "Build the unit tests" ON)
option(FCM_COPY_PLUGIN_AFTER_BUILD "Copy built plugins into the system plugin folders" OFF)

include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(FCM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
