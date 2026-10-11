// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/AuditionSequencer.h"
#include "dsp/engine/NoteQueue.h"

#include <atomic>
#include <juce_core/juce_core.h>

namespace fcm::core
{

// What the Standalone's audition strip shares between the message thread and the audio thread
// (docs/specs/standalone-audition.md). Deliberately not parameters: the host never sees them and
// they are not saved. Plain atomics and a lock-free queue, so the audio thread never waits.
class AuditionState
{
  public:
    static constexpr float kDefaultBpm = 200.0f;
    static constexpr float kMinBpm = dsp::AuditionSequencer::kMinBpm;
    static constexpr float kMaxBpm = dsp::AuditionSequencer::kMaxBpm;

    // Message thread.
    void setAutoPlay(bool shouldPlay) noexcept { autoPlay.store(shouldPlay); }
    void setBpm(float newBpm) noexcept { bpm.store(juce::jlimit(kMinBpm, kMaxBpm, newBpm)); }
    bool sendNote(bool on, int note, int velocity) noexcept { return queue.push({on, note, velocity}); }

    // Audio thread.
    bool isAutoPlay() const noexcept { return autoPlay.load(); }
    float getBpm() const noexcept { return bpm.load(); }
    bool popNote(dsp::LiveNote& event) noexcept { return queue.pop(event); }

  private:
    std::atomic<bool> autoPlay{false};
    std::atomic<float> bpm{kDefaultBpm};
    dsp::NoteQueue queue;
};

} // namespace fcm::core
