// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Voice.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

using fcm::dsp::EngineParameters;
using fcm::dsp::Voice;
using fcm::dsp::WavetableData;

namespace
{
struct VoiceFixture
{
    explicit VoiceFixture(double newSampleRate, const EngineParameters& parameters = {})
        : sampleRate(newSampleRate)
    {
        sine = WavetableData::makeSine();
        voice.prepare(sampleRate, sine.get());
        voice.setParameters(parameters, true);
    }

    std::vector<float> render(int numSamples, float settleNote)
    {
        std::vector<float> out(static_cast<size_t>(numSamples));
        for (auto& s : out)
            s = voice.process(settleNote);
        return out;
    }

    double sampleRate;
    std::unique_ptr<const WavetableData> sine;
    Voice voice;
};
} // namespace

TEST_CASE("Voice is silent until triggered and plays after", "[dsp][voice]")
{
    VoiceFixture fixture(48000.0);
    REQUIRE_FALSE(fixture.voice.isActive());
    REQUIRE(juce::exactlyEqual(fcm::test::peak(fixture.render(1000, 36.0f)), 0.0f));

    fixture.voice.trigger(1.0f);
    REQUIRE(fixture.voice.isActive());
    const auto out = fixture.render(4800, 36.0f);
    REQUIRE(fcm::test::allFinite(out));
    REQUIRE(fcm::test::peak(out) > 0.5f);

    // The default Kick + Bass holds its body while the key is down; the release ends it and the voice goes
    // idle.
    fixture.render(48000, 36.0f);
    REQUIRE(fixture.voice.isActive());
    fixture.voice.release();
    fixture.render(48000, 36.0f);
    REQUIRE_FALSE(fixture.voice.isActive());
    REQUIRE(juce::exactlyEqual(fcm::test::peak(fixture.render(1000, 36.0f)), 0.0f));
}

TEST_CASE("Voice tail settles within 1 cent of the note's pitch", "[dsp][voice]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        for (float note : {33.0f, 36.0f, 48.0f, 60.0f})
        {
            VoiceFixture fixture(sampleRate);
            fixture.voice.trigger(1.0f);
            const auto out = fixture.render(static_cast<int>(0.4 * sampleRate), note);

            // The pitch drop ends 90 ms after the note; measure well after it.
            const int from = static_cast<int>(0.12 * sampleRate);
            const double measured = fcm::test::measureFrequency(
                out.data() + from, static_cast<int>(out.size()) - from, sampleRate);
            const double expected = fcm::test::midiToHz(note);

            INFO("sampleRate " << sampleRate << " note " << note << " measured " << measured << " expected "
                               << expected);
            REQUIRE(std::abs(fcm::test::centsBetween(measured, expected)) < 1.0);
        }
    }
}

TEST_CASE("Voice starts with the pitch drop", "[dsp][voice]")
{
    VoiceFixture fixture(48000.0);
    fixture.voice.trigger(1.0f);
    const auto out = fixture.render(4800, 36.0f);

    // First 5 ms: +48 semitones above 65.4 Hz is about 1 kHz. Later the pitch has fallen to 65 Hz.
    const double early = fcm::test::measureFrequency(out.data(), 240, 48000.0);
    const double late = fcm::test::measureFrequency(out.data() + 2400, 2400, 48000.0);
    REQUIRE(early > 500.0);
    REQUIRE(late < 200.0);
}

TEST_CASE("Voice oscillator tuning follows pitch, octave and fine", "[dsp][voice]")
{
    EngineParameters parameters;
    parameters.oscillators[0].pitchSemitones = 7;
    parameters.oscillators[0].octave = -1;
    parameters.oscillators[0].fineCents = 25.0f;

    VoiceFixture fixture(48000.0, parameters);
    fixture.voice.trigger(1.0f);
    const auto out = fixture.render(19200, 48.0f);

    // 7 semitones - 12 + 0.25 semitone = -4.75 semitones below the note.
    const double expected = fcm::test::midiToHz(48.0 - 4.75);
    const double measured = fcm::test::measureFrequency(out.data() + 6000, 13000, 48000.0);
    REQUIRE(std::abs(fcm::test::centsBetween(measured, expected)) < 1.0);
}

TEST_CASE("Voice mixes both oscillators and honours on/off and level", "[dsp][voice]")
{
    EngineParameters parameters;
    parameters.oscillators[1].on = true;
    parameters.oscillators[1].level = 0.5f;

    VoiceFixture both(48000.0, parameters);
    both.voice.trigger(1.0f);
    const auto mixed = both.render(2000, 36.0f);

    VoiceFixture single(48000.0);
    single.voice.trigger(1.0f);
    const auto alone = single.render(2000, 36.0f);

    // Both oscillators are in phase at the start, so the mix is 1.5 times a single one.
    for (size_t i = 0; i < alone.size(); ++i)
        REQUIRE_THAT(mixed[i], Catch::Matchers::WithinAbs(1.5f * alone[i], 1.0e-4));

    parameters.oscillators[0].on = false;
    parameters.oscillators[1].on = false;
    VoiceFixture none(48000.0, parameters);
    none.voice.trigger(1.0f);
    REQUIRE(juce::exactlyEqual(fcm::test::peak(none.render(2000, 36.0f)), 0.0f));
}

