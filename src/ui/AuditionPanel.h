// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/AuditionState.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace fcm::ui
{

// The Standalone's strip for hearing the kick without a DAW: an on-screen keyboard (also played
// with the computer keyboard) and an auto-play loop with a tempo (docs/specs/standalone-audition.md).
// It talks to the audio thread only through core::AuditionState.
class AuditionPanel : public juce::Component,
                      public juce::KeyListener,
                      private juce::MidiKeyboardState::Listener
{
  public:
    static constexpr int kHeight = 110;

    explicit AuditionPanel(core::AuditionState& state);
    ~AuditionPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // Registered on the editor so the computer keys work wherever the focus is inside the window.
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;
    using juce::Component::keyPressed;
    using juce::Component::keyStateChanged;

  private:
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                       float velocity) override;

    void shiftOctave(int octaves);

    core::AuditionState& state;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::ToggleButton autoPlayButton{"Auto-play"};
    juce::Label bpmLabel{{}, "BPM"};
    juce::Slider bpmSlider{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    int keyBaseOctave = 3; // with the default key map the lower letter row starts at MIDI note 36 (C1)
    juce::Component* listeningOn = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuditionPanel)
};

} // namespace fcm::ui
