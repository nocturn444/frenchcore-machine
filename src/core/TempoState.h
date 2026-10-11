// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <atomic>

namespace fcm::core
{

// The tempo that tempo-synced envelopes follow (docs/decisions/0007): the host's tempo in a
// plugin, the BPM control of the audition strip in the Standalone, 120 when nobody says. The audio
// thread writes it every block, the editor reads it; one atomic, no lock.
class TempoState
{
  public:
    static constexpr float kDefaultBpm = 120.0f;

    void set(float newBpm) noexcept { bpm.store(newBpm); }
    float get() const noexcept { return bpm.load(); }

  private:
    std::atomic<float> bpm{kDefaultBpm};
};

} // namespace fcm::core
