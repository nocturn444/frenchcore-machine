// SPDX-License-Identifier: AGPL-3.0-only
#include "../TestUtils.h"
#include "dsp/fx/formant/FormantEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <vector>

using fcm::dsp::FormantEffect;

namespace
{
constexpr int kBlock = 256;
constexpr double kRate = 48000.0;
constexpr int kResponseLength = 1 << 15;

struct Settings
{
    float vowel = 0.0f;
    float morph = 0.0f;
    float shift = 0.0f;
    float resonance = 50.0f;
    float drive = 0.0f;
    std::vector<float> values() const { return {vowel, morph, shift, resonance, drive}; }
};

std::vector<float> run(const Settings& settings, const std::vector<float>& input)
{
    FormantEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());

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

// The magnitude response, read from the impulse response (the effect is linear with Drive 0).
struct Response
{
    std::vector<float> impulse;

    explicit Response(const Settings& settings)
    {
        std::vector<float> input(kResponseLength, 0.0f);
        input[0] = 1.0f;
        impulse = run(settings, input);
    }

    double magnitude(double hz) const
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < impulse.size(); ++i)
        {
            const double w = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / kRate;
            re += impulse[i] * std::cos(w);
            im -= impulse[i] * std::sin(w);
        }
        return std::sqrt(re * re + im * im);
    }

    // The three highest peaks, lowest frequency first.
    std::vector<double> peaks() const
    {
        std::vector<double> hz, level;
        for (double f = 100.0; f < 6000.0; f *= 1.01)
        {
            hz.push_back(f);
            level.push_back(magnitude(f));
        }
        std::vector<std::pair<double, double>> found; // level, hz
        for (size_t i = 1; i + 1 < hz.size(); ++i)
            if (level[i] > level[i - 1] && level[i] >= level[i + 1])
                found.push_back({level[i], hz[i]});
        std::sort(found.rbegin(), found.rend());
        std::vector<double> top;
        for (size_t i = 0; i < std::min<size_t>(3, found.size()); ++i)
            top.push_back(found[i].second);
        std::sort(top.begin(), top.end());
        return top;
    }

    // The power of pink noise through the filter: equal weight per octave.
    double pinkPower() const
    {
        double total = 0.0;
        int count = 0;
        for (double f = 20.0; f < 20000.0; f *= 1.03, ++count)
        {
            const double m = magnitude(f);
            total += m * m;
        }
        return total / count;
    }
};
} // namespace

TEST_CASE("Each vowel has its three formants where the table puts them", "[dsp][fx][formant]")
{
    for (int v = 0; v < FormantEffect::kVowelCount; ++v)
    {
        Settings settings;
        settings.vowel = static_cast<float>(v);
        const auto found = Response(settings).peaks();
        REQUIRE(found.size() == 3);
        auto expected = FormantEffect::vowel(v).hz;
        std::sort(expected.begin(), expected.end());
        for (size_t i = 0; i < 3; ++i)
        {
            INFO("vowel " << v << " formant " << i << ": " << found[i] << " Hz, expected " << expected[i]);
            REQUIRE(std::abs(found[i] - expected[i]) < 0.1 * expected[i]);
        }
    }
}

TEST_CASE("Morph 50 % sits between two vowels and Shift moves every formant", "[dsp][fx][formant]")
{
    // A (730, 1090, 2440) towards E (530, 1840, 2480).
    Settings halfway;
    halfway.morph = 50.0f;
    const auto middle = Response(halfway).peaks();
    REQUIRE(middle.size() == 3);
    REQUIRE(middle[0] > 530.0);
    REQUIRE(middle[0] < 730.0);
    REQUIRE(middle[1] > 1090.0);
    REQUIRE(middle[1] < 1840.0);

    Settings octave;
    octave.shift = 12.0f;
    const auto up = Response(octave).peaks();
    REQUIRE(up.size() == 3);
    const auto& a = FormantEffect::vowel(0).hz;
    REQUIRE(std::abs(up[0] - 2.0 * a[0]) < 0.05 * 2.0 * a[0]);
    REQUIRE(std::abs(up[1] - 2.0 * a[1]) < 0.05 * 2.0 * a[1]);

    Settings down;
    down.shift = -12.0f;
    const auto low = Response(down).peaks();
    REQUIRE(low.size() >= 1);
}

