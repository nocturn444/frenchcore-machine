// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/eq/EqEffect.h"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::EqEffect;

namespace
{
constexpr int kBlock = 256;

using Shape = EqEffect::Shape;

struct Band
{
    bool on = true;
    Shape type = Shape::bell;
    float freq = 1000.0f;
    float gain = 0.0f;
    float q = 1.0f;
    bool extreme = false; // docs/specs/fx-eq.md section 8
    float xgain = 0.0f;
    float xq = 1.0f;
    float keyTrack = 0.0f; // percent
};

// Four bands in the order of the parameters: on, type, frequency, gain, Q, extreme, extreme gain,
// extreme Q, key track.
struct Settings
{
    std::array<Band, 4> bands{
        Band{true, Shape::lowShelf, 80.0f, 0.0f, 0.71f}, Band{true, Shape::bell, 400.0f, 0.0f, 1.0f},
        Band{true, Shape::bell, 2500.0f, 0.0f, 1.0f}, Band{true, Shape::highShelf, 8000.0f, 0.0f, 0.71f}};

    std::vector<float> values() const
    {
        std::vector<float> out;
        for (const auto& band : bands)
            out.insert(out.end(), {band.on ? 1.0f : 0.0f, static_cast<float>(band.type), band.freq, band.gain,
                                   band.q, band.extreme ? 1.0f : 0.0f, band.xgain, band.xq, band.keyTrack});
        return out;
    }
};

std::vector<float> run(EqEffect& effect, const std::vector<float>& input)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    return out;
}

// The gain in dB of the effect at one frequency, measured with a sine.
double gainDbAt(const Settings& settings, double hz, double sampleRate = 48000.0, double noteSemitones = 0.0)
{
    EqEffect effect;
    effect.prepare({sampleRate, kBlock, 2});
    effect.setParameters(settings.values());
    effect.setContext({static_cast<float>(noteSemitones), 120.0f});

    const int total = static_cast<int>(sampleRate) / kBlock * kBlock; // about one second
    std::vector<float> input(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i)
        input[static_cast<size_t>(i)] =
            0.1f * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / sampleRate));

    const auto out = run(effect, input);
    double inEnergy = 0.0, outEnergy = 0.0;
    for (size_t i = static_cast<size_t>(total) / 2; i < out.size(); ++i)
    {
        inEnergy += static_cast<double>(input[i]) * input[i];
        outEnergy += static_cast<double>(out[i]) * out[i];
    }
    return 10.0 * std::log10(outEnergy / inEnergy);
}
} // namespace

TEST_CASE("A flat EQ changes nothing at all", "[dsp][fx][eq]")
{
    EqEffect effect;
    effect.prepare({48000.0, kBlock, 2});
    effect.setParameters(Settings{}.values());

    juce::Random random(5);
    std::vector<float> input(kBlock * 20);
    for (auto& s : input)
        s = random.nextFloat() * 2.0f - 1.0f;
    REQUIRE(run(effect, input) == input);
}

TEST_CASE("The parameters describe four bands of nine values", "[dsp][fx][eq]")
{
    EqEffect effect;
    REQUIRE(effect.parameters().size() == static_cast<size_t>(EqEffect::kBands * EqEffect::kValuesPerBand));
    REQUIRE(Settings{}.values().size() == effect.parameters().size());

    // The defaults of the specs are the defaults of Settings: flat.
    const auto values = Settings{}.values();
    for (size_t i = 0; i < effect.parameters().size(); ++i)
        REQUIRE_THAT(values[i], WithinAbs(effect.parameters()[i].defaultValue, 1.0e-6));
}

TEST_CASE("A low shelf boosts and cuts below its frequency", "[dsp][fx][eq]")
{
    Settings boost;
    boost.bands[0] = {true, Shape::lowShelf, 100.0f, 12.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(boost, 20.0), WithinAbs(12.0, 0.5));
    REQUIRE_THAT(gainDbAt(boost, 2000.0), WithinAbs(0.0, 0.5));
    REQUIRE_THAT(gainDbAt(boost, 100.0), WithinAbs(6.0, 0.7)); // half the gain at the corner

    Settings cut;
    cut.bands[0] = {true, Shape::lowShelf, 100.0f, -12.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(cut, 20.0), WithinAbs(-12.0, 0.5));
    REQUIRE_THAT(gainDbAt(cut, 2000.0), WithinAbs(0.0, 0.5));
}

