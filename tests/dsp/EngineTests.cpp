// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <random>
#include <vector>

using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;

namespace
{
struct MidiEvent
{
    int sample;
    juce::MidiMessage message;
};

MidiEvent noteOn(int sample, int note, int velocity = 100)
{
    return {sample, juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(velocity))};
}

MidiEvent noteOff(int sample, int note)
{
    return {sample, juce::MidiMessage::noteOff(1, note)};
}

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

// Parameters are set before prepare() so they apply without ramping.
struct EngineFixture
{
    explicit EngineFixture(const EngineParameters& parameters = {}, double newSampleRate = 48000.0,
                           int newBlockSize = 512)
        : sampleRate(newSampleRate), blockSize(newBlockSize)
    {
        engine.setParameters(parameters);
        engine.prepare({sampleRate, static_cast<juce::uint32>(blockSize), 2});
    }

    // Renders totalSamples in blocks, delivering each MIDI event at its absolute sample position.
    Rendered render(int totalSamples, const std::vector<MidiEvent>& events = {})
    {
        Rendered result;
        juce::AudioBuffer<float> buffer(2, blockSize);

        for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
        {
            const int length = std::min(blockSize, totalSamples - blockStart);
            buffer.setSize(2, length, false, false, true);
            buffer.clear();
            buffer.setSample(0, 0, 0.7f); // garbage from the host must be overwritten

            juce::MidiBuffer midi;
            for (const auto& event : events)
                if (event.sample >= blockStart && event.sample < blockStart + length)
                    midi.addEvent(event.message, event.sample - blockStart);

            engine.process(buffer, midi);

            result.left.insert(result.left.end(), buffer.getReadPointer(0),
                               buffer.getReadPointer(0) + length);
            result.right.insert(result.right.end(), buffer.getReadPointer(1),
                                buffer.getReadPointer(1) + length);
        }
        return result;
    }

    int samples(double seconds) const { return static_cast<int>(seconds * sampleRate); }

    double measure(const std::vector<float>& signal, double fromSeconds, double toSeconds) const
    {
        return fcm::test::measureFrequency(signal.data() + samples(fromSeconds),
                                           samples(toSeconds) - samples(fromSeconds), sampleRate);
    }

    double sampleRate;
    int blockSize;
    Engine engine;
};

EngineParameters quietParameters()
{
    EngineParameters parameters;
    parameters.outputGain = 0.5f; // keeps the signal below the limiter's knee
    return parameters;
}
} // namespace

TEST_CASE("Engine is silent until a note is played", "[dsp][engine]")
{
    EngineFixture fixture;
    const auto out = fixture.render(4800);
    REQUIRE(juce::exactlyEqual(fcm::test::peak(out.left), 0.0f));
    REQUIRE(juce::exactlyEqual(fcm::test::peak(out.right), 0.0f));
}

TEST_CASE("Engine keeps the prepared sample rate", "[dsp][engine]")
{
    Engine engine;
    engine.prepare({96000.0, 256, 2});
    REQUIRE(juce::exactlyEqual(engine.getSampleRate(), 96000.0));
}

TEST_CASE("Engine plays a kick when a MIDI note arrives", "[dsp][engine]")
{
    EngineFixture fixture;
    const auto out = fixture.render(fixture.samples(0.5), {noteOn(0, 36)});

    REQUIRE(fcm::test::allFinite(out.left));
    REQUIRE(fcm::test::peak(out.left) > 0.5f);
    REQUIRE(out.left == out.right); // mono voice on both channels

    // Tail settles on the MIDI note (36 is about 65.4 Hz).
    REQUIRE(std::abs(fcm::test::centsBetween(fixture.measure(out.left, 0.12, 0.45),
                                             fcm::test::midiToHz(36))) < 1.0);
}

