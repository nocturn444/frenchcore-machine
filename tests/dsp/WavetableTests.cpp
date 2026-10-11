// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/oscillator/WavetableData.h"
#include "dsp/oscillator/WavetableOscillator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_dsp/juce_dsp.h>
#include <numbers>
#include <random>

using fcm::dsp::WavetableData;
using fcm::dsp::WavetableOscillator;

namespace
{
std::vector<float> makeSaw()
{
    std::vector<float> saw(WavetableData::kFrameSize);
    for (size_t i = 0; i < saw.size(); ++i)
        saw[i] = 2.0f * static_cast<float>(i) / WavetableData::kFrameSize - 1.0f;
    return saw;
}

// Magnitude of harmonic `h` of a frame, by direct correlation (exact for a periodic frame).
double harmonicMagnitude(const float* frame, int h)
{
    double re = 0.0, im = 0.0;
    for (int i = 0; i < WavetableData::kFrameSize; ++i)
    {
        const double angle = 2.0 * std::numbers::pi * h * i / WavetableData::kFrameSize;
        re += frame[i] * std::cos(angle);
        im -= frame[i] * std::sin(angle);
    }
    return 2.0 * std::hypot(re, im) / WavetableData::kFrameSize;
}
} // namespace

TEST_CASE("WavetableData rejects invalid sizes", "[dsp][wavetable]")
{
    REQUIRE(WavetableData::fromFrames({}) == nullptr);

    std::vector<float> notAMultiple(WavetableData::kFrameSize + 1, 0.0f);
    REQUIRE(WavetableData::fromFrames(notAMultiple) == nullptr);

    std::vector<float> tooMany(static_cast<size_t>(WavetableData::kMaxFrames + 1) * WavetableData::kFrameSize,
                               0.0f);
    REQUIRE(WavetableData::fromFrames(tooMany) == nullptr);

    std::vector<float> maximum(static_cast<size_t>(WavetableData::kMaxFrames) * WavetableData::kFrameSize,
                               0.0f);
    REQUIRE(WavetableData::fromFrames(maximum) != nullptr);
}

TEST_CASE("WavetableData mip 0 reproduces the source apart from the Nyquist bin", "[dsp][wavetable]")
{
    std::mt19937 random(1234);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    std::vector<float> source(WavetableData::kFrameSize);
    for (auto& s : source)
        s = noise(random);

    const auto table = WavetableData::fromFrames(source);
    REQUIRE(table != nullptr);
    REQUIRE(table->numFrames() == 1);

    // Removing bin 1024 subtracts (-1)^n * c, with c the Nyquist component of the source.
    double nyquist = 0.0;
    for (int i = 0; i < WavetableData::kFrameSize; ++i)
        nyquist += source[static_cast<size_t>(i)] * ((i % 2) ? -1.0 : 1.0);
    nyquist /= WavetableData::kFrameSize;

    const float* mip0 = table->frame(0, 0);
    for (int i = 0; i < WavetableData::kFrameSize; ++i)
    {
        const double expected = source[static_cast<size_t>(i)] - nyquist * ((i % 2) ? -1.0 : 1.0);
        REQUIRE(std::abs(mip0[i] - expected) < 1.0e-4);
    }
}

TEST_CASE("WavetableData mip levels are band-limited", "[dsp][wavetable]")
{
    const auto table = WavetableData::fromFrames(makeSaw());
    REQUIRE(table != nullptr);

    const double fundamental = harmonicMagnitude(table->frame(0, 0), 1);
    REQUIRE(fundamental > 0.5);

    for (int mip = 2; mip < WavetableData::kNumMipLevels; ++mip)
    {
        const int limit = WavetableData::maxHarmonics(mip);
        const float* frame = table->frame(mip, 0);

        // A saw has every harmonic, so the last kept one is present and the first removed one is gone.
        REQUIRE(harmonicMagnitude(frame, limit) > 0.5 * fundamental / limit);
        for (int h : {limit + 1, limit + 2, 2 * limit, 3 * limit})
            if (h < WavetableData::kFrameSize / 2)
                REQUIRE(harmonicMagnitude(frame, h) < 1.0e-4 * fundamental);
    }
}

