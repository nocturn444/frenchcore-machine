# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_core INTERFACE)
target_sources(fcm_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/BoundedMidiCopy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EffectCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EffectCurveState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineParameterReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EqResponse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopeState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FxRackState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FactoryPresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NoiseImport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resample.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TockAnalysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NoiseSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ProjectState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMigrations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StoredAudio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TockImport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TockSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UntrustedXml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableImport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetableSelector.cpp
)
target_link_libraries(fcm_core INTERFACE fcm_dsp)
# The version shown in the editor's licence notice, kept equal to the project version.
target_compile_definitions(fcm_core INTERFACE FCM_VERSION="${PROJECT_VERSION}")
