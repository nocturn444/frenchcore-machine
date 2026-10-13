// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/WavetableSelector.h"
#include "ui/ParameterControl.h"
#include "ui/SectionPanel.h"
#include "ui/WavetableDisplay.h"

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::ui
{

// One oscillator (docs/UI_LAYOUT.md): a header with On, the wavetable menu and a Load button; Pitch,
// Octave and Fine on one strip; the wavetable display; and the knobs. The same class serves both
// oscillators. It only asks for a file; reading it is the processor's job.
class OscillatorPanel : public SectionPanel
{
  public:
    // `onLoad(file)` is called on the message thread with the chosen WAV file.
    OscillatorPanel(juce::AudioProcessorValueTreeState& state, int oscillator,
                    core::WavetableSelector& selector, std::function<void(const juce::File&)> onLoad);

    void resized() override;

  private:
    void choose();

    std::function<void(const juce::File&)> loadRequested;
    std::unique_ptr<ParameterControl> onSwitch;
    std::unique_ptr<ParameterControl> wavetable;
    juce::TextButton loadButton{"Load..."};
    std::unique_ptr<ParameterControl> pitch;
    std::unique_ptr<ParameterControl> octave;
    std::unique_ptr<ParameterControl> fine;
    WavetableDisplay display;
    std::unique_ptr<ParameterControl> position;
    std::unique_ptr<ParameterControl> level;
    std::unique_ptr<ParameterControl> phase;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorPanel)
};

} // namespace fcm::ui