TEST_CASE("Engine MIDI timing is sample accurate, also across blocks", "[dsp][engine]")
{
    for (int noteSample : {100, 511, 512, 1300})
    {
        EngineFixture fixture;
        const auto out = fixture.render(2048, {noteOn(noteSample, 36)});

        for (int i = 0; i < noteSample; ++i)
            REQUIRE(juce::exactlyEqual(out.left[static_cast<size_t>(i)], 0.0f));
        // The first sample of the note is the sine at phase 0 (zero, up to rounding in the table).
        REQUIRE(std::abs(out.left[static_cast<size_t>(noteSample)]) < 1.0e-5f);
        REQUIRE(std::abs(out.left[static_cast<size_t>(noteSample + 1)]) > 1.0e-4f);
    }
}

TEST_CASE("Engine with Key Track off settles on the root note", "[dsp][engine]")
{
    EngineParameters parameters;
    parameters.keyTrack = false;
    parameters.rootNote = 36;

    for (int note : {24, 36, 60, 72})
    {
        EngineFixture fixture(parameters);
        const auto out = fixture.render(fixture.samples(0.45), {noteOn(0, note)});
        INFO("note " << note);
        REQUIRE(std::abs(fcm::test::centsBetween(fixture.measure(out.left, 0.12, 0.43),
                                                 fcm::test::midiToHz(36))) < 1.0);
    }
}

TEST_CASE("Engine glide slides the tail between consecutive notes", "[dsp][engine]")
{
    EngineParameters parameters;
    parameters.glideOn = true;
    parameters.glideMs = 400.0f;

    // Note 36 plays out, then note 48 arrives.
    const std::vector<MidiEvent> notes{noteOn(0, 36), noteOn(14400, 48)};

    EngineFixture gliding(parameters);
    const auto slid = gliding.render(gliding.samples(0.9), notes);
    // First note: nothing to slide from, it lands exactly on its pitch.
    REQUIRE(std::abs(fcm::test::centsBetween(gliding.measure(slid.left, 0.12, 0.28),
                                             fcm::test::midiToHz(36))) < 1.0);
    // 0.1 to 0.2 s after the second note the tail is partway: pitch 39 to 42 on a 36 to 48 slide.
    const double partway = gliding.measure(slid.left, 0.3 + 0.1, 0.3 + 0.2);
    REQUIRE(partway > fcm::test::midiToHz(37.5));
    REQUIRE(partway < fcm::test::midiToHz(43.0));
    // After the slide (400 ms) it sits on the new note.
    REQUIRE(std::abs(fcm::test::centsBetween(gliding.measure(slid.left, 0.3 + 0.42, 0.3 + 0.5),
                                             fcm::test::midiToHz(48))) < 1.0);

    parameters.glideOn = false;
    EngineFixture jumping(parameters);
    const auto jumped = jumping.render(jumping.samples(0.9), notes);
    REQUIRE(std::abs(fcm::test::centsBetween(jumping.measure(jumped.left, 0.3 + 0.12, 0.3 + 0.28),
                                             fcm::test::midiToHz(48))) < 1.0);
}

TEST_CASE("Engine returns to an older held note without retriggering", "[dsp][engine]")
{
    // A held, B pressed, B released while A is still held.
    const std::vector<MidiEvent> played{noteOn(0, 36), noteOn(14400, 48), noteOff(21600, 48)};
    EngineFixture fixture;
    const auto out = fixture.render(fixture.samples(0.85), played);

    REQUIRE(std::abs(fcm::test::centsBetween(fixture.measure(out.left, 0.3 + 0.12, 0.3 + 0.14),
                                             fcm::test::midiToHz(48))) < 3.0);
    // Pitch goes back to A after the release at 0.45 s.
    REQUIRE(std::abs(fcm::test::centsBetween(fixture.measure(out.left, 0.5, 0.75), fcm::test::midiToHz(36))) <
            1.0);

    // No restart: the output keeps decaying through the release instead of jumping back to a full kick.
    const auto envelope = [&](double seconds)
    {
        float peak = 0.0f;
        for (int i = fixture.samples(seconds); i < fixture.samples(seconds + 0.01); ++i)
            peak = std::max(peak, std::abs(out.left[static_cast<size_t>(i)]));
        return peak;
    };
    REQUIRE(envelope(0.46) < envelope(0.44));
}

