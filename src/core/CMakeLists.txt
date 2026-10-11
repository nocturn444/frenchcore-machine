# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_core INTERFACE)
target_sources(fcm_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/BoundedMidiCopy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineParameterReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopeState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMigrations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TockSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableImport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableSelector.cpp
)
target_link_libraries(fcm_core INTERFACE fcm_dsp)
