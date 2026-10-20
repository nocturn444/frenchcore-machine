# SPDX-License-Identifier: AGPL-3.0-only
include(Catch)

# DSP tests link only juce_dsp, which enforces that fcm_dsp never depends on UI or plugin code.
juce_add_console_app(fcm_dsp_tests PRODUCT_NAME "fcm_dsp_tests")
target_sources(fcm_dsp_tests PRIVATE
    dsp/AllocationCounter.cpp
    dsp/AuditionTests.cpp
    dsp/BreakpointEnvelopeTests.cpp
    dsp/EffectCurveTests.cpp
    dsp/EffectRackTests.cpp
    dsp/MixerBusTests.cpp
    dsp/NoteTriggerTests.cpp
    dsp/fx/CompressorTests.cpp
    dsp/fx/DistortionTests.cpp
    dsp/fx/EqTests.cpp
    dsp/fx/FilterTests.cpp
    dsp/fx/StereoTests.cpp
    dsp/fx/TransientTests.cpp
    dsp/fx/RegisteredEffectsTests.cpp
    dsp/fx/ReverbTests.cpp
    dsp/EngineTests.cpp
    dsp/EnvelopeEngineTests.cpp
    dsp/EnvelopeRendererTests.cpp
    dsp/EnvelopeRobustnessTests.cpp
    dsp/FactoryWavetablesTests.cpp
    dsp/MusicalTimeTests.cpp
    dsp/NoiseTests.cpp
    dsp/NoteHandlingTests.cpp
    dsp/PublishedPointerTests.cpp
    dsp/PunchTailTests.cpp
    dsp/TockTests.cpp
    dsp/OscillatorRobustnessTests.cpp
    dsp/VoiceTests.cpp
    dsp/WavetableTests.cpp
)
target_link_libraries(fcm_dsp_tests PRIVATE
    fcm_dsp
    juce::juce_dsp
    Catch2::Catch2WithMain
)
fcm_apply_juce_options(fcm_dsp_tests)
target_compile_definitions(fcm_dsp_tests PRIVATE JUCE_STANDALONE_APPLICATION=1)

catch_discover_tests(fcm_dsp_tests)

# Core tests need the JUCE parameter classes (APVTS), so they link juce_audio_processors.
juce_add_console_app(fcm_core_tests PRODUCT_NAME "fcm_core_tests")
target_sources(fcm_core_tests PRIVATE
    core/AboutTextTests.cpp
    core/AuditionStateTests.cpp
    core/EffectCurveStateTests.cpp
    core/EnvelopeStateTests.cpp
    core/EqResponseTests.cpp
    core/FxCoreTests.cpp
    core/MixerParameterTests.cpp
    core/ParameterTests.cpp
    core/NoiseCoreTests.cpp
    core/PresetTests.cpp
    core/ProjectStateTests.cpp
    core/ReferenceRenderTests.cpp
    core/TempoStateTests.cpp
    core/BoundedMidiCopyTests.cpp
    core/StateLoaderTests.cpp
    core/TailLengthTests.cpp
    core/TockAnalysisTests.cpp
    core/TockImportTests.cpp
    core/TockSelectorTests.cpp
    core/UntrustedInputTests.cpp
    core/WavetableImportTests.cpp
    core/WavetableSelectorTests.cpp
    core/WavetableShapeTests.cpp
)
target_link_libraries(fcm_core_tests PRIVATE
    fcm_core
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    Catch2::Catch2WithMain
)
fcm_apply_juce_options(fcm_core_tests)
# The loader tests wait for the message thread with runDispatchLoopUntil, which JUCE only offers
# when modal loops are permitted.
target_compile_definitions(fcm_core_tests PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
    FCM_REFERENCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/reference-0.1.1"
    FCM_REFERENCE_DIR_020="${CMAKE_CURRENT_SOURCE_DIR}/data/reference-0.2.0")

catch_discover_tests(fcm_core_tests)
