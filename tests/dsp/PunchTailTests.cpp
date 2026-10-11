// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Voice.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::EngineParameters;
using fcm::dsp::Voice;
using fcm::dsp::WavetableData;

namespace
{
constexpr double kSampleRate = 48000.0;

struct Fixture
{
    explicit Fixture(const EngineParameters& parameters) : sine(WavetableData::makeSine())
    {
        voice.prepare(kSampleRate, sine.get());
        voice.setParameters(parameters, true);
    }

    std::vector<float> render(int samples)
    {
        std::vector<float> out(static_cast<size_t>(samples));
        for (auto& s : out)
            s = voice.process(36.0f);
        return out;
    }

    std::unique_ptr<const WavetableData> sine;
    Voice voice;
};

int ms(double milliseconds)
{
    return static_cast<int>(std::lround(milliseconds * 0.001 * kSampleRate));
}

EngineParameters withBalance(float balance, float splitMs = 80.0f)
{
    EngineParameters parameters;
    parameters.punchBalance = balance;
    parameters.punchSplitMs = splitMs;
    return parameters;
}

std::vector<float> kick(const EngineParameters& parameters, int samples)
{
    Fixture fixture(parameters);
    fixture.voice.trigger(1.0f);
    return fixture.render(samples);
}
} // namespace

TEST_CASE("Balance 0 leaves the voice untouched whatever the split", "[dsp][punchtail]")
{
    const auto reference = kick(withBalance(0.0f, 80.0f), ms(400));
    for (float split : {5.0f, 30.0f, 200.0f, 500.0f})
        REQUIRE(kick(withBalance(0.0f, split), ms(400)) == reference);
}

TEST_CASE("Balance -100 keeps the punch and silences the tail", "[dsp][punchtail]")
{
    const auto reference = kick(withBalance(0.0f), ms(400));
    const auto balanced = kick(withBalance(-1.0f), ms(400));

    // Before the crossfade starts (split - 2 ms) the voice is identical.
    for (int i = 0; i < ms(77.9); ++i)
        REQUIRE(juce::exactlyEqual(balanced[static_cast<size_t>(i)], reference[static_cast<size_t>(i)]));
    // After it ends (split + 2 ms) nothing is left.
    for (int i = ms(82.1); i < ms(400); ++i)
        REQUIRE(juce::exactlyEqual(balanced[static_cast<size_t>(i)], 0.0f));
    REQUIRE(fcm::test::peak(std::vector<float>(reference.begin() + ms(100), reference.end())) > 0.3f);
}

TEST_CASE("Balance +100 keeps the tail and silences the punch", "[dsp][punchtail]")
{
    const auto reference = kick(withBalance(0.0f), ms(400));
    const auto balanced = kick(withBalance(1.0f), ms(400));

    for (int i = 0; i < ms(77.9); ++i)
        REQUIRE(juce::exactlyEqual(balanced[static_cast<size_t>(i)], 0.0f));
    for (int i = ms(82.1); i < ms(400); ++i)
        REQUIRE(juce::exactlyEqual(balanced[static_cast<size_t>(i)], reference[static_cast<size_t>(i)]));
}

TEST_CASE("Balance of 50 percent halves the quieter region", "[dsp][punchtail]")
{
    const auto reference = kick(withBalance(0.0f), ms(300));

    const auto favourPunch = kick(withBalance(-0.5f), ms(300));
    for (int i = ms(90); i < ms(300); ++i)
        REQUIRE_THAT(favourPunch[static_cast<size_t>(i)],
                     WithinAbs(0.5f * reference[static_cast<size_t>(i)], 1.0e-6));

    const auto favourTail = kick(withBalance(0.5f), ms(300));
    for (int i = 100; i < ms(70); ++i)
        REQUIRE_THAT(favourTail[static_cast<size_t>(i)],
                     WithinAbs(0.5f * reference[static_cast<size_t>(i)], 1.0e-6));
}

TEST_CASE("The change between punch and tail is smooth and 4 ms wide", "[dsp][punchtail]")
{
    const auto reference = kick(withBalance(0.0f), ms(200));
    const auto balanced = kick(withBalance(-1.0f), ms(200));

    // The applied gain, wherever the reference is far enough from zero to read it.
    float previous = 1.0f;
    int readings = 0;
    for (int i = ms(77.5); i <= ms(82.5); ++i)
    {
        const float ref = reference[static_cast<size_t>(i)];
        if (std::abs(ref) < 0.05f)
            continue;
        const float gain = balanced[static_cast<size_t>(i)] / ref;
        REQUIRE(gain <= previous + 1.0e-4f); // monotonic from the punch gain (1) down to the tail gain (0)
        REQUIRE(gain >= -1.0e-4f);
        previous = gain;
        ++readings;
    }
    REQUIRE(readings > 5);

    // Centred on the split: half way there at 80 ms.
    int centre = ms(80.0);
    while (std::abs(reference[static_cast<size_t>(centre)]) < 0.05f)
        ++centre;
    if (centre < ms(81.0))
    {
        const float gain = balanced[static_cast<size_t>(centre)] / reference[static_cast<size_t>(centre)];
        REQUIRE(gain > 0.1f);
        REQUIRE(gain < 0.9f);
    }
}

TEST_CASE("The split moves with the parameter", "[dsp][punchtail]")
{
    for (float split : {20.0f, 150.0f, 300.0f})
    {
        const auto out = kick(withBalance(-1.0f, split), ms(500));
        for (int i = ms(split + 2.1); i < ms(500); ++i)
            REQUIRE(juce::exactlyEqual(out[static_cast<size_t>(i)], 0.0f));
        REQUIRE(fcm::test::peak(std::vector<float>(out.begin() + ms(split - 15.0),
                                                   out.begin() + ms(split - 3.0))) > 0.3f);
    }
}

