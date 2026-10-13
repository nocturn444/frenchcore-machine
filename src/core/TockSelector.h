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

    // The menu entry of the user sample: the entry after the last factory one.
    static int userEntry() noexcept;

    // Message thread. Takes a user sample (checked again here) and publishes it if the User entry is
    // selected. Returns false and changes nothing when it is not valid. The caller then selects the
    // User entry.
    bool loadUser(dsp::TockSample sample, const juce::String& name);
    void clearUser();
    bool hasUser() const noexcept { return !userSample.samples.empty(); }
    const juce::String& userName() const noexcept { return userDisplayName; }

    // Saved form: a node "UserTockSample" with the name, the sample rate and the samples as base64
    // float32; an invalid ValueTree when there is no user sample.
    juce::ValueTree toValueTree() const;
    // Restores from a saved node, which is untrusted input. A missing or unusable node leaves no
    // user sample and returns false.
    bool restoreFrom(const juce::ValueTree& node);

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
    bool dirty = false;
    dsp::TockSample userSample;
    juce::String userDisplayName;
};

} // namespace fcm::core
