// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/AuditionPanel.h"

namespace fcm::ui
{

namespace
{
constexpr int kFirstNote = 24;
constexpr int kLastNote = 83;
constexpr int kWhiteKeys = 35; // white keys between those notes
constexpr int kVelocity = 100;
constexpr int kControlsWidth = 230;
constexpr int kMargin = 8;
constexpr int kMinOctave = 0;
constexpr int kMaxOctave = 8;
} // namespace

AuditionPanel::AuditionPanel(core::AuditionState& auditionState)
    : state(auditionState), keyboard(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    keyboardState.addListener(this);

    keyboard.setAvailableRange(kFirstNote, kLastNote);
    keyboard.setLowestVisibleKey(kFirstNote);
    keyboard.setScrollButtonsVisible(false);
    keyboard.setVelocity(static_cast<float>(kVelocity) / 127.0f, false);
    keyboard.setKeyPressBaseOctave(keyBaseOctave);
    addAndMakeVisible(keyboard);

    autoPlayButton.setTitle("Auto-play");
    autoPlayButton.onClick = [this] { state.setAutoPlay(autoPlayButton.getToggleState()); };
    addAndMakeVisible(autoPlayButton);

    bpmLabel.attachToComponent(&bpmSlider, true);
    bpmSlider.setTitle("BPM");
    bpmSlider.setRange(core::AuditionState::kMinBpm, core::AuditionState::kMaxBpm, 1.0);
    bpmSlider.setValue(core::AuditionState::kDefaultBpm, juce::dontSendNotification);
    bpmSlider.setDoubleClickReturnValue(true, core::AuditionState::kDefaultBpm);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 24);
    bpmSlider.onValueChange = [this] { state.setBpm(static_cast<float>(bpmSlider.getValue())); };
    addAndMakeVisible(bpmSlider);

    // Start from the documented defaults: the state is not saved between launches.
    state.setAutoPlay(false);
    state.setBpm(core::AuditionState::kDefaultBpm);
}

AuditionPanel::~AuditionPanel()
{
    if (listeningOn != nullptr)
        listeningOn->removeKeyListener(this);

    keyboardState.allNotesOff(0); // a key held while the window closes must not stay down
    keyboardState.removeListener(this);
}

void AuditionPanel::parentHierarchyChanged()
{
    // The key listener goes on the editor (our parent), so the computer keys work wherever the focus is.
    auto* parent = getParentComponent();
    if (parent == listeningOn)
        return;

    if (listeningOn != nullptr)
        listeningOn->removeKeyListener(this);
    listeningOn = parent;
    if (listeningOn != nullptr)
    {
        listeningOn->addKeyListener(this);
        keyboard.grabKeyboardFocus();
    }
}

void AuditionPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1d1f25));
}

void AuditionPanel::resized()
{
    auto area = getLocalBounds().reduced(kMargin);
    auto controls = area.removeFromLeft(kControlsWidth);
    area.removeFromLeft(kMargin);

    autoPlayButton.setBounds(controls.removeFromTop(32));
    controls.removeFromTop(kMargin);
    controls.removeFromLeft(44); // room for the label attached to the slider
    bpmSlider.setBounds(controls.removeFromTop(28));

    keyboard.setKeyWidth(static_cast<float>(area.getWidth()) / static_cast<float>(kWhiteKeys));
    keyboard.setBounds(area);
}

void AuditionPanel::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    state.sendNote(true, midiNoteNumber, juce::jlimit(1, 127, juce::roundToInt(velocity * 127.0f)));
}

void AuditionPanel::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    state.sendNote(false, midiNoteNumber, 0);
}

void AuditionPanel::shiftOctave(int octaves)
{
    keyBaseOctave = juce::jlimit(kMinOctave, kMaxOctave, keyBaseOctave + octaves);
    keyboard.setKeyPressBaseOctave(keyBaseOctave);
}

bool AuditionPanel::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;

    // Z and X move the computer-keyboard piano down and up one octave.
    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (character == 'z' || character == 'x')
    {
        shiftOctave(character == 'x' ? 1 : -1);
        return true;
    }

    return keyboard.keyPressed(key);
}

bool AuditionPanel::keyStateChanged(bool isKeyDown, juce::Component*)
{
    return keyboard.keyStateChanged(isKeyDown);
}

} // namespace fcm::ui
