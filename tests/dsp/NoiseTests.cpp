// SPDX-License-Identifier: AGPL-3.0-only
#include "AllocationCounter.h"
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/engine/Voice.h"
#include "dsp/noise/FactoryNoises.h"
#include "dsp/noise/NoisePlayer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <set>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::FactoryNoises;
using fcm::dsp::NoisePlayer;
using fcm::dsp::NoiseSample;
using fcm::dsp::NoiseSettings;

namespace
{
constexpr double kRate = 48000.0;

float peakOf(const std::vector<float>& samples)
{
    float peak = 0.0f;
    for (float s : samples)
        peak = std::max(peak, std::abs(s));
    return peak;
}

// Power in dB of a band of an FFT of the first 2^15 samples, to measure the slope of a noise.
double bandPowerDb(const std::vector<float>& samples, double lowHz, double highHz)
{
    constexpr int order = 15;
    constexpr size_t size = size_t{1} << order;
    juce::dsp::FFT fft(order);
    std::vector<float> data(size * 2, 0.0f);
    for (size_t i = 0; i < size && i < samples.size(); ++i)
        data[i] = samples[i];
    fft.performRealOnlyForwardTransform(data.data(), true);

    double power = 0.0;
    size_t bins = 0;
    for (size_t k = 0; k < size / 2; ++k)
    {
        const double hz = static_cast<double>(k) * kRate / static_cast<double>(size);
        if (hz >= lowHz && hz < highHz)
        {
            power += static_cast<double>(data[2 * k]) * data[2 * k] +
                     static_cast<double>(data[2 * k + 1]) * data[2 * k + 1];
            ++bins;
        }
    }
    return 10.0 * std::log10(power / static_cast<double>(std::max<size_t>(bins, 1)));
}

NoiseSample rampSample(int length, double rate = kRate)
{
    NoiseSample sample;
    sample.sampleRate = rate;
    for (int i = 0; i < length; ++i)
        sample.samples.push_back(static_cast<float>(i) / static_cast<float>(length));
    return sample;
}
} // namespace

TEST_CASE("The factory noises are the eight of the spec, in order", "[dsp][noise][factory]")
{
    REQUIRE(FactoryNoises::count() == 8);
    const std::vector<std::string> names{"White", "Pink",    "Brown",     "Crackle",
                                         "Metal", "Digital", "Air Burst", "Thump"};
    for (int i = 0; i < 8; ++i)
        REQUIRE(names[static_cast<size_t>(i)] == FactoryNoises::name(i));
    REQUIRE(std::string(FactoryNoises::name(8)).empty());
    REQUIRE(std::string(FactoryNoises::name(-1)).empty());
}

TEST_CASE("Every factory noise is finite, peak 1, deterministic and the right length",
          "[dsp][noise][factory]")
{
    std::set<std::vector<float>> distinct;
    for (int i = 0; i < FactoryNoises::count(); ++i)
    {
        INFO(FactoryNoises::name(i));
        const auto first = FactoryNoises::build(i);
        const auto second = FactoryNoises::build(i);
        REQUIRE(first->samples == second->samples);
        REQUIRE(first->sampleRate == FactoryNoises::kSampleRate);
        REQUIRE(fcm::test::allFinite(first->samples));
        REQUIRE_THAT(peakOf(first->samples), WithinAbs(1.0, 1.0e-6));

        // The loops last 2 seconds, the two one-shots 150 ms.
        REQUIRE(first->samples.size() == (i < 6 ? 96000u : 7200u));
        distinct.insert(first->samples);
    }
    REQUIRE(distinct.size() == 8u);
    REQUIRE(FactoryNoises::build(99)->samples == FactoryNoises::build(0)->samples);
}

TEST_CASE("Pink falls 3 dB per octave and Brown 6 dB", "[dsp][noise][factory]")
{
    const auto slope = [](int index)
    {
        const auto noise = FactoryNoises::build(index);
        // The level in each octave band from 100 Hz to 10 kHz, as a power per bin: fall per octave.
        const double low = bandPowerDb(noise->samples, 200.0, 400.0);
        const double high = bandPowerDb(noise->samples, 6400.0, 12800.0);
        return (high - low) / 5.0; // five octaves between the bands
    };
    REQUIRE_THAT(slope(0), WithinAbs(0.0, 1.5));  // White: flat
    REQUIRE_THAT(slope(1), WithinAbs(-3.0, 1.5)); // Pink
    REQUIRE_THAT(slope(2), WithinAbs(-6.0, 1.5)); // Brown
}