TEST_CASE("A bell boosts around its frequency, narrower with a higher Q", "[dsp][fx][eq]")
{
    Settings bell;
    bell.bands[1] = {true, Shape::bell, 1000.0f, 12.0f, 1.0f};
    REQUIRE_THAT(gainDbAt(bell, 1000.0), WithinAbs(12.0, 0.5)); // the full gain at the centre
    REQUIRE_THAT(gainDbAt(bell, 100.0), WithinAbs(0.0, 0.7));
    REQUIRE_THAT(gainDbAt(bell, 10000.0), WithinAbs(0.0, 0.7));

    Settings wide = bell;
    wide.bands[1].q = 0.5f;
    Settings narrow = bell;
    narrow.bands[1].q = 8.0f;
    REQUIRE(gainDbAt(wide, 1500.0) > gainDbAt(narrow, 1500.0) + 4.0);
}

TEST_CASE("Any band can be any type, at any frequency", "[dsp][fx][eq]")
{
    // The point of a parametric EQ: the fourth band as a bell at 60 Hz, the first as a high shelf.
    Settings settings;
    settings.bands[3] = {true, Shape::bell, 60.0f, 9.0f, 2.0f};
    settings.bands[0] = {true, Shape::highShelf, 5000.0f, -6.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(settings, 60.0), WithinAbs(9.0, 0.7));
    REQUIRE_THAT(gainDbAt(settings, 18000.0), WithinAbs(-6.0, 0.8));
    REQUIRE_THAT(gainDbAt(settings, 600.0), WithinAbs(0.0, 0.8));
}

TEST_CASE("A high shelf boosts and cuts above its frequency", "[dsp][fx][eq]")
{
    Settings boost;
    boost.bands[3] = {true, Shape::highShelf, 8000.0f, 12.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(boost, 16000.0), WithinAbs(12.0, 0.8));
    REQUIRE_THAT(gainDbAt(boost, 500.0), WithinAbs(0.0, 0.5));
    REQUIRE_THAT(gainDbAt(boost, 8000.0), WithinAbs(6.0, 0.8));
}

TEST_CASE("The cuts remove what is below or above their frequency", "[dsp][fx][eq]")
{
    Settings low;
    low.bands[0] = {true, Shape::lowCut, 100.0f, 0.0f, 0.71f}; // the gain is not used
    REQUIRE_THAT(gainDbAt(low, 100.0), WithinAbs(-3.0, 0.7));
    REQUIRE(gainDbAt(low, 25.0) < -20.0);
    REQUIRE_THAT(gainDbAt(low, 2000.0), WithinAbs(0.0, 0.3));

    Settings high;
    high.bands[3] = {true, Shape::highCut, 5000.0f, 0.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(high, 5000.0), WithinAbs(-3.0, 0.7));
    REQUIRE(gainDbAt(high, 15000.0) < -15.0);
    REQUIRE_THAT(gainDbAt(high, 200.0), WithinAbs(0.0, 0.3));
}

TEST_CASE("A band that is off does nothing, even a cut", "[dsp][fx][eq]")
{
    Settings off;
    off.bands[0] = {false, Shape::lowCut, 500.0f, 0.0f, 0.71f};
    off.bands[1] = {false, Shape::bell, 1000.0f, 12.0f, 1.0f};
    REQUIRE_THAT(gainDbAt(off, 50.0), WithinAbs(0.0, 0.01));
    REQUIRE_THAT(gainDbAt(off, 1000.0), WithinAbs(0.0, 0.01));
}

TEST_CASE("Bands add up in dB when they are far apart", "[dsp][fx][eq]")
{
    Settings all;
    all.bands[0] = {true, Shape::lowShelf, 80.0f, 6.0f, 0.71f};
    all.bands[1] = {true, Shape::bell, 1000.0f, -6.0f, 1.0f};
    all.bands[3] = {true, Shape::highShelf, 10000.0f, 4.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(all, 20.0), WithinAbs(6.0, 0.8));
    REQUIRE_THAT(gainDbAt(all, 1000.0), WithinAbs(-6.0, 0.8));
    REQUIRE_THAT(gainDbAt(all, 20000.0), WithinAbs(4.0, 0.8));
}

TEST_CASE("The response function agrees with the filter", "[dsp][fx][eq]")
{
    for (const auto shape : {Shape::bell, Shape::lowShelf, Shape::highShelf, Shape::lowCut, Shape::highCut})
        for (const double hz : {40.0, 300.0, 1500.0, 9000.0})
        {
            Settings settings;
            settings.bands[1] = {true, shape, 700.0f, 9.0f, 1.3f};
            const auto design = EqEffect::design(shape, 48000.0, 700.0, 1.3, 9.0);
            INFO(static_cast<int>(shape) << " at " << hz);
            REQUIRE_THAT(EqEffect::magnitudeDb(design, 48000.0, hz), WithinAbs(gainDbAt(settings, hz), 0.15));
        }
}

