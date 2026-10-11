// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/PublishedPointer.h"
#include "dsp/tock/TockSample.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// Keeps the Tock's sample in step with its Sample parameter (docs/specs/tock.md §4), the same way
// WavetableSelector does for a wavetable: a change of the parameter, from any thread, schedules the
// work on the message thread, which builds the sample and publishes it. The audio thread only
// reads `source()`.
class TockSelector : private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater
{
  public:
    // Publishes the sample of the current parameter value at once.
    explicit TockSelector(juce::AudioProcessorValueTreeState& state);
    ~TockSelector() override;

    dsp::PublishedPointer<dsp::TockSample>& source() noexcept { return published; }
    int currentEntry() const noexcept { return entry; }

    // Message thread. Publishes the sample of the parameter's current value if it changed.
    void update();

  private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    int wantedEntry() const;

    juce::AudioProcessorValueTreeState& state;
    std::atomic<float>* raw = nullptr;
    dsp::PublishedPointer<dsp::TockSample> published;
    int entry = -1;
};

} // namespace fcm::core
