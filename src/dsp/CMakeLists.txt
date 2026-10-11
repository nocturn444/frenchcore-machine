# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_dsp INTERFACE)
target_sources(fcm_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Engine.cpp
)
target_include_directories(fcm_dsp INTERFACE ${PROJECT_SOURCE_DIR}/src)