TEST_CASE("The loops have no step at the wrap", "[dsp][noise][factory]")
{
    // Playing a loop for longer than its length never steps more than the noise itself does.
    for (int index = 0; index < 6; ++index)
    {
        const auto noise = FactoryNoises::build(index);
        NoisePlayer player;
        player.prepare(kRate);
        player.setSample(noise.get());
        NoiseSettings settings;
        settings.random = 0.0f;
        player.start(settings);

        float biggest = 0.0f;
        float previous = player.process(0.0);
        for (size_t i = 0; i < noise->samples.size() * 2 + 100; ++i)
        {
            const float now = player.process(0.0);
            REQUIRE(std::isfinite(now));
            biggest = std::max(biggest, std::abs(now - previous));
            previous = now;
        }

        // The largest step of the sound itself.
        float own = 0.0f;
        for (size_t i = 1; i < noise->samples.size(); ++i)
            own = std::max(own, std::abs(noise->samples[i] - noise->samples[i - 1]));
        INFO(FactoryNoises::name(index));
        REQUIRE(biggest <= own * 1.05f + 1.0e-6f);
    }
}

TEST_CASE("The player plays at the pitch asked", "[dsp][noise][player]")
{
    const auto ramp = rampSample(48000);
    NoisePlayer player;
    player.prepare(kRate);
    player.setSample(&ramp);

    NoiseSettings settings;
    settings.oneShot = true;
    settings.phase = 0.0f;
    settings.random = 0.0f;

    const auto positionAfter = [&](double semitones, int samples)
    {
        player.start(settings);
        float last = 0.0f;
        for (int i = 0; i < samples; ++i)
            last = player.process(semitones);
        return last;
    };

    // At 0 semitones a ramp reads 1000/48000 after 1000 steps; +12 doubles the rate, -12 halves it.
    REQUIRE_THAT(positionAfter(0.0, 1000), WithinAbs(999.0 / 48000.0, 1.0e-4));
    REQUIRE_THAT(positionAfter(12.0, 1000), WithinAbs(2.0 * 999.0 / 48000.0, 2.0e-4));
    REQUIRE_THAT(positionAfter(-12.0, 1000), WithinAbs(0.5 * 999.0 / 48000.0, 1.0e-4));

    // A sound recorded at another rate is played at its true speed.
    const auto slow = rampSample(24000, 24000.0);
    player.setSample(&slow);
    REQUIRE_THAT(positionAfter(0.0, 1000), WithinAbs(0.5 * 999.0 / 24000.0, 1.0e-3));
}

TEST_CASE("One Shot stops at the end and a loop never stops", "[dsp][noise][player]")
{
    const auto ramp = rampSample(1000);
    NoisePlayer player;
    player.prepare(kRate);
    player.setSample(&ramp);

    NoiseSettings settings;
    settings.random = 0.0f;
    settings.oneShot = true;
    player.start(settings);
    for (int i = 0; i < 2000; ++i)
        player.process(0.0);
    REQUIRE_FALSE(player.isRunning());
    REQUIRE(player.process(0.0) == 0.0f);

    settings.oneShot = false;
    player.start(settings);
    for (int i = 0; i < 100000; ++i)
        REQUIRE(std::isfinite(player.process(0.0)));
    REQUIRE(player.isRunning());
}

TEST_CASE("Air Burst and Thump play once whatever One Shot says, the others follow the switch",
          "[dsp][noise][player]")
{
    for (int index = 0; index < FactoryNoises::count(); ++index)
    {
        const auto sample = FactoryNoises::build(index);
        const bool shortHit = std::string(FactoryNoises::name(index)) == "Air Burst" ||
                              std::string(FactoryNoises::name(index)) == "Thump";
        REQUIRE(sample->playOnce == shortHit);

        NoisePlayer player;
        player.prepare(kRate);
        player.setSample(sample.get());
        NoiseSettings settings;
        settings.random = 0.0f;
        settings.oneShot = false; // the switch is off
        player.start(settings);
        for (int i = 0; i < 200000; ++i) // more than 4 seconds, longer than any built-in sound
            player.process(0.0);
        INFO(FactoryNoises::name(index));
        REQUIRE(player.isRunning() == !shortHit);
    }
}

