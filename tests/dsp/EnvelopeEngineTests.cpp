// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/envelope/BreakpointEnvelope.h"
#include "dsp/envelope/EnvelopePresets.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_core/juce_core.h>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::AmpKnob;
using fcm::dsp::BreakpointEnvelope;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::EnvelopeKind;
using fcm::dsp::EnvelopeShape;
using fcm::dsp::PitchKnob;
using fcm::dsp::PublishedPointer;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

struct Rig
{
    explicit Rig(const EngineParameters& parameters = {}, bool usePublishers = true)
    {
        if (usePublishers)
        {
            publish(BreakpointEnvelope(EnvelopeKind::amp), BreakpointEnvelope(EnvelopeKind::pitch));
            engine.setEnvelopeSources(&ampSource, &pitchSource);
        }
        engine.setParameters(parameters);
        engine.prepare({kSampleRate, kBlock, 2});
    }

    void publish(const BreakpointEnvelope& amp, const BreakpointEnvelope& pitch)
    {
        ampSource.publish(amp.toShape());
        pitchSource.publish(pitch.toShape());
    }

    void publishAmp(const BreakpointEnvelope& amp) { ampSource.publish(amp.toShape()); }

    // Renders whole blocks; `events` are (block index, message) pairs delivered at sample 0 of that block.
    std::vector<float> render(int blocks, const std::vector<std::pair<int, juce::MidiMessage>>& events = {})
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer midi;
            for (const auto& [at, message] : events)
                if (at == block)
                    midi.addEvent(message, 0);

            buffer.clear();
            engine.process(buffer, midi);
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        }
        return out;
    }

    // Rounded up, so a window that ends at `seconds` never reads past the rendered audio.
    static int blocksFor(double seconds)
    {
        return static_cast<int>(std::ceil(seconds * kSampleRate / kBlock));
    }

    PublishedPointer<EnvelopeShape> ampSource;
    PublishedPointer<EnvelopeShape> pitchSource;
    Engine engine;
};

std::vector<float> slice(const std::vector<float>& v, double fromSeconds, double toSeconds)
{
    // A window outside the rendered audio is a mistake in the test, not something to read.
    REQUIRE(static_cast<size_t>(std::ceil(toSeconds * kSampleRate)) <= v.size());
    return {v.begin() + static_cast<std::ptrdiff_t>(fromSeconds * kSampleRate),
            v.begin() + static_cast<std::ptrdiff_t>(toSeconds * kSampleRate)};
}

juce::MidiMessage on(int note = 36)
{
    return juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100));
}
juce::MidiMessage off(int note = 36)
{
    return juce::MidiMessage::noteOff(1, note);
}
} // namespace

TEST_CASE("The engine plays the published amp shape", "[dsp][envelope][engine]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);
    amp.setKnob(AmpKnob::sustain, 0.35f);

    Rig rig;
    rig.publishAmp(amp);
    const auto out = rig.render(Rig::blocksFor(0.5), {{0, on()}});

    // 65 Hz: 0.25 to 0.45 s holds many cycles, so the peak is the held level.
    REQUIRE_THAT(fcm::test::peak(slice(out, 0.25, 0.45)), WithinAbs(0.35, 0.01));
}

TEST_CASE("Without publishers the engine plays the default shapes", "[dsp][envelope][engine]")
{
    Rig bare({}, false);
    Rig published;

    const std::vector<std::pair<int, juce::MidiMessage>> notes{{0, on()}};
    REQUIRE(bare.render(Rig::blocksFor(0.3), notes) == published.render(Rig::blocksFor(0.3), notes));
}

TEST_CASE("A shape published while a kick sounds is picked up at the next block", "[dsp][envelope][engine]")
{
    Rig rig;
    const auto before = rig.render(Rig::blocksFor(0.3), {{0, on()}});
    REQUIRE_THAT(fcm::test::peak(slice(before, 0.2, 0.3)), WithinAbs(0.7, 0.01));

    BreakpointEnvelope quieter(EnvelopeKind::amp);
    quieter.setKnob(AmpKnob::sustain, 0.4f);
    rig.publishAmp(quieter);

    const auto after = rig.render(Rig::blocksFor(0.3));
    REQUIRE_THAT(fcm::test::peak(slice(after, 0.1, 0.3)), WithinAbs(0.4, 0.01));
}