TEST_CASE("Engine lets the kick play out after note-off", "[dsp][engine]")
{
    EngineFixture held;
    const auto whileHeld = held.render(held.samples(0.5), {noteOn(0, 36)});

    EngineFixture released;
    const auto afterRelease = released.render(released.samples(0.5), {noteOn(0, 36), noteOff(100, 36)});

    REQUIRE(whileHeld.left == afterRelease.left);
}

TEST_CASE("Engine retrigger restarts the kick without a click", "[dsp][engine][click]")
{
    EngineFixture fixture(quietParameters());
    // Many retriggers at awkward positions, including one a few samples after another.
    const auto out = fixture.render(fixture.samples(0.5), {noteOn(0, 36), noteOn(700, 36), noteOn(1923, 36),
                                                           noteOn(1930, 36), noteOn(9000, 40)});
    float largestStep = 0.0f;
    for (size_t i = 1; i < out.left.size(); ++i)
        largestStep = std::max(largestStep, std::abs(out.left[i] - out.left[i - 1]));
    // 0.5 output gain, and the steepest the kick's own waveform gets is about 0.07 per sample.
    REQUIRE(largestStep < 0.2f);
}

TEST_CASE("Engine chords at one instant do not click", "[dsp][engine][click]")
{
    // Find where the first kick is loud, so cutting it would be a big step.
    EngineFixture probe(quietParameters());
    const auto single = probe.render(2000, {noteOn(0, 36)});
    int loudest = 300;
    for (int i = 300; i < 1500; ++i)
        if (std::abs(single.left[static_cast<size_t>(i)]) >
            std::abs(single.left[static_cast<size_t>(loudest)]))
            loudest = i;
    REQUIRE(std::abs(single.left[static_cast<size_t>(loudest)]) > 0.3f);

    EngineFixture fixture(quietParameters());
    std::vector<MidiEvent> chord{noteOn(0, 36)};
    for (int note : {36, 40, 43, 47, 50, 52})
        chord.push_back(noteOn(loudest, note));
    const auto out = fixture.render(fixture.samples(0.3), chord);

    float largestStep = 0.0f;
    for (size_t i = 1; i < out.left.size(); ++i)
        largestStep = std::max(largestStep, std::abs(out.left[i] - out.left[i - 1]));
    REQUIRE(largestStep < 0.15f);
}

TEST_CASE("Engine velocity sensitivity scales the level", "[dsp][engine]")
{
    auto peakFor = [](float sensitivity, int velocity)
    {
        EngineParameters parameters = quietParameters();
        parameters.velocitySensitivity = sensitivity;
        EngineFixture fixture(parameters);
        return fcm::test::peak(fixture.render(fixture.samples(0.2), {noteOn(0, 36, velocity)}).left);
    };

    const float fullVelocity = peakFor(1.0f, 127);
    REQUIRE_THAT(peakFor(1.0f, 64), Catch::Matchers::WithinAbs(fullVelocity * 64.0f / 127.0f, 1.0e-3));
    REQUIRE_THAT(peakFor(0.5f, 64),
                 Catch::Matchers::WithinAbs(fullVelocity * (0.5f + 0.5f * 64.0f / 127.0f), 1.0e-3));
    REQUIRE_THAT(peakFor(0.0f, 1), Catch::Matchers::WithinAbs(fullVelocity, 1.0e-4));
}

