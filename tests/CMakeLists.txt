# SPDX-License-Identifier: AGPL-3.0-only
include(Catch)

# DSP tests link only juce_dsp, which enforces that fcm_dsp never depends on UI or plugin code.
juce_add_console_app(fcm_dsp_tests PRODUCT_NAME "fcm_dsp_tests")
target_sources(fcm_dsp_tests PRIVATE
    dsp/AuditionTests.cpp
    dsp/BreakpointEnvelopeTests.cpp
    dsp/EngineTests.cpp
    dsp/EnvelopeEngineTests.cpp
    dsp/EnvelopeRendererTests.cpp
    dsp/NoteHandlingTests.cpp
    dsp/PublishedPointerTests.cpp
    dsp/PunchTailTests.cpp
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
    core/AuditionStateTests.cpp
    core/EnvelopeStateTests.cpp
    core/ParameterTests.cpp
)
target_link_libraries(fcm_core_tests PRIVATE
    fcm_core
    juce::juce_audio_processors
    juce::juce_dsp
    Catch2::Catch2WithMain
)
fcm_apply_juce_options(fcm_core_tests)
target_compile_definitions(fcm_core_tests PRIVATE JUCE_STANDALONE_APPLICATION=1)

catch_discover_tests(fcm_core_tests)