TEST_CASE("Editing the shape while sounding does not click", "[dsp][envelope][engine][click]")
{
    EngineParameters parameters;
    parameters.outputGain = 0.5f;
    Rig rig(parameters);
    auto out = rig.render(Rig::blocksFor(0.2), {{0, on()}});

    // Drag the sustain level in small steps, as the editor does.
    BreakpointEnvelope amp(EnvelopeKind::amp);
    for (int step = 0; step < 20; ++step)
    {
        amp.setKnob(AmpKnob::sustain, 0.7f - 0.01f * static_cast<float>(step));
        rig.publishAmp(amp);
        const auto block = rig.render(1);
        out.insert(out.end(), block.begin(), block.end());
    }

    float largestStep = 0.0f;
    for (size_t i = static_cast<size_t>(0.2 * kSampleRate); i < out.size(); ++i)
        largestStep = std::max(largestStep, std::abs(out[i] - out[i - 1]));
    // 65 Hz at about 0.35 output: slope around 0.003 per sample, plus the 1 % level change per step.
    REQUIRE(largestStep < 0.02f);
}

TEST_CASE("Short Punch is a one-shot that ends by itself", "[dsp][envelope][engine]")
{
    Rig rig;
    rig.publishAmp(fcm::dsp::EnvelopePresets::all(EnvelopeKind::amp)[1].envelope);
    const auto out = rig.render(Rig::blocksFor(0.6), {{0, on()}}); // the key is never released

    REQUIRE(fcm::test::peak(slice(out, 0.01, 0.1)) > 0.3f);
    for (double t = 0.13; t < 0.59; t += 0.01)
        REQUIRE(juce::exactlyEqual(fcm::test::peak(slice(out, t, t + 0.005)), 0.0f));
}

TEST_CASE("The pitch amount sets how far the sweep starts above the note", "[dsp][envelope][engine]")
{
    auto frequencyAt = [](float amount, double fromSeconds, double toSeconds)
    {
        EngineParameters parameters;
        parameters.pitchAmount = amount;
        Rig rig(parameters);
        const auto out = rig.render(Rig::blocksFor(0.5), {{0, on()}});
        return fcm::test::measureFrequency(out.data() + static_cast<size_t>(fromSeconds * kSampleRate),
                                           static_cast<int>((toSeconds - fromSeconds) * kSampleRate),
                                           kSampleRate);
    };

    const double note = fcm::test::midiToHz(36);

    // Amount 0: no sweep at all, the kick is on the note from the first cycle.
    REQUIRE(std::abs(fcm::test::centsBetween(frequencyAt(0.0f, 0.0, 0.3), note)) < 1.0);

    // A bigger amount starts higher; every amount still lands exactly on the note.
    const double early48 = frequencyAt(48.0f, 0.0, 0.02);
    const double early24 = frequencyAt(24.0f, 0.0, 0.02);
    REQUIRE(early48 > early24 * 1.5);
    REQUIRE(early24 > note * 2.0);
    for (float amount : {12.0f, 48.0f, 96.0f})
        REQUIRE(std::abs(fcm::test::centsBetween(frequencyAt(amount, 0.15, 0.45), note)) < 1.0);
}

TEST_CASE("Pitch envelope shape controls the tail pitch through End", "[dsp][envelope][engine]")
{
    BreakpointEnvelope pitch(EnvelopeKind::pitch);
    pitch.setKnob(PitchKnob::end, 0.25f); // lands a quarter of the amount (12 semitones) above the note

    Rig rig;
    rig.publish(BreakpointEnvelope(EnvelopeKind::amp), pitch);
    const auto out = rig.render(Rig::blocksFor(0.5), {{0, on()}});

    const double measured = fcm::test::measureFrequency(out.data() + static_cast<size_t>(0.15 * kSampleRate),
                                                        static_cast<int>(0.3 * kSampleRate), kSampleRate);
    REQUIRE(std::abs(fcm::test::centsBetween(measured, fcm::test::midiToHz(36 + 12))) < 1.0);
}

TEST_CASE("Time scale is read at each note-on and stretches the release", "[dsp][envelope][engine]")
{
    EngineParameters parameters;
    parameters.ampTimeScale = 2.0f;

    Rig rig(parameters);
    const auto out = rig.render(Rig::blocksFor(1.0), {{0, on()}, {Rig::blocksFor(0.5), off()}});

    // The release takes 2 x 30 ms: audible 40 ms after the key goes up, gone 70 ms after.
    const double released = Rig::blocksFor(0.5) * kBlock / kSampleRate;
    REQUIRE(fcm::test::peak(slice(out, released + 0.035, released + 0.045)) > 0.0f);
    REQUIRE(juce::exactlyEqual(fcm::test::peak(slice(out, released + 0.065, released + 0.5)), 0.0f));

    // Changing the scale while a kick sounds does not touch that kick: it applies from the next note.
    Rig second;
    second.render(Rig::blocksFor(0.2), {{0, on()}});
    EngineParameters changed;
    changed.ampTimeScale = 3.0f;
    second.engine.setParameters(changed);
    const auto tail = second.render(Rig::blocksFor(0.2));
    REQUIRE_THAT(fcm::test::peak(slice(tail, 0.1, 0.2)), WithinAbs(0.7, 0.01)); // still the 70 % body
}

