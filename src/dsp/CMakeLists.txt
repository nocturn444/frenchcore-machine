# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_dsp INTERFACE)
target_sources(fcm_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/AuditionSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/BusStage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Mixer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/NoteHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Voice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope/BreakpointEnvelope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope/EnvelopePresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope/MusicalTime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/compressor/CompressorEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/distortion/DistortionEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/EffectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/EffectRack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/eq/EqEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/filter/FilterEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/peak/PeakEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/reverb/ReverbEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/stereo/StereoEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/transient/TransientEffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/EffectRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/RegisterEffects.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/noise/FactoryNoises.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/noise/NoisePlayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator/FactoryWavetables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator/WavetableData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator/WavetableOscillator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tock/FactoryTocks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tock/Tock.cpp
)
target_include_directories(fcm_dsp INTERFACE ${PROJECT_SOURCE_DIR}/src)
