// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace fcm::dsp
{

// A fixed delay of whole samples (a ring buffer), for lining signals up with a rack's latency.
class DelayRing
{
  public:
    void prepare(int capacity);
    void reset() noexcept;
    // Writes `in` and returns what was written `delay` samples ago (clamped to the capacity).
    float process(float in, int delay) noexcept
    {
        data[static_cast<size_t>(write)] = in;
        const int size = static_cast<int>(data.size());
        const int clamped = delay < 0 ? 0 : (delay >= size ? size - 1 : delay);
        const int read = (write + size - clamped) % size;
        write = (write + 1) % size;
        return data[static_cast<size_t>(read)];
    }

  private:
    std::vector<float> data = std::vector<float>(1, 0.0f);
    int write = 0;
};

// What a bus does around its rack of effects (docs/specs/mixer-and-buses.md §1, §4): the mono input is
// copied to two channels for the rack; afterwards the output is
//   level * (clean * (1 - w) + processed * w)
// where `clean` is the input delayed by the rack's latency, and w is the weight of the Apply To region
// (1 for Whole, the punch weight for Punch, its complement for Tail), delayed the same way so it lines
// up with the processed sound. Level and region change smoothly. Real-time safe; allocates nothing.
class BusStage
{
  public:
    static constexpr double kSmoothSeconds = 0.010;

    // `maxBlock` is the longest chunk given to begin(); `maxLatency` the longest delay it will ask for.
    void prepare(double sampleRate, int maxBlock, int maxLatency);
    void reset() noexcept;
    void setParameters(const BusParameters& parameters, bool snap) noexcept;

    // Takes `count` samples of mono input with the punch weight of each, and fills both channels of
    // `rackInput` with the input. `latency` is the rack's latency in samples.
    void begin(const float* input, const float* punchWeight, int count, int latency,
               juce::AudioBuffer<float>& rackInput) noexcept;

    // Adds the bus output of the chunk to `destination` (both channels), given what the rack made.
    void finish(const juce::AudioBuffer<float>& processed, int count,
                juce::AudioBuffer<float>& destination) noexcept;

  private:
    DelayRing cleanRing;
    DelayRing weightRing;
    std::vector<float> clean;  // the delayed input of the chunk
    std::vector<float> weight; // the delayed weight of the chunk
    juce::SmoothedValue<float> level{1.0f};
    juce::SmoothedValue<float> whole{1.0f};
    juce::SmoothedValue<float> punch{0.0f};
    juce::SmoothedValue<float> tail{0.0f};
};

} // namespace fcm::dsp
