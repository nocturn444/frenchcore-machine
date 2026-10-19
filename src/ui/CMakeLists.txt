# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_ui INTERFACE)
target_sources(fcm_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/AuditionPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EffectDecorations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EffectsBox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopeEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EqDisplay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FxRackPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixerPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/OscillatorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SectionPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TockWaveform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableDisplay.cpp
)
target_link_libraries(fcm_ui INTERFACE fcm_core)