TEST_CASE("The design is stable and bounded at every extreme", "[dsp][fx][eq]")
{
    juce::Random random(11);
    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const float freq :
             {-5.0f, 1.0f, 20.0f, 20000.0f, 90000.0f, std::numeric_limits<float>::quiet_NaN()})
            for (const float q : {0.0f, 0.1f, 10.0f, 500.0f, std::numeric_limits<float>::infinity()})
                for (const float gain : {-60.0f, -18.0f, 18.0f, 60.0f})
                    for (const auto shape :
                         {Shape::bell, Shape::lowShelf, Shape::highShelf, Shape::lowCut, Shape::highCut})
                    {
                        Settings settings;
                        for (auto& band : settings.bands)
                            band = {true, shape, freq, gain, q};

                        EqEffect effect;
                        effect.prepare({rate, kBlock, 2});
                        effect.setParameters(settings.values());

                        // Noise, then a loud kick-like burst.
                        std::vector<float> input(kBlock * 16);
                        for (size_t i = 0; i < input.size(); ++i)
                            input[i] = i < kBlock * 8
                                           ? random.nextFloat() * 2.0f - 1.0f
                                           : static_cast<float>(std::sin(2.0 * std::numbers::pi * 50.0 *
                                                                         static_cast<double>(i) / rate));
                        for (float s : run(effect, input))
                        {
                            REQUIRE(std::isfinite(s));
                            // Four identical bands at +36 dB with a Q of 50 can add up to a resonance far
                            // above the nominal gain (a shelf peaks at about gain times Q per band): a
                            // wide bound; an unstable filter would reach infinity.
                            REQUIRE(std::abs(s) < 1.0e15f);
                        }
                    }
}

TEST_CASE("Changing the parameters every block stays bounded", "[dsp][fx][eq]")
{
    juce::Random random(21);
    EqEffect effect;
    effect.prepare({48000.0, kBlock, 2});

    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < 400; ++block)
    {
        Settings settings;
        for (auto& band : settings.bands)
            band = {random.nextFloat() > 0.2f, static_cast<Shape>(random.nextInt(5)),
                    20.0f + random.nextFloat() * 19980.0f, random.nextFloat() * 36.0f - 18.0f,
                    0.1f + random.nextFloat() * 9.9f};
        effect.setParameters(settings.values());

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, 0.5f * (random.nextFloat() * 2.0f - 1.0f));
        effect.process(buffer);
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(std::abs(buffer.getSample(0, i)) < 1.0e4f); // four +18 dB peaks at one frequency: 7.9^4
    }
}

TEST_CASE("The EQ works on one channel", "[dsp][fx][eq]")
{
    EqEffect effect;
    effect.prepare({48000.0, kBlock, 1});
    Settings settings;
    settings.bands[0] = {true, Shape::lowShelf, 100.0f, 12.0f, 0.71f};
    effect.setParameters(settings.values());

    juce::AudioBuffer<float> mono(1, kBlock);
    for (int i = 0; i < kBlock; ++i)
        mono.setSample(0, i, 0.1f);
    effect.process(mono);
    for (int i = 0; i < kBlock; ++i)
        REQUIRE(std::isfinite(mono.getSample(0, i)));
}

TEST_CASE("Extreme off ignores the extreme controls, on ignores Gain and Q", "[dsp][fx][eq][extreme]")
{
    Settings normal;
    normal.bands[1] = {true, Shape::bell, 1000.0f, 12.0f, 1.0f, false, -30.0f, 40.0f, 0.0f};
    Settings ignored;
    ignored.bands[1] = {true, Shape::bell, 1000.0f, 12.0f, 1.0f, false, 20.0f, 5.0f, 0.0f};
    for (const double hz : {300.0, 1000.0, 3000.0})
        REQUIRE(gainDbAt(normal, hz) == gainDbAt(ignored, hz));

    Settings extremeA;
    extremeA.bands[1] = {true, Shape::bell, 1000.0f, 12.0f, 1.0f, true, 20.0f, 5.0f, 0.0f};
    Settings extremeB;
    extremeB.bands[1] = {true, Shape::bell, 1000.0f, -9.0f, 7.0f, true, 20.0f, 5.0f, 0.0f};
    for (const double hz : {300.0, 1000.0, 3000.0})
        REQUIRE(gainDbAt(extremeA, hz) == gainDbAt(extremeB, hz));
    REQUIRE_THAT(gainDbAt(extremeA, 1000.0), WithinAbs(20.0, 0.7));
}