TEST_CASE("Engine output gain scales the level and its changes are smoothed", "[dsp][engine]")
{
    EngineParameters parameters;
    parameters.outputGain = 0.25f;
    EngineFixture quiet(parameters);
    const float quietPeak = fcm::test::peak(quiet.render(quiet.samples(0.2), {noteOn(0, 36)}).left);

    parameters.outputGain = 0.5f;
    EngineFixture louder(parameters);
    REQUIRE_THAT(fcm::test::peak(louder.render(louder.samples(0.2), {noteOn(0, 36)}).left),
                 Catch::Matchers::WithinAbs(2.0f * quietPeak, 1.0e-4));

    // A jump in the gain setting while sounding ramps instead of stepping.
    EngineFixture ramping(quietParameters());
    ramping.render(2400, {noteOn(0, 36)});
    auto jumped = quietParameters();
    jumped.outputGain = 0.0f;
    ramping.engine.setParameters(jumped);
    const auto after = ramping.render(2400);
    float largestStep = 0.0f;
    for (size_t i = 1; i < after.left.size(); ++i)
        largestStep = std::max(largestStep, std::abs(after.left[i] - after.left[i - 1]));
    REQUIRE(largestStep < 0.1f);
    REQUIRE(juce::exactlyEqual(after.left.back(), 0.0f));
}

TEST_CASE("Engine safety limiter keeps the output at or below 0 dBFS", "[dsp][engine][limiter]")
{
    EngineParameters loud;
    loud.outputGain = 4.0f; // +12 dB
    loud.oscillators[1].on = true;

    const std::vector<MidiEvent> notes{noteOn(0, 36), noteOn(4000, 40), noteOn(8000, 36)};

    EngineFixture limited(loud);
    const auto out = limited.render(limited.samples(0.5), notes);
    REQUIRE(fcm::test::allFinite(out.left));
    REQUIRE(fcm::test::peak(out.left) <= 1.0f);
    REQUIRE(fcm::test::peak(out.left) > 0.95f); // it really was driven into the limiter

    // The control: with the limiter off the same signal goes well above full scale.
    loud.limiterOn = false;
    EngineFixture unlimited(loud);
    REQUIRE(fcm::test::peak(unlimited.render(unlimited.samples(0.5), notes).left) > 2.0f);
}

TEST_CASE("Engine oscillator controls tune the oscillators", "[dsp][engine]")
{
    EngineParameters parameters;
    parameters.oscillators[0].on = false;
    parameters.oscillators[1].on = true;
    parameters.oscillators[1].octave = 1;
    parameters.oscillators[1].fineCents = -30.0f;

    EngineFixture fixture(parameters);
    const auto out = fixture.render(fixture.samples(0.45), {noteOn(0, 36)});
    REQUIRE(std::abs(fcm::test::centsBetween(fixture.measure(out.left, 0.12, 0.43),
                                             fcm::test::midiToHz(36 + 12 - 0.3))) < 1.0);
}

TEST_CASE("Engine parameter changes while sounding stay finite and click-free", "[dsp][engine][click]")
{
    EngineFixture fixture(quietParameters());
    fixture.render(2400, {noteOn(0, 36)});

    auto toggled = quietParameters();
    toggled.oscillators[1].on = true; // switching an oscillator on mid-note
    fixture.engine.setParameters(toggled);
    const auto out = fixture.render(4800);

    REQUIRE(fcm::test::allFinite(out.left));
    float largestStep = 0.0f;
    for (size_t i = 1; i < out.left.size(); ++i)
        largestStep = std::max(largestStep, std::abs(out.left[i] - out.left[i - 1]));
    REQUIRE(largestStep < 0.1f);
}

TEST_CASE("Engine kick is silent after the reported tail length", "[dsp][engine]")
{
    EngineFixture fixture;
    const auto out = fixture.render(fixture.samples(Engine::kTailSeconds + 0.2), {noteOn(0, 36)});
    for (int i = fixture.samples(Engine::kTailSeconds); i < static_cast<int>(out.left.size()); ++i)
        REQUIRE(juce::exactlyEqual(out.left[static_cast<size_t>(i)], 0.0f));
}

