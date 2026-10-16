// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TockAnalysis.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::core
{

namespace
{
constexpr double kOnsetThreshold = 0.0316227766; // -30 dB of the peak
constexpr double kLeadMs = 1.0;
constexpr double kEnvelopeMs = 1.0;
constexpr double kHighPassHz = 1000.0;
constexpr double kEndDrop = 0.1; // -20 dB
constexpr double kMinLengthMs = 5.0;
constexpr double kMaxLengthMs = 80.0;
} // namespace

std::vector<float> waveformPeaks(const dsp::TockSample& sample, int bins)
{
    if (sample.samples.empty() || bins < 1)
        return {};

    const auto length = sample.samples.size();
    std::vector<float> peaks(static_cast<size_t>(bins), 0.0f);
    float largest = 0.0f;
    for (size_t i = 0; i < length; ++i)
    {
        const auto bin = std::min(static_cast<size_t>(bins) - 1, i * static_cast<size_t>(bins) / length);
        const float magnitude = std::abs(sample.samples[i]);
        if (std::isfinite(magnitude))
            peaks[bin] = std::max(peaks[bin], magnitude);
    }
    for (float p : peaks)
        largest = std::max(largest, p);
    if (largest > 0.0f)
        for (float& p : peaks)
            p /= largest;
    return peaks;
}

TransientRange detectTransient(const dsp::TockSample& sample)
{
    const auto& x = sample.samples;
    const double rate = sample.sampleRate;
    if (x.size() < 16 || !(rate > 0.0))
        return {};

    float peak = 0.0f;
    for (float s : x)
        peak = std::max(peak, std::abs(s));
    if (!(peak > 1.0e-6f))
        return {};

    size_t onset = 0;
    while (onset < x.size() && std::abs(x[onset]) < kOnsetThreshold * peak)
        ++onset;
    const auto lead = static_cast<size_t>(kLeadMs * 0.001 * rate);
    const size_t start = onset > lead ? onset - lead : 0;

    // One-pole high-pass at 1 kHz, then a 1 ms moving mean of the square.
    const double pole = std::exp(-2.0 * std::numbers::pi * kHighPassHz / rate);
    const auto window = std::max<size_t>(1, static_cast<size_t>(kEnvelopeMs * 0.001 * rate));
    std::vector<double> squared(x.size());
    double previousIn = x[0], previousOut = 0.0; // starting at rest, so a constant gives nothing
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double out = pole * (previousOut + x[i] - previousIn);
        previousIn = x[i];
        previousOut = out;
        squared[i] = out * out;
    }

    std::vector<double> envelope(x.size());
    double sum = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        sum += squared[i];
        if (i >= window)
            sum -= squared[i - window];
        envelope[i] = std::sqrt(std::max(sum, 0.0) / static_cast<double>(window));
    }

    const auto maxIt = std::max_element(envelope.begin(), envelope.end());
    if (!(*maxIt > 1.0e-9))
        return {}; // nothing above 1 kHz: a flat or very dull sample

    const auto minEnd = start + static_cast<size_t>(kMinLengthMs * 0.001 * rate);
    const auto maxEnd = start + static_cast<size_t>(kMaxLengthMs * 0.001 * rate);
    size_t end = maxEnd;
    for (size_t i = static_cast<size_t>(maxIt - envelope.begin()); i < envelope.size(); ++i)
        if (envelope[i] < *maxIt * kEndDrop)
        {
            end = i;
            break;
        }
    end = std::clamp(end, minEnd, maxEnd);
    end = std::min(end, x.size());

    const auto length = static_cast<double>(x.size());
    return {static_cast<double>(start) / length, static_cast<double>(end) / length};
}

} // namespace fcm::core
