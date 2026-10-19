// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace fcm::dsp
{

// The source mixer (docs/specs/mixer-and-buses.md): adds the stems of the voice into one mono sum per
// destination, each stem through its Mute and Solo and sent to its destination. Every change is
// smoothed over 10 ms, so switching never clicks, and a stem that changes destination crossfades
// between the old and the new sum. Real-time safe; allocates nothing.
class Mixer
{
  public:
    static constexpr double kSmoothSeconds = 0.010;

    void prepare(double sampleRate) noexcept;
    // `snap` jumps to the new values (the first call after prepare).
    void setParameters(const std::array<MixerSourceParameters, kNumStems>& sources, bool snap) noexcept;
    void reset() noexcept;

    // One sample: `stems` in, the three destination sums out (index = Destination).
    void process(const std::array<float, kNumStems>& stems,
                 std::array<float, kNumDestinations>& sums) noexcept;

  private:
    struct Channel
    {
        juce::SmoothedValue<float> audible{1.0f};
        std::array<juce::SmoothedValue<float>, kNumDestinations> route;
    };

    std::array<Channel, kNumStems> channels;
};

} // namespace fcm::dsp