TEST_CASE("Engine reset returns to a deterministic state", "[dsp][engine]")
{
    EngineParameters parameters;
    parameters.glideOn = true;
    const std::vector<MidiEvent> notes{noteOn(0, 36), noteOn(5000, 43, 70), noteOff(7000, 43)};

    EngineFixture fixture(parameters);
    const auto first = fixture.render(fixture.samples(0.4), notes);
    fixture.engine.reset();
    const auto second = fixture.render(fixture.samples(0.4), notes);
    REQUIRE(first.left == second.left);

    EngineFixture other(parameters);
    REQUIRE(other.render(other.samples(0.4), notes).left == first.left);
}

TEST_CASE("Engine handles all-sound-off and all-notes-off", "[dsp][engine]")
{
    EngineFixture fixture;
    const auto out = fixture.render(4800, {noteOn(0, 36), {2400, juce::MidiMessage::allSoundOff(1)}});
    REQUIRE(juce::exactlyEqual(fcm::test::peak(std::vector<float>(out.left.begin() + 2401, out.left.end())),
                               0.0f));

    // All notes off clears held notes but the kick that is sounding plays out.
    EngineFixture second;
    const auto notCut = second.render(4800, {noteOn(0, 36), {2400, juce::MidiMessage::allNotesOff(1)}});
    REQUIRE(fcm::test::peak(std::vector<float>(notCut.left.begin() + 2401, notCut.left.end())) > 0.1f);
}

TEST_CASE("Engine survives odd buffers and unusual MIDI", "[dsp][engine]")
{
    EngineFixture fixture;

    juce::AudioBuffer<float> empty(2, 0);
    fixture.engine.process(empty, juce::MidiBuffer{});

    juce::AudioBuffer<float> mono(1, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::createSysExMessage(std::vector<juce::uint8>(200, 0x11).data(), 200), 5);
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 9000), 6);
    midi.addEvent(juce::MidiMessage::programChange(1, 4), 7);
    midi.addEvent(juce::MidiMessage::midiClock(), 8);
    fixture.engine.process(mono, midi);
    REQUIRE(fcm::test::peak(std::vector<float>(mono.getReadPointer(0), mono.getReadPointer(0) + 256)) > 0.0f);

    juce::AudioBuffer<float> surround(6, 128);
    fixture.engine.process(surround, juce::MidiBuffer{});
    for (int channel = 1; channel < 6; ++channel)
        for (int i = 0; i < 128; ++i)
            REQUIRE(juce::exactlyEqual(surround.getSample(channel, i), surround.getSample(0, i)));
}

TEST_CASE("Engine random MIDI stress stays finite and within 0 dBFS", "[dsp][engine]")
{
    std::mt19937 random(7);
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        for (int blockSize : {1, 64, 480, 1024})
        {
            EngineParameters parameters;
            parameters.glideOn = true;
            parameters.glideMs = 30.0f;
            parameters.oscillators[1].on = true;
            parameters.outputGain = 4.0f;

            EngineFixture fixture(parameters, sampleRate, blockSize);
            std::vector<MidiEvent> events;
            const int total = fixture.samples(1.0);
            for (int i = 0; i < 80; ++i)
            {
                const int at = static_cast<int>(random() % static_cast<unsigned>(total));
                const int note = 12 + static_cast<int>(random() % 80);
                events.push_back(random() % 3 == 0 ? noteOff(at, note)
                                                   : noteOn(at, note, 1 + static_cast<int>(random() % 127)));
            }

            const auto out = fixture.render(total, events);
            INFO("sampleRate " << sampleRate << " blockSize " << blockSize);
            REQUIRE(fcm::test::allFinite(out.left));
            REQUIRE(fcm::test::peak(out.left) <= 1.0f);
        }
    }
}