TEST_CASE("Phase sets the start, Random varies it and reset makes runs identical", "[dsp][noise][player]")
{
    const auto ramp = rampSample(1000);
    NoisePlayer player;
    player.prepare(kRate);
    player.setSample(&ramp);

    NoiseSettings settings;
    settings.oneShot = true;
    settings.random = 0.0f;
    settings.phase = 0.5f;
    player.start(settings);
    REQUIRE_THAT(player.process(0.0), WithinAbs(0.5, 2.0e-3)); // starts half way through

    // Random 0 gives the same start every time, Random 100 % a different one.
    settings.phase = 0.0f;
    std::set<float> fixed, varied;
    for (int i = 0; i < 20; ++i)
    {
        settings.random = 0.0f;
        player.start(settings);
        fixed.insert(player.process(0.0));
        settings.random = 1.0f;
        player.start(settings);
        varied.insert(player.process(0.0));
    }
    REQUIRE(fixed.size() == 1u);
    REQUIRE(varied.size() > 10u);

    // reset() restarts the sequence of starts.
    const auto run = [&]
    {
        std::vector<float> starts;
        player.reset();
        settings.random = 1.0f;
        for (int i = 0; i < 8; ++i)
        {
            player.start(settings);
            starts.push_back(player.process(0.0));
        }
        return starts;
    };
    REQUIRE(run() == run());
}

TEST_CASE("The player stays finite for odd settings and sample rates", "[dsp][noise][player]")
{
    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        const auto noise = FactoryNoises::build(4);
        NoisePlayer player;
        player.prepare(rate);
        player.setSample(noise.get());
        NoiseSettings settings;
        settings.phase = std::numeric_limits<float>::quiet_NaN();
        settings.random = std::numeric_limits<float>::infinity();
        player.start(settings);
        for (const double semitones :
             {-1.0e9, -48.0, 0.0, 48.0, 1.0e9, std::numeric_limits<double>::quiet_NaN()})
            for (int i = 0; i < 5000; ++i)
                REQUIRE(std::isfinite(player.process(semitones)));
    }

    // No sample: silence. A sample that is far too short: silence.
    NoisePlayer empty;
    empty.prepare(kRate);
    empty.start({});
    REQUIRE(empty.process(0.0) == 0.0f);
    NoiseSample tiny;
    tiny.samples = {1.0f};
    empty.setSample(&tiny);
    empty.start({});
    REQUIRE(empty.process(0.0) == 0.0f);
}

namespace
{
struct VoiceRig
{
    VoiceRig()
    {
        sine = fcm::dsp::WavetableData::makeSine();
        noise = FactoryNoises::build(0);
        voice.prepare(kRate, sine.get());
        voice.setNoiseSample(noise.get());
    }

    std::vector<float> render(const fcm::dsp::EngineParameters& parameters, int samples, float note = 36.0f)
    {
        voice.setParameters(parameters, true);
        voice.trigger(1.0f);
        std::vector<float> out;
        std::array<float, fcm::dsp::kNumStems> stems{};
        float weight = 0.0f;
        for (int i = 0; i < samples; ++i)
        {
            voice.processStems(note, stems, weight);
            out.push_back(stems[fcm::dsp::stemNoise]);
            noiseOnly.push_back(stems[fcm::dsp::stemNoise]);
            rest.push_back(stems[fcm::dsp::stemOsc1] + stems[fcm::dsp::stemOsc2] + stems[fcm::dsp::stemTock]);
        }
        return out;
    }

    std::unique_ptr<const fcm::dsp::WavetableData> sine;
    std::unique_ptr<const NoiseSample> noise;
    fcm::dsp::Voice voice;
    std::vector<float> noiseOnly, rest;
};
} // namespace

TEST_CASE("With the noise off the noise stem is silent and the others are unchanged", "[dsp][noise][voice]")
{
    fcm::dsp::EngineParameters off;
    VoiceRig rig;
    for (float s : rig.render(off, 4800))
        REQUIRE(s == 0.0f);

    // The same kick with the noise on has the same oscillator stems.
    fcm::dsp::EngineParameters on;
    on.noise.on = true;
    VoiceRig other;
    other.render(on, 4800);
    REQUIRE(other.rest == rig.rest);
    REQUIRE(fcm::test::peak(other.noiseOnly) > 0.1f);
}

