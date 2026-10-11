// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/FixedAmpEnvelope.h"
#include "dsp/envelope/FixedPitchEnvelope.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_core/juce_core.h>
#include <vector>

using fcm::dsp::FixedAmpEnvelope;
using fcm::dsp::FixedPitchEnvelope;

TEST_CASE("FixedAmpEnvelope attacks linearly in 1 ms, then decays 60 dB in 0.5 s", "[dsp][envelope]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        FixedAmpEnvelope envelope;
        envelope.prepare(sampleRate);
        REQUIRE_FALSE(envelope.isActive());

        envelope.start();
        REQUIRE(envelope.isActive());

        const int attackSamples = static_cast<int>(std::lround(0.001 * sampleRate));
        float previous = 0.0f;
        float value = 0.0f;
        for (int i = 0; i < attackSamples; ++i)
        {
            value = envelope.next();
            REQUIRE(value > previous);
            previous = value;
        }
        REQUIRE_THAT(value, Catch::Matchers::WithinAbs(1.0, 0.03));

        // The decay is exponential: after the peak, 0.5 s later the level is 1/1000 (-60 dB).
        float peak = value;
        for (int i = 0; i < static_cast<int>(0.5 * sampleRate); ++i)
            value = envelope.next();
        const double dropDb = 20.0 * std::log10(value / peak);
        INFO("sampleRate " << sampleRate);
        REQUIRE_THAT(dropDb, Catch::Matchers::WithinAbs(-60.0, 0.2));
    }
}

TEST_CASE("FixedAmpEnvelope goes idle within its stated maximum length", "[dsp][envelope]")
{
    FixedAmpEnvelope envelope;
    envelope.prepare(48000.0);
    envelope.start();

    int samples = 0;
    float last = 1.0f;
    while (envelope.isActive())
    {
        last = envelope.next();
        ++samples;
        REQUIRE(samples < 48000 * 2);
    }

    REQUIRE(juce::exactlyEqual(last, 0.0f));
    REQUIRE(samples <= static_cast<int>(FixedAmpEnvelope::kMaxLengthSeconds * 48000.0) + 4); // rounding
}

TEST_CASE("FixedAmpEnvelope restarts from zero and is deterministic after reset", "[dsp][envelope]")
{
    FixedAmpEnvelope envelope;
    envelope.prepare(48000.0);

    auto run = [&]
    {
        std::vector<float> out;
        envelope.start();
        for (int i = 0; i < 3000; ++i)
            out.push_back(envelope.next());
        return out;
    };

    const auto first = run();
    envelope.reset();
    REQUIRE_FALSE(envelope.isActive());
    REQUIRE(run() == first);
    REQUIRE(first.front() > 0.0f);
    REQUIRE(first.front() < 0.05f); // starts near zero, no jump
}

TEST_CASE("FixedAmpEnvelope release fades linearly from the current level in 30 ms", "[dsp][envelope]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        // Released early in the decay, and released while still in the attack.
        for (int samplesBeforeRelease :
             {static_cast<int>(0.05 * sampleRate), static_cast<int>(0.0004 * sampleRate)})
        {
            FixedAmpEnvelope envelope;
            envelope.prepare(sampleRate);
            envelope.start();

            float level = 0.0f;
            for (int i = 0; i < samplesBeforeRelease; ++i)
                level = envelope.next();
            REQUIRE(level > 0.0f);

            envelope.release();
            REQUIRE(envelope.isActive());

            const int releaseSamples =
                static_cast<int>(std::lround(FixedAmpEnvelope::kReleaseMs * 0.001 * sampleRate));
            float previous = level;
            float firstStep = 0.0f;
            int samples = 0;
            while (envelope.isActive())
            {
                const float value = envelope.next();
                INFO("sampleRate " << sampleRate << " released after " << samplesBeforeRelease);
                REQUIRE(value <= previous); // never rises again
                if (samples == 0)
                    firstStep = previous - value;
                previous = value;
                ++samples;
                REQUIRE(samples <= releaseSamples + 2);
            }

            // Linear: the first step is the whole level over the release length.
            REQUIRE_THAT(firstStep, Catch::Matchers::WithinAbs(level / releaseSamples, level * 0.01f));
            REQUIRE(samples >= releaseSamples - 2);
            REQUIRE(juce::exactlyEqual(previous, 0.0f));
        }
    }
}

TEST_CASE("FixedAmpEnvelope release does nothing when idle and cannot be restarted", "[dsp][envelope]")
{
    FixedAmpEnvelope envelope;
    envelope.prepare(48000.0);

    envelope.release();
    REQUIRE_FALSE(envelope.isActive());

    envelope.start();
    for (int i = 0; i < 500; ++i)
        envelope.next();
    envelope.release();
    const float afterFirstStep = envelope.next();
    envelope.release(); // a second release must not restart the ramp from the lower level
    const float afterSecondStep = envelope.next();
    const float step = afterFirstStep - afterSecondStep;
    REQUIRE(step > 0.0f);

    // Constant step: the ramp did not change slope.
    const float third = envelope.next();
    REQUIRE_THAT(afterSecondStep - third, Catch::Matchers::WithinAbs(step, 1.0e-6));
}

TEST_CASE("FixedPitchEnvelope falls from +48 semitones to exactly 0 in 90 ms", "[dsp][envelope]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        FixedPitchEnvelope envelope;
        envelope.prepare(sampleRate);
        envelope.start();

        const int dropSamples = static_cast<int>(std::lround(0.09 * sampleRate));

        float previous = envelope.next();
        REQUIRE_THAT(previous, Catch::Matchers::WithinAbs(48.0, 1.0e-4));

        for (int i = 1; i < dropSamples; ++i)
        {
            const float value = envelope.next();
            INFO("sampleRate " << sampleRate << " sample " << i);
            REQUIRE(value < previous);
            REQUIRE(value >= 0.0f);
            previous = value;
        }

        // From the end of the drop the offset is exactly zero: the tail sits on the note's pitch.
        for (int i = 0; i < 1000; ++i)
            REQUIRE(juce::exactlyEqual(envelope.next(), 0.0f));
        REQUIRE_FALSE(envelope.isActive());
    }
}

TEST_CASE("FixedPitchEnvelope is deterministic after reset", "[dsp][envelope]")
{
    FixedPitchEnvelope envelope;
    envelope.prepare(48000.0);

    auto run = [&]
    {
        std::vector<float> out;
        envelope.start();
        for (int i = 0; i < 6000; ++i)
            out.push_back(envelope.next());
        return out;
    };

    const auto first = run();
    envelope.reset();
    REQUIRE(run() == first);
}
