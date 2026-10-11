# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_core INTERFACE)
target_sources(fcm_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineParameterReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopeState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.cpp
)
target_link_libraries(fcm_core INTERFACE fcm_dsp)
