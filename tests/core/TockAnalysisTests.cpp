// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TockAnalysis.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;
using fcm::core::detectTransient;
using fcm::core::waveformPeaks;
using fcm::dsp::TockSample;

namespace
{
constexpr double kRate = 48000.0;

// A synthetic kick: silence, then at `onsetMs` a 40 Hz body that lasts and a 4 kHz click that fades
// with a 3 ms time constant over 12 ms.
TockSample syntheticKick(double onsetMs, double totalMs = 400.0)
{
    TockSample sample;
    sample.sampleRate = kRate;
    const auto length = static_cast<int>(totalMs * 0.001 * kRate);
    const auto onset = static_cast<int>(onsetMs * 0.001 * kRate);
    for (int i = 0; i < length; ++i)
    {
        double value = 0.0;
        if (i >= onset)
        {
            const double t = static_cast<double>(i - onset) / kRate;
            value += 0.5 * std::sin(2.0 * std::numbers::pi * 40.0 * t);
            if (t < 0.012)
                value += 0.5 * std::sin(2.0 * std::numbers::pi * 4000.0 * t) * std::exp(-t / 0.003);
        }
        sample.samples.push_back(static_cast<float>(value));
    }
    return sample;
}
} // namespace

TEST_CASE("Detect finds the onset and the end of the hit", "[core][tock][detect]")
{
    for (const double onsetMs : {5.0, 20.0, 0.0})
    {
        const auto sample = syntheticKick(onsetMs);
        const auto range = detectTransient(sample);
        const double lengthMs = static_cast<double>(sample.samples.size()) / kRate * 1000.0;
        const double startMs = range.start * lengthMs;
        const double endMs = range.end * lengthMs;

        INFO("onset " << onsetMs);
        // The start is just before the first hit (1 ms before it, or at the very beginning).
        REQUIRE_THAT(startMs, WithinAbs(std::max(onsetMs - 1.0, 0.0), 1.0));
        // The end is where the click has died away: more than the minimum 5 ms and less than the 80 ms limit.
        REQUIRE(endMs - startMs >= 5.0 - 0.1);
        REQUIRE(endMs - startMs <= 12.0);
    }
}

TEST_CASE("Detect gives the whole sample when there is nothing to find", "[core][tock][detect]")
{
    TockSample silent;
    silent.sampleRate = kRate;
    silent.samples.assign(4800, 0.0f);
    auto range = detectTransient(silent);
    REQUIRE(range.start == 0.0);
    REQUIRE(range.end == 1.0);

    TockSample flat = silent;
    flat.samples.assign(4800, 0.5f); // DC: nothing above 1 kHz
    range = detectTransient(flat);
    REQUIRE(range.start == 0.0);
    REQUIRE(range.end == 1.0);

    TockSample tiny;
    tiny.sampleRate = kRate;
    tiny.samples.assign(3, 1.0f);
    range = detectTransient(tiny);
    REQUIRE(range.start == 0.0);
    REQUIRE(range.end == 1.0);
}

TEST_CASE("The end never goes beyond 80 ms after the start", "[core][tock][detect]")
{
    // A high-frequency tone that never dies: the end is clamped.
    TockSample sample;
    sample.sampleRate = kRate;
    for (int i = 0; i < 48000; ++i)
        sample.samples.push_back(static_cast<float>(std::sin(2.0 * std::numbers::pi * 5000.0 * i / kRate)));
    const auto range = detectTransient(sample);
    REQUIRE_THAT((range.end - range.start) * 1000.0, WithinAbs(80.0, 1.0));
}

TEST_CASE("Waveform peaks are a normalised, decimated view", "[core][tock][detect]")
{
    TockSample sample;
    sample.sampleRate = kRate;
    sample.samples.assign(1000, 0.0f);
    sample.samples[10] = -0.5f;
    sample.samples[990] = 0.25f;

    const auto peaks = waveformPeaks(sample, 10);
    REQUIRE(peaks.size() == 10u);
    REQUIRE(peaks.front() == 1.0f);
    REQUIRE_THAT(peaks.back(), WithinAbs(0.5, 1.0e-6));
    REQUIRE(peaks[5] == 0.0f);

    REQUIRE(waveformPeaks(TockSample{}, 10).empty());
    REQUIRE(waveformPeaks(sample, 0).empty());
    // Fewer samples than bins still gives every bin a value.
    TockSample few;
    few.samples.assign(3, 0.5f);
    REQUIRE(waveformPeaks(few, 8).size() == 8u);
}
