# SPDX-License-Identifier: AGPL-3.0-only
juce_add_plugin(FrenchcoreMachine
    PRODUCT_NAME "Frenchcore Machine"
    COMPANY_NAME "nocturn4"
    BUNDLE_ID com.nocturn4.frenchcoremachine
    PLUGIN_MANUFACTURER_CODE Nctr
    PLUGIN_CODE Fcmk
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${FCM_COPY_PLUGIN_AFTER_BUILD}
)

target_sources(FrenchcoreMachine PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_link_libraries(FrenchcoreMachine PRIVATE
    fcm_ui
    juce::juce_audio_utils
    juce::juce_dsp
)

fcm_apply_juce_options(FrenchcoreMachine)
