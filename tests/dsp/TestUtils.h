// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace fcm::test
{

inline double centsBetween(double measuredHz, double expectedHz)
{
    return 1200.0 * std::log2(measuredHz / expectedHz);
}

// Frequency of a signal from the spacing of its rising zero crossings (linear interpolation between
// samples). Accurate to a small fraction of a sample over many cycles; independent of amplitude.
inline double measureFrequency(const float* samples, int numSamples, double sampleRate)
{
    double firstCrossing = -1.0;
    double lastCrossing = -1.0;
    int crossings = 0;

    for (int i = 1; i < numSamples; ++i)
    {
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f)
        {
            const double fraction = -static_cast<double>(samples[i - 1]) / (samples[i] - samples[i - 1]);
            const double position = (i - 1) + fraction;
            if (crossings == 0)
                firstCrossing = position;
            lastCrossing = position;
            ++crossings;
        }
    }

    if (crossings < 2)
        return 0.0;
    return (crossings - 1) * sampleRate / (lastCrossing - firstCrossing);
}

inline float peak(const std::vector<float>& samples)
{
    float result = 0.0f;
    for (float s : samples)
        result = std::max(result, std::abs(s));
    return result;
}

inline bool allFinite(const std::vector<float>& samples)
{
    return std::all_of(samples.begin(), samples.end(), [](float s) { return std::isfinite(s); });
}

inline double midiToHz(double note)
{
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

} // namespace fcm::test