TEST_CASE("An extreme bell reaches +36 dB at the centre and is narrow", "[dsp][fx][eq][extreme]")
{
    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        Settings settings;
        settings.bands[1] = {true, Shape::bell, 1000.0f, 0.0f, 1.0f, true, 36.0f, 50.0f, 0.0f};
        INFO("rate " << rate);
        REQUIRE_THAT(gainDbAt(settings, 1000.0, rate), WithinAbs(36.0, 1.0));
        REQUIRE(gainDbAt(settings, 1100.0, rate) < 3.0);
        REQUIRE(gainDbAt(settings, 900.0, rate) < 3.0);

        // The negative side is as deep.
        settings.bands[1].xgain = -36.0f;
        REQUIRE_THAT(gainDbAt(settings, 1000.0, rate), WithinAbs(-36.0, 1.0));
    }
}

TEST_CASE("The extreme peak stays finite and bounded for noise and a loud sweep", "[dsp][fx][eq][extreme]")
{
    juce::Random random(77);
    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const float freq : {30.0f, 1000.0f, 12000.0f})
        {
            Settings settings;
            for (auto& band : settings.bands)
                band = {true, Shape::bell, freq, 0.0f, 1.0f, true, 36.0f, 50.0f, 0.0f};

            EqEffect effect;
            effect.prepare({rate, kBlock, 2});
            effect.setParameters(settings.values());

            std::vector<float> input(kBlock * 40);
            for (size_t i = 0; i < input.size(); ++i)
            {
                const double t = static_cast<double>(i) / rate;
                input[i] =
                    i < kBlock * 16
                        ? random.nextFloat() * 2.0f - 1.0f
                        : static_cast<float>(std::sin(2.0 * std::numbers::pi * (30.0 * t + 4000.0 * t * t)));
            }
            for (float sample : run(effect, input))
            {
                REQUIRE(std::isfinite(sample));
                // Four +36 dB peaks at one frequency would be 63^4 = 1.6e7 at the very most.
                REQUIRE(std::abs(sample) < 1.0e9f);
            }
        }
}

TEST_CASE("Key Track moves the band with the note", "[dsp][fx][eq][keytrack]")
{
    // Freq is the frequency at C3 (note 60). A peak at 500 Hz with Key Track 100 % is at 1 kHz for the
    // note an octave above (72), and at 500 * 2^0.5 for half the amount.
    const auto peakHz = [](float keyTrack, double note)
    {
        Settings settings;
        settings.bands[1] = {true, Shape::bell, 500.0f, 12.0f, 8.0f, false, 0.0f, 1.0f, keyTrack};
        double best = 0.0, bestHz = 0.0;
        for (double hz = 200.0; hz < 2400.0; hz *= 1.01)
        {
            const double g = gainDbAt(settings, hz, 48000.0, note);
            if (g > best)
            {
                best = g;
                bestHz = hz;
            }
        }
        return bestHz;
    };

    REQUIRE_THAT(peakHz(100.0f, 0.0), WithinAbs(500.0, 8.0));
    REQUIRE_THAT(peakHz(100.0f, 12.0), WithinAbs(1000.0, 15.0));
    REQUIRE_THAT(peakHz(100.0f, -12.0), WithinAbs(250.0, 6.0));
    REQUIRE_THAT(peakHz(50.0f, 12.0), WithinAbs(500.0 * std::sqrt(2.0), 12.0));
    REQUIRE_THAT(peakHz(0.0f, 12.0), WithinAbs(500.0, 8.0)); // 0 % does not move
}

TEST_CASE("Key Track keeps the frequency in range", "[dsp][fx][eq][keytrack]")
{
    using fcm::dsp::EqEffect;
    REQUIRE(EqEffect::keyTrackedFrequency(1000.0, 0.0, 24.0, 48000.0) == 1000.0);
    REQUIRE_THAT(EqEffect::keyTrackedFrequency(1000.0, 100.0, 12.0, 48000.0), WithinAbs(2000.0, 1.0e-9));
    REQUIRE(EqEffect::keyTrackedFrequency(20.0, 100.0, -48.0, 48000.0) == 20.0);
    REQUIRE(EqEffect::keyTrackedFrequency(10000.0, 100.0, 60.0, 48000.0) == 0.45 * 48000.0);
    // Bad numbers leave the setting alone.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(EqEffect::keyTrackedFrequency(1000.0, nan, 12.0, 48000.0) == 1000.0);
    REQUIRE(EqEffect::keyTrackedFrequency(1000.0, 100.0, nan, 48000.0) == 1000.0);
}
