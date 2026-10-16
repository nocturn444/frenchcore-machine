// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Resample.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::core
{

// Brings `input` from `rate` down to kResampleTargetRate with a Hann-windowed sinc low-pass (12 zero
// crossings each side at the output rate), so a long file at 96 or 192 kHz is stored at most 480000
// samples long without folding its highs back into the audible band.
std::vector<float> resampleDownTo48k(const std::vector<float>& input, double rate)
{
    const double ratio = rate / kResampleTargetRate; // > 1
    const auto outLength = static_cast<size_t>(static_cast<double>(input.size()) / ratio);
    constexpr double kCutoff = 0.45;    // of the output rate
    constexpr double kHalfWidth = 12.0; // zero crossings of the output rate

    std::vector<float> out(outLength);
    for (size_t n = 0; n < outLength; ++n)
    {
        const double centre = static_cast<double>(n) * ratio;
        const auto first = static_cast<long long>(std::ceil(centre - kHalfWidth * ratio));
        const auto last = static_cast<long long>(std::floor(centre + kHalfWidth * ratio));
        double sum = 0.0, weight = 0.0;
        for (long long i = first; i <= last; ++i)
        {
            if (i < 0 || i >= static_cast<long long>(input.size()))
                continue;
            const double x = (static_cast<double>(i) - centre) / ratio; // in output samples
            const double window = 0.5 + 0.5 * std::cos(std::numbers::pi * x / kHalfWidth);
            const double arg = 2.0 * kCutoff * x;
            const double sinc =
                std::abs(arg) < 1.0e-12 ? 1.0 : std::sin(std::numbers::pi * arg) / (std::numbers::pi * arg);
            const double k = 2.0 * kCutoff * sinc * window;
            sum += k * input[static_cast<size_t>(i)];
            weight += k;
        }
        out[n] = static_cast<float>(weight > 1.0e-9 ? sum / weight : 0.0);
    }
    return out;
}

} // namespace fcm::core
