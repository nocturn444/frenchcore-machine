// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/oscillator/WavetableData.h"

#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <numbers>

namespace fcm::dsp
{

std::unique_ptr<const WavetableData> WavetableData::fromFrames(std::span<const float> samples)
{
    if (samples.empty() || samples.size() % kFrameSize != 0)
        return nullptr;

    const auto numFrames = static_cast<int>(samples.size() / kFrameSize);
    if (numFrames > kMaxFrames)
        return nullptr;

    std::unique_ptr<WavetableData> table(new WavetableData(numFrames));
    table->data.resize(static_cast<size_t>(kNumMipLevels) * static_cast<size_t>(numFrames) * kFrameStride);

    // The real-only transforms need a buffer of twice the FFT size.
    juce::dsp::FFT fft(11);
    static_assert(kFrameSize == 2048, "FFT order above must match the frame size");
    std::vector<float> spectrum(static_cast<size_t>(2 * kFrameSize));
    std::vector<float> filtered(spectrum.size());

    for (int frameIndex = 0; frameIndex < numFrames; ++frameIndex)
    {
        std::fill(spectrum.begin(), spectrum.end(), 0.0f);
        std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(frameIndex) * kFrameSize, kFrameSize,
                    spectrum.begin());
        fft.performRealOnlyForwardTransform(spectrum.data(), true);

        for (int mip = 0; mip < kNumMipLevels; ++mip)
        {
            // Bins above the limit are zeroed. Complex bin k lives at [2k] (real) and [2k + 1] (imaginary).
            const int limit = std::min(maxHarmonics(mip), kFrameSize / 2 - 1);
            std::fill(filtered.begin(), filtered.end(), 0.0f);
            std::copy_n(spectrum.begin(), 2 * (limit + 1), filtered.begin());
            fft.performRealOnlyInverseTransform(filtered.data());

            float* destination =
                table->data.data() + (static_cast<size_t>(mip) * static_cast<size_t>(numFrames) +
                                      static_cast<size_t>(frameIndex)) *
                                         kFrameStride;
            std::copy_n(filtered.begin(), kFrameSize, destination);
            destination[kFrameSize] = destination[0];
        }
    }

    return table;
}

std::unique_ptr<const WavetableData> WavetableData::makeSine()
{
    std::vector<float> cycle(static_cast<size_t>(kFrameSize));
    for (size_t i = 0; i < cycle.size(); ++i)
        cycle[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi * static_cast<double>(i) / kFrameSize));

    return fromFrames(cycle);
}

} // namespace fcm::dsp