TEST_CASE("WavetableData guard sample repeats the first sample", "[dsp][wavetable]")
{
    std::vector<float> twoFrames(2 * WavetableData::kFrameSize);
    const auto saw = makeSaw();
    std::copy(saw.begin(), saw.end(), twoFrames.begin());
    std::copy(saw.begin(), saw.end(), twoFrames.begin() + WavetableData::kFrameSize);

    const auto table = WavetableData::fromFrames(twoFrames);
    REQUIRE(table != nullptr);
    REQUIRE(table->numFrames() == 2);

    for (int mip = 0; mip < WavetableData::kNumMipLevels; ++mip)
        for (int frame = 0; frame < 2; ++frame)
            REQUIRE(juce::exactlyEqual(table->frame(mip, frame)[WavetableData::kFrameSize],
                                       table->frame(mip, frame)[0]));
}

TEST_CASE("WavetableOscillator mip level selection keeps harmonics below Nyquist", "[dsp][oscillator]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        for (double frequency = 5.0; frequency < 0.45 * sampleRate; frequency *= 1.07)
        {
            const int mip = WavetableOscillator::mipLevelFor(frequency, sampleRate);
            REQUIRE(mip >= 0);
            REQUIRE(mip < WavetableData::kNumMipLevels);

            // Highest kept harmonic must stay below Nyquist, except at the last level (fundamental only).
            if (mip < WavetableData::kNumMipLevels - 1)
                REQUIRE(WavetableData::maxHarmonics(mip) * frequency <= 0.5 * sampleRate);
            // And the level is the least aggressive that is safe.
            if (mip > 0)
                REQUIRE(WavetableData::maxHarmonics(mip - 1) * frequency > 0.5 * sampleRate);
        }
    }
}

TEST_CASE("WavetableOscillator plays a sine at the requested frequency", "[dsp][oscillator]")
{
    const auto sine = WavetableData::makeSine();

    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        for (double frequency : {55.0, 440.0, 4000.0})
        {
            WavetableOscillator osc;
            osc.prepare(sampleRate);
            osc.setWavetable(sine.get());

            std::vector<float> out(static_cast<size_t>(sampleRate)); // one second
            for (auto& s : out)
                s = osc.process(frequency);

            REQUIRE(fcm::test::allFinite(out));
            REQUIRE_THAT(fcm::test::peak(out), Catch::Matchers::WithinAbs(1.0, 1.0e-3));

            const double measured =
                fcm::test::measureFrequency(out.data(), static_cast<int>(out.size()), sampleRate);
            INFO("sampleRate " << sampleRate << " frequency " << frequency);
            REQUIRE(std::abs(fcm::test::centsBetween(measured, frequency)) < 1.0);
        }
    }
}

TEST_CASE("WavetableOscillator sine is clean", "[dsp][oscillator]")
{
    const auto sine = WavetableData::makeSine();
    WavetableOscillator osc;
    osc.prepare(48000.0);
    osc.setWavetable(sine.get());

    // 3 kHz at 48 kHz is exactly 16 samples per cycle, so 4096 samples hold a whole number of cycles.
    constexpr int n = 4096;
    std::vector<float> out(n);
    for (auto& s : out)
        s = osc.process(3000.0);

    // Error against an ideal sine of the same phase (starts at 0). Linear interpolation error of a
    // 2048-point table is about -118 dB; the budget is -90 dB.
    double errorEnergy = 0.0, signalEnergy = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double ideal = std::sin(2.0 * std::numbers::pi * 3000.0 * i / 48000.0);
        errorEnergy += (out[static_cast<size_t>(i)] - ideal) * (out[static_cast<size_t>(i)] - ideal);
        signalEnergy += ideal * ideal;
    }
    const double distortionDb = 10.0 * std::log10(errorEnergy / signalEnergy);
    REQUIRE(distortionDb < -90.0);
}

TEST_CASE("WavetableOscillator reset is deterministic and honours the start phase", "[dsp][oscillator]")
{
    const auto sine = WavetableData::makeSine();
    WavetableOscillator osc;
    osc.prepare(48000.0);
    osc.setWavetable(sine.get());

    std::vector<float> first(256), second(256);
    for (auto& s : first)
        s = osc.process(333.0);
    osc.reset();
    for (auto& s : second)
        s = osc.process(333.0);
    REQUIRE(first == second);

    osc.reset(0.25);
    REQUIRE_THAT(osc.process(100.0), Catch::Matchers::WithinAbs(1.0, 1.0e-4));
}

