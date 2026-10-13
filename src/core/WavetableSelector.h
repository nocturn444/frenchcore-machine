// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/PublishedPointer.h"
#include "dsp/oscillator/WavetableData.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

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

    // What the display shows: the raw frames of the published table (numFrames * 2048 samples), its
    // name, and a broadcaster that announces every change. Message thread.
    const std::vector<float>& displayFrames() const noexcept { return shownFrames; }
    juce::String displayName() const;
    juce::ChangeBroadcaster& changes() noexcept { return broadcaster; }

    // The entry the published table was built from.
    int currentEntry() const noexcept { return entry; }

    // The menu entry of the user wavetable: the entry after the last factory one.
    static int userEntry() noexcept;

    // Message thread. Takes the frames of a user wavetable (already checked by importWavetable, or
    // checked again here) and publishes it if the User entry is selected. Returns false and changes
    // nothing when the frames are not valid. The caller then selects the User entry.
    bool loadUser(std::vector<float> frames, const juce::String& name);
    void clearUser();
    bool hasUser() const noexcept { return !userFrames.empty(); }
    const juce::String& userName() const noexcept { return userDisplayName; }

    // Saved form: a node "UserWavetable1" / "UserWavetable2" with the name and the frames as
    // base64 float32; an invalid ValueTree when there is no user wavetable.
    juce::ValueTree toValueTree() const;
    // Restores from a saved node, which is untrusted input. A missing or unusable node leaves no
    // user wavetable and returns false.
    bool restoreFrom(const juce::ValueTree& node);

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
    std::vector<float> shownFrames;
    juce::ChangeBroadcaster broadcaster;
    bool dirty = false; // the user frames changed since the last build
    int oscillator = 0;
    std::vector<float> userFrames;
    juce::String userDisplayName;
};

} // namespace fcm::core
