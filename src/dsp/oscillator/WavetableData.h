// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <memory>
#include <span>
#include <vector>

namespace fcm::dsp
{

// Immutable, band-limited wavetable (docs/specs/oscillator-wavetable.md §4.1). Built off the audio
// thread; the oscillator only reads it.
//
// Layout: kNumMipLevels levels, each with numFrames frames of kFrameSize samples plus one guard
// sample (a copy of the first sample) so linear interpolation never needs to wrap. Mip level m keeps
// the harmonics 1 .. maxHarmonics(m); see Nigel Redmon, "A wavetable oscillator" (earlevel.com).
class WavetableData
{
  public:
    static constexpr int kFrameSize = 2048;
    static constexpr int kMaxFrames = 256;
    static constexpr int kNumMipLevels = 11;
    static constexpr int kFrameStride = kFrameSize + 1;

    // Highest harmonic kept by a mip level: 1024, 512, ... 1. Level 0 drops bin 1024 (the Nyquist
    // bin, whose phase is ambiguous), so it keeps 1023.
    static constexpr int maxHarmonics(int mipLevel) noexcept { return (kFrameSize / 2) >> mipLevel; }

    // `samples` holds numFrames consecutive frames of kFrameSize samples. Returns nullptr when the
    // size is not a non-zero multiple of kFrameSize or has more than kMaxFrames frames.
    static std::unique_ptr<const WavetableData> fromFrames(std::span<const float> samples);

    // One frame holding one cycle of a sine.
    static std::unique_ptr<const WavetableData> makeSine();

    int numFrames() const noexcept { return frames; }

    // Pointer to kFrameStride samples (kFrameSize + guard sample).
    const float* frame(int mipLevel, int frameIndex) const noexcept
    {
        return data.data() + (static_cast<size_t>(mipLevel) * static_cast<size_t>(frames) +
                              static_cast<size_t>(frameIndex)) *
                                 kFrameStride;
    }

  private:
    explicit WavetableData(int numFramesIn) : frames(numFramesIn) {}

    int frames;
    std::vector<float> data;
};

} // namespace fcm::dsp