TEST_CASE("WavetableOscillator output is finite for extreme frequencies and silent without a table",
          "[dsp][oscillator]")
{
    const auto sine = WavetableData::makeSine();
    WavetableOscillator osc;
    osc.prepare(44100.0);

    REQUIRE(juce::exactlyEqual(osc.process(440.0), 0.0f)); // no table yet

    osc.setWavetable(sine.get());
    std::vector<float> out;
    for (double frequency : {0.0, -10.0, 1.0, 22050.0, 30000.0, 1.0e9})
        for (int i = 0; i < 100; ++i)
            out.push_back(osc.process(frequency));
    REQUIRE(fcm::test::allFinite(out));
    REQUIRE(fcm::test::peak(out) <= 1.001f);
}

TEST_CASE("WavetableOscillator interpolates between frames", "[dsp][oscillator]")
{
    std::vector<float> frames(2 * WavetableData::kFrameSize);
    std::fill(frames.begin(), frames.begin() + WavetableData::kFrameSize, -0.5f);
    std::fill(frames.begin() + WavetableData::kFrameSize, frames.end(), 0.5f);
    const auto table = WavetableData::fromFrames(frames);
    REQUIRE(table != nullptr);

    WavetableOscillator osc;
    osc.prepare(48000.0);
    osc.setWavetable(table.get());

    // Constant frames are DC, which every mip level keeps.
    const std::vector<std::pair<float, float>> cases{
        {0.0f, -0.5f}, {0.25f, -0.25f}, {0.5f, 0.0f}, {1.0f, 0.5f}};
    for (const auto& [position, expected] : cases)
    {
        osc.setPosition(position);
        osc.reset();
        REQUIRE_THAT(osc.process(100.0), Catch::Matchers::WithinAbs(expected, 1.0e-4));
    }
}

TEST_CASE("WavetableOscillator does not alias on bright tables", "[dsp][oscillator][aliasing]")
{
    // 372 whole cycles in 4096 samples: the signal repeats exactly every window, so a rectangular
    // FFT is exact, true harmonics land on bins that are multiples of 372, and any harmonic that
    // folds back over Nyquist lands on other bins (gcd(372, 4096) = 4, first fold is harmonic 6).
    constexpr int kWindow = 4096;
    constexpr int kCycles = 372;
    constexpr double kSampleRate = 44100.0;
    constexpr double kFrequency = kSampleRate * kCycles / kWindow; // about 4006 Hz

    // Energy outside the harmonic bins relative to the energy at them, in dB.
    auto inharmonicDb = [&](const std::vector<float>& signal)
    {
        juce::dsp::FFT fft(12);
        std::vector<float> spectrum(2 * kWindow, 0.0f);
        std::copy(signal.begin(), signal.end(), spectrum.begin());
        fft.performRealOnlyForwardTransform(spectrum.data(), true);

        double harmonic = 0.0, other = 0.0;
        for (int k = 1; k <= kWindow / 2; ++k)
        {
            const auto bin = static_cast<size_t>(k);
            const double power = std::pow(spectrum[2 * bin], 2.0) + std::pow(spectrum[2 * bin + 1], 2.0);
            (k % kCycles == 0 ? harmonic : other) += power;
        }
        return 10.0 * std::log10(other / harmonic);
    };

    const auto table = WavetableData::fromFrames(makeSaw());
    REQUIRE(table != nullptr);
    WavetableOscillator osc;
    osc.prepare(kSampleRate);
    osc.setWavetable(table.get());

    std::vector<float> bandLimited(kWindow), naive(kWindow);
    for (auto& s : bandLimited)
        s = osc.process(kFrequency);

    // The control: a naive saw straight from the phase. If this does not measure as aliased, the
    // metric above cannot tell the difference and the test proves nothing.
    double phase = 0.0;
    for (auto& s : naive)
    {
        s = static_cast<float>(2.0 * phase - 1.0);
        phase += kFrequency / kSampleRate;
        phase -= std::floor(phase);
    }

    REQUIRE(inharmonicDb(naive) > -40.0);
    REQUIRE(inharmonicDb(bandLimited) < -80.0);
}