TEST_CASE("A synced shape follows the tempo, an unsynced one does not", "[dsp][envelope][engine][sync]")
{
    // The release is the easiest time to measure: 40 ms written at 120 BPM.
    auto releaseSamples = [](bool synced, float bpm)
    {
        BreakpointEnvelope amp(EnvelopeKind::amp);
        amp.setReleaseMs(40.0f);
        if (synced)
            amp.enableSync(120.0f);

        EngineParameters parameters;
        parameters.bpm = bpm;
        Rig rig(parameters);
        rig.publishAmp(amp);

        const int noteOff = Rig::blocksFor(0.3);
        const auto out = rig.render(Rig::blocksFor(0.6), {{0, on()}, {noteOff, off()}});

        // Count the samples after the key goes up until the signal is exactly silent.
        const size_t start = static_cast<size_t>(noteOff) * kBlock;
        size_t last = start;
        for (size_t i = start; i < out.size(); ++i)
            if (!juce::exactlyEqual(out[i], 0.0f))
                last = i;
        return static_cast<int>(last - start);
    };

    const int at120 = releaseSamples(true, 120.0f);
    REQUIRE(std::abs(at120 - static_cast<int>(0.040 * kSampleRate)) <= 3);

    // Twice the tempo, half the time; half the tempo, twice the time.
    REQUIRE(std::abs(releaseSamples(true, 240.0f) - at120 / 2) <= 3);
    REQUIRE(std::abs(releaseSamples(true, 60.0f) - at120 * 2) <= 4);

    // Not synced: the tempo makes no difference.
    REQUIRE(std::abs(releaseSamples(false, 240.0f) - at120) <= 3);
    REQUIRE(std::abs(releaseSamples(false, 60.0f) - at120) <= 3);
}

TEST_CASE("The sync scale multiplies the time scale and applies to the pitch envelope too",
          "[dsp][envelope][engine][sync]")
{
    // A pitch drop of 90 ms, synced at 120 BPM, heard at 240 BPM with a time scale of 2: 90 ms again.
    BreakpointEnvelope pitch(EnvelopeKind::pitch);
    pitch.enableSync(120.0f);

    EngineParameters parameters;
    parameters.bpm = 240.0f;
    parameters.pitchTimeScale = 2.0f;
    Rig rig(parameters);
    rig.publish(BreakpointEnvelope(EnvelopeKind::amp), pitch);
    const auto out = rig.render(Rig::blocksFor(0.5), {{0, on()}});

    auto frequency = [&](double from, double to)
    {
        return fcm::test::measureFrequency(out.data() + static_cast<size_t>(from * kSampleRate),
                                           static_cast<int>((to - from) * kSampleRate), kSampleRate);
    };

    // The drop lasts 90 ms (120 / 240 * 2 = 1): at 40 to 120 ms it is still settling, a little above the
    // note, and from 150 ms the tail is exactly on it. A different scale would change either.
    REQUIRE(fcm::test::centsBetween(frequency(0.04, 0.12), fcm::test::midiToHz(36)) > 8.0);
    REQUIRE(std::abs(fcm::test::centsBetween(frequency(0.15, 0.45), fcm::test::midiToHz(36))) < 1.0);
}

TEST_CASE("The tempo is read at each note-on, not in the middle of a kick", "[dsp][envelope][engine][sync]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);
    amp.enableSync(120.0f);

    EngineParameters parameters;
    parameters.bpm = 120.0f;
    Rig rig(parameters);
    rig.publishAmp(amp);
    rig.render(Rig::blocksFor(0.05), {{0, on()}});

    // A tempo change while the kick is in its attack leaves this kick alone.
    EngineParameters faster;
    faster.bpm = 300.0f;
    rig.engine.setParameters(faster);
    const auto during = rig.render(Rig::blocksFor(0.3));
    REQUIRE_THAT(fcm::test::peak(slice(during, 0.2, 0.3)),
                 WithinAbs(0.7, 0.01)); // still holding the body of the 70 % shape
}
