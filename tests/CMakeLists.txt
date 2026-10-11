# SPDX-License-Identifier: AGPL-3.0-only
include(Catch)

# DSP tests link only juce_dsp, which enforces that fcm_dsp never depends on UI or plugin code.
juce_add_console_app(fcm_dsp_tests PRODUCT_NAME "fcm_dsp_tests")
target_sources(fcm_dsp_tests PRIVATE
    dsp/EngineTests.cpp
)
target_link_libraries(fcm_dsp_tests PRIVATE
    fcm_dsp
    juce::juce_dsp
    Catch2::Catch2WithMain
)
fcm_apply_juce_options(fcm_dsp_tests)
target_compile_definitions(fcm_dsp_tests PRIVATE JUCE_STANDALONE_APPLICATION=1)

catch_discover_tests(fcm_dsp_tests)