TEST_CASE("The split is counted from each note's own start", "[dsp][punchtail]")
{
    Fixture fixture(withBalance(-1.0f, 80.0f));
    fixture.voice.trigger(1.0f);
    fixture.render(ms(500)); // the first kick is long past its split
    fixture.voice.trigger(1.0f);
    const auto out = fixture.render(ms(300));

    // The new kick has its punch again, up to its own split time.
    REQUIRE(fcm::test::peak(std::vector<float>(out.begin() + ms(20), out.begin() + ms(70))) > 0.3f);
    for (int i = ms(85); i < ms(300); ++i)
        REQUIRE(juce::exactlyEqual(out[static_cast<size_t>(i)], 0.0f));
}

TEST_CASE("Moving the balance while sounding does not click", "[dsp][punchtail][click]")
{
    Fixture fixture(withBalance(0.0f));
    fixture.voice.trigger(1.0f);
    fixture.render(ms(200));

    auto moved = withBalance(1.0f);
    fixture.voice.setParameters(
        moved, false); // the tail is what plays now: it ramps from full to 0 only for the punch
    moved = withBalance(-1.0f);
    fixture.voice.setParameters(moved, false);
    const auto out = fixture.render(ms(100));

    float largestStep = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        largestStep = std::max(largestStep, std::abs(out[i] - out[i - 1]));
    // 65 Hz at level 0.7: the signal's own slope is below 0.01 per sample; the 10 ms ramp adds a little.
    REQUIRE(largestStep < 0.02f);
    REQUIRE(fcm::test::allFinite(out));
}

namespace
{
EngineParameters withLevels(float punchLevel, float tailLevel, float balance = 0.0f)
{
    EngineParameters parameters;
    parameters.punchLevel = punchLevel;
    parameters.tailLevel = tailLevel;
    parameters.punchBalance = balance;
    return parameters;
}
} // namespace

TEST_CASE("Punch level scales the punch region and leaves the tail alone", "[dsp][punchtail][levels]")
{
    const auto reference = kick(withLevels(1.0f, 1.0f), ms(300));
    const auto quieter = kick(withLevels(0.5f, 1.0f), ms(300));

    for (int i = 0; i < ms(77.9); ++i)
        REQUIRE_THAT(quieter[static_cast<size_t>(i)],
                     WithinAbs(0.5f * reference[static_cast<size_t>(i)], 1.0e-6));
    for (int i = ms(82.1); i < ms(300); ++i)
        REQUIRE(juce::exactlyEqual(quieter[static_cast<size_t>(i)], reference[static_cast<size_t>(i)]));
}

TEST_CASE("Tail level scales the tail region and leaves the punch alone", "[dsp][punchtail][levels]")
{
    const auto reference = kick(withLevels(1.0f, 1.0f), ms(300));
    const auto louder = kick(withLevels(1.0f, 2.0f), ms(300));

    for (int i = 0; i < ms(77.9); ++i)
        REQUIRE(juce::exactlyEqual(louder[static_cast<size_t>(i)], reference[static_cast<size_t>(i)]));
    for (int i = ms(82.1); i < ms(300); ++i)
        REQUIRE_THAT(louder[static_cast<size_t>(i)],
                     WithinAbs(2.0f * reference[static_cast<size_t>(i)], 1.0e-6));
}

TEST_CASE("Levels multiply with the balance", "[dsp][punchtail][levels]")
{
    const auto reference = kick(withLevels(1.0f, 1.0f), ms(300));
    // Balance -50 % halves the tail; a tail level of 0.5 halves it again.
    const auto both = kick(withLevels(1.0f, 0.5f, -0.5f), ms(300));

    for (int i = ms(82.1); i < ms(300); ++i)
        REQUIRE_THAT(both[static_cast<size_t>(i)],
                     WithinAbs(0.25f * reference[static_cast<size_t>(i)], 1.0e-6));
    for (int i = 0; i < ms(77.9); ++i)
        REQUIRE_THAT(both[static_cast<size_t>(i)], WithinAbs(reference[static_cast<size_t>(i)], 1.0e-6));
}

TEST_CASE("A level of zero silences its region", "[dsp][punchtail][levels]")
{
    const auto noPunch = kick(withLevels(0.0f, 1.0f), ms(300));
    for (int i = 0; i < ms(77.9); ++i)
        REQUIRE(juce::exactlyEqual(noPunch[static_cast<size_t>(i)], 0.0f));
    REQUIRE(fcm::test::peak(std::vector<float>(noPunch.begin() + ms(100), noPunch.end())) > 0.3f);

    const auto noTail = kick(withLevels(1.0f, 0.0f), ms(300));
    for (int i = ms(82.1); i < ms(300); ++i)
        REQUIRE(juce::exactlyEqual(noTail[static_cast<size_t>(i)], 0.0f));
}

TEST_CASE("Changing a level while sounding does not click", "[dsp][punchtail][levels][click]")
{
    Fixture fixture(withLevels(1.0f, 1.0f));
    fixture.voice.trigger(1.0f);
    fixture.render(ms(200));

    fixture.voice.setParameters(withLevels(1.0f, 0.0f),
                                false); // the tail drops to silence over the smoothing time
    const auto out = fixture.render(ms(100));

    float largestStep = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        largestStep = std::max(largestStep, std::abs(out[i] - out[i - 1]));
    REQUIRE(largestStep < 0.02f);
    REQUIRE(juce::exactlyEqual(out.back(), 0.0f));
}
