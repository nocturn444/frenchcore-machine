# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_dsp INTERFACE)
target_sources(fcm_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/AuditionSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/NoteHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Voice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope/BreakpointEnvelope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope/EnvelopePresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope/MusicalTime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator/FactoryWavetables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator/WavetableData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator/WavetableOscillator.cpp
)
target_include_directories(fcm_dsp INTERFACE ${PROJECT_SOURCE_DIR}/src)