TEST_CASE("Voice velocity gain scales the level", "[dsp][voice]")
{
    VoiceFixture full(48000.0);
    full.voice.trigger(1.0f);
    const auto loud = full.render(3000, 36.0f);

    VoiceFixture half(48000.0);
    half.voice.trigger(0.5f);
    const auto quiet = half.render(3000, 36.0f);

    REQUIRE_THAT(fcm::test::peak(quiet), Catch::Matchers::WithinAbs(0.5f * fcm::test::peak(loud), 1.0e-4));
}

TEST_CASE("Voice retrigger does not click", "[dsp][voice][click]")
{
    constexpr double sampleRate = 48000.0;

    // Where the first kick is loud, so a hard restart would make a big step.
    VoiceFixture first(sampleRate);
    first.voice.trigger(1.0f);
    const auto original = first.render(4000, 36.0f);
    int retriggerAt = 300;
    for (int i = 300; i < 1500; ++i)
        if (std::abs(original[static_cast<size_t>(i)]) > std::abs(original[static_cast<size_t>(retriggerAt)]))
            retriggerAt = i;
    REQUIRE(std::abs(original[static_cast<size_t>(retriggerAt)]) > 0.6f);

    auto largestStep = [](const std::vector<float>& signal, int from, int to)
    {
        float step = 0.0f;
        for (int i = from; i < to; ++i)
            step =
                std::max(step, std::abs(signal[static_cast<size_t>(i)] - signal[static_cast<size_t>(i - 1)]));
        return step;
    };

    // The control: restarting by cutting from the old kick to a fresh one is audibly a click.
    VoiceFixture fresh(sampleRate);
    fresh.voice.trigger(1.0f);
    const auto restart = fresh.render(1000, 36.0f);
    std::vector<float> hardCut(original.begin(), original.begin() + retriggerAt);
    hardCut.insert(hardCut.end(), restart.begin(), restart.end());
    const float hardStep = largestStep(hardCut, retriggerAt - 1, retriggerAt + 200);
    REQUIRE(hardStep > 0.5f);

    // The voice: same retrigger instant, no step beyond what the signal itself can do. The fastest
    // slope is the start of the pitch drop (about 1 kHz at 48 kHz, 0.13 per sample).
    VoiceFixture retriggered(sampleRate);
    retriggered.voice.trigger(1.0f);
    auto output = retriggered.render(retriggerAt, 36.0f);
    retriggered.voice.trigger(1.0f);
    const auto after = retriggered.render(1000, 36.0f);
    output.insert(output.end(), after.begin(), after.end());

    REQUIRE(fcm::test::allFinite(output));
    REQUIRE(largestStep(output, retriggerAt - 1, retriggerAt + 200) < 0.25f);
}

TEST_CASE("Voice retrigger starts the new kick at the same instant", "[dsp][voice]")
{
    // After the 1.5 ms fade the output equals a fresh kick played from that note-on.
    constexpr double sampleRate = 48000.0;
    VoiceFixture retriggered(sampleRate);
    retriggered.voice.trigger(1.0f);
    retriggered.render(2000, 36.0f);
    retriggered.voice.trigger(1.0f);
    const auto after = retriggered.render(500, 36.0f);

    VoiceFixture fresh(sampleRate);
    fresh.voice.trigger(1.0f);
    const auto expected = fresh.render(500, 36.0f);

    const int fadeSamples = static_cast<int>(Voice::kRetriggerFadeMs * 0.001 * sampleRate) + 2;
    for (size_t i = static_cast<size_t>(fadeSamples); i < after.size(); ++i)
        REQUIRE(juce::exactlyEqual(after[i], expected[i]));
}

TEST_CASE("Voice reset makes two runs identical", "[dsp][voice]")
{
    VoiceFixture fixture(48000.0);

    auto run = [&]
    {
        fixture.voice.trigger(0.8f);
        auto out = fixture.render(2000, 40.0f);
        fixture.voice.trigger(0.8f);
        const auto more = fixture.render(2000, 43.0f);
        out.insert(out.end(), more.begin(), more.end());
        return out;
    };

    const auto first = run();
    fixture.voice.reset();
    REQUIRE_FALSE(fixture.voice.isActive());
    REQUIRE(run() == first);
}

TEST_CASE("Voice start phase is applied at every note", "[dsp][voice]")
{
    EngineParameters parameters;
    parameters.oscillators[0].startPhase = 0.25f; // a sine at its peak

    VoiceFixture fixture(48000.0, parameters);
    fixture.voice.trigger(1.0f);
    const auto out = fixture.render(100, 36.0f);

    // The amp attack takes 1 ms and starts at 0, so the first samples are a small fraction of the peak.
    REQUIRE(std::abs(out[0]) < 0.05f);
    REQUIRE(out[5] > 0.0f);
    REQUIRE(fcm::test::peak(out) > 0.8f);
}

TEST_CASE("Voice output is finite across a range of settings", "[dsp][voice]")
{
    EngineParameters parameters;
    parameters.oscillators[1].on = true;
    parameters.oscillators[1].octave = 4;
    parameters.oscillators[1].pitchSemitones = 24;

    for (double sampleRate : {22050.0, 44100.0, 96000.0, 192000.0})
    {
        VoiceFixture fixture(sampleRate, parameters);
        for (float note : {0.0f, 12.0f, 127.0f})
        {
            fixture.voice.trigger(1.0f);
            REQUIRE(fcm::test::allFinite(fixture.render(static_cast<int>(sampleRate * 0.3), note)));
        }
    }
}
