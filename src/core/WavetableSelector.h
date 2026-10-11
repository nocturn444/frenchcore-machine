// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/PublishedPointer.h"
#include "dsp/oscillator/WavetableData.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// Keeps one oscillator's wavetable in step with its Wavetable parameter (docs/specs/wavetable-factory.md,
// docs/decisions/0008-wavetable-selection-and-publishing.md). A change of the parameter, from any
// thread, schedules the work on the message thread: build the band-limited table, publish it to the
// audio thread, let the publisher free the old one. The audio thread only reads `source()`.
class WavetableSelector : private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater
{
  public:
    // Publishes the table of the current parameter value at once, so it is ready before playback.
    WavetableSelector(juce::AudioProcessorValueTreeState& state, int oscillatorIndex);
    ~WavetableSelector() override;

    // The audio thread acquires its table here, once per block.
    dsp::PublishedPointer<dsp::WavetableData>& source() noexcept { return published; }

    // The entry the published table was built from.
    int currentEntry() const noexcept { return entry; }

    // Message thread. Publishes the table of the parameter's current value if it changed. Called by
    // the scheduled update; tests and the state loader may call it directly.
    void update();

  private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    int wantedEntry() const;

    juce::AudioProcessorValueTreeState& state;
    juce::String parameterId;
    std::atomic<float>* raw = nullptr;
    dsp::PublishedPointer<dsp::WavetableData> published;
    int entry = -1;
};

} // namespace fcm::core