TEST_CASE("The noise follows the amp envelope and ignores the pitch envelope", "[dsp][noise][voice]")
{
    fcm::dsp::EngineParameters on;
    on.noise.on = true;

    // A kick the amp envelope fades out after its release: the noise ends with it.
    VoiceRig rig;
    rig.voice.setParameters(on, true);
    rig.voice.trigger(1.0f);
    std::array<float, fcm::dsp::kNumStems> stems{};
    float weight = 0.0f;
    for (int i = 0; i < 48000; ++i)
        rig.voice.processStems(36.0f, stems, weight);
    REQUIRE(std::abs(stems[fcm::dsp::stemNoise]) > 0.0f); // held while the key is down
    rig.voice.release();
    for (int i = 0; i < 48000; ++i)
        rig.voice.processStems(36.0f, stems, weight);
    REQUIRE(stems[fcm::dsp::stemNoise] == 0.0f);

    // The pitch envelope amount changes the oscillators but not the noise.
    fcm::dsp::EngineParameters deep = on;
    deep.pitchAmount = 0.0f;
    VoiceRig a, b;
    const auto withDrop = a.render(on, 4800);
    const auto withoutDrop = b.render(deep, 4800);
    REQUIRE(withDrop == withoutDrop);
    REQUIRE(a.rest != b.rest);
}

TEST_CASE("The noise follows Level, the Punch and Tail levels and Key Track", "[dsp][noise][voice]")
{
    fcm::dsp::EngineParameters on;
    on.noise.on = true;
    on.oscillators[0].on = false; // only the noise sounds

    VoiceRig base;
    const auto reference = base.render(on, 4800);

    fcm::dsp::EngineParameters quieter = on;
    quieter.noise.level = 0.5f;
    VoiceRig half;
    const auto halved = half.render(quieter, 4800);
    for (size_t i = 0; i < reference.size(); ++i)
        REQUIRE_THAT(halved[i], WithinAbs(reference[i] * 0.5f, 1.0e-6));

    fcm::dsp::EngineParameters punchy = on;
    punchy.punchLevel = 0.0f; // the punch part is silent
    VoiceRig punch;
    const auto cut = punch.render(punchy, 4800);
    REQUIRE(fcm::test::peak({cut.begin(), cut.begin() + 1000}) < 1.0e-6f);
    REQUIRE(fcm::test::peak({cut.begin() + 4000, cut.end()}) > 0.05f);

    // Key Track off: another note sounds the same. On: it plays at another speed.
    VoiceRig low, high;
    REQUIRE(low.render(on, 2000, 36.0f) == high.render(on, 2000, 72.0f));
    fcm::dsp::EngineParameters tracked = on;
    tracked.noise.keyTrack = true;
    VoiceRig lowTracked, highTracked;
    REQUIRE(lowTracked.render(tracked, 2000, 36.0f) != highTracked.render(tracked, 2000, 72.0f));
}

TEST_CASE("The engine plays the noise through its mixer channel and allocates nothing",
          "[dsp][noise][engine][realtime]")
{
    const auto noise = FactoryNoises::build(5);
    fcm::dsp::PublishedPointer<NoiseSample> source;
    source.publish(std::make_unique<NoiseSample>(*noise));

    fcm::dsp::Engine engine(fcm::test::noEffects());
    engine.setNoiseSource(&source);
    fcm::dsp::EngineParameters parameters;
    parameters.noise.on = true;
    parameters.oscillators[0].on = false;
    parameters.mixer[fcm::dsp::stemNoise].destination = fcm::dsp::Destination::busA;
    parameters.noise.oneShot = false;
    engine.setParameters(parameters);
    engine.prepare({kRate, 256, 2});

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer noteOn, none;
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);

    float peak = 0.0f;
    fcm::test::AllocationCounter counter;
    for (int block = 0; block < 20; ++block)
    {
        buffer.clear();
        engine.process(buffer, block == 0 ? noteOn : none);
        for (int i = 0; i < 256; ++i)
            peak = std::max(peak, std::abs(buffer.getSample(0, i)));
    }
    const int allocations = counter.count();
    REQUIRE(allocations == 0);
    REQUIRE(peak > 0.05f);
}