TEST_CASE("The five vowels come out at the same pink-noise level, and Resonance narrows the peaks",
          "[dsp][fx][formant]")
{
    std::vector<double> levels;
    for (int v = 0; v < FormantEffect::kVowelCount; ++v)
    {
        Settings settings;
        settings.vowel = static_cast<float>(v);
        levels.push_back(10.0 * std::log10(Response(settings).pinkPower()));
    }
    const auto [lowest, highest] = std::minmax_element(levels.begin(), levels.end());
    INFO("levels " << *lowest << " to " << *highest << " dB");
    REQUIRE(*highest - *lowest < 1.0);

    // The -3 dB width of the first formant of A.
    const auto width = [](float resonance)
    {
        Settings settings;
        settings.resonance = resonance;
        const Response response(settings);
        double peak = 0.0;
        for (double f = 500.0; f < 1000.0; f += 2.0)
            peak = std::max(peak, response.magnitude(f));
        double low = 0.0, high = 0.0;
        for (double f = 500.0; f < 1000.0; f += 2.0)
            if (response.magnitude(f) > peak * 0.7071)
            {
                if (low == 0.0)
                    low = f;
                high = f;
            }
        return high - low;
    };
    REQUIRE(width(100.0f) < width(50.0f));
    REQUIRE(width(50.0f) < width(0.0f));
}

TEST_CASE("Changing the vowel on a sine does not click", "[dsp][fx][formant]")
{
    FormantEffect effect;
    effect.prepare({kRate, kBlock, 2});
    Settings settings;
    effect.setParameters(settings.values());

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    constexpr int kBlocks = 160;
    constexpr int kChangeAt = 80;
    for (int block = 0; block < kBlocks; ++block)
    {
        if (block == kChangeAt)
        {
            settings.vowel = 2.0f; // I
            effect.setParameters(settings.values());
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i,
                                 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 500.0 *
                                                                    (block * kBlock + i) / kRate)));
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }

    const auto largestStep = [&](size_t from, size_t to)
    {
        float most = 0.0f;
        for (size_t i = from + 1; i < to; ++i)
            most = std::max(most, std::abs(out[i] - out[i - 1]));
        return most;
    };
    const size_t change = static_cast<size_t>(kChangeAt) * kBlock;
    const float before = largestStep(change - 4096, change);
    const float after = largestStep(out.size() - 4096, out.size());
    // The formants glide over 20 ms (and while one passes the tone the level rises, which is the
    // vowel doing its job); a hard switch would show as a jump at the very sample of the change.
    const float atChange = largestStep(change - 1, change + 64);
    INFO("steps: before " << before << ", at the change " << atChange << ", after " << after);
    REQUIRE(atChange <= 1.2f * std::max(before, after));
}

TEST_CASE("Drive 0 is clean and a full-scale sweep stays bounded and finite", "[dsp][fx][formant]")
{
    Settings clean;
    const auto input = std::vector<float>(kBlock * 8, 0.0f);
    REQUIRE(fcm::test::allFinite(run(clean, input)));

    Settings driven;
    driven.drive = 100.0f;
    driven.resonance = 100.0f;
    std::vector<float> sweep(kBlock * 400);
    double phase = 0.0;
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        const double hz = 30.0 * std::pow(300.0, static_cast<double>(i) / static_cast<double>(sweep.size()));
        phase += 2.0 * std::numbers::pi * hz / kRate;
        sweep[i] = static_cast<float>(std::sin(phase));
    }
    for (int v = 0; v < FormantEffect::kVowelCount; ++v)
    {
        driven.vowel = static_cast<float>(v);
        const auto out = run(driven, sweep);
        REQUIRE(fcm::test::allFinite(out));
        REQUIRE(fcm::test::peak(out) < 20.0f);
    }
}

TEST_CASE("The effect is deterministic after a reset and has no latency", "[dsp][fx][formant]")
{
    FormantEffect effect;
    effect.prepare({kRate, kBlock, 2});
    REQUIRE(effect.latencySamples(8) == 0);

    Settings settings;
    effect.setParameters(settings.values());
    const auto render = [&]
    {
        effect.reset();
        effect.setParameters(settings.values());
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int i = 0; i < kBlock; ++i)
        {
            buffer.setSample(0, i, i == 0 ? 1.0f : 0.0f);
            buffer.setSample(1, i, i == 0 ? 1.0f : 0.0f);
        }
        effect.process(buffer);
        return std::vector<float>(buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    };
    REQUIRE(render() == render());
}

TEST_CASE("Hostile parameter values give finite sound", "[dsp][fx][formant]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const auto& values :
         {std::vector<float>{nan, nan, nan, nan, nan}, std::vector<float>{inf, inf, inf, inf, inf},
          std::vector<float>{-inf, -1e9f, -1e9f, -1e9f, -1e9f},
          std::vector<float>{99.0f, 1e9f, 1e9f, 1e9f, 1e9f}})
    {
        FormantEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(values);
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int i = 0; i < kBlock; ++i)
        {
            const float sample = i == 3 ? nan : 0.5f * static_cast<float>(std::sin(0.05 * i));
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }
        effect.process(buffer);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                REQUIRE(std::isfinite(buffer.getSample(c, i)));
    }
}
