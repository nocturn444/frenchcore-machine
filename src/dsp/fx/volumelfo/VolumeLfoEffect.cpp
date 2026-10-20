// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/volumelfo/VolumeLfoEffect.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace fcm::dsp
{

namespace
{
constexpr double kDepthSmoothSeconds = 0.010;
constexpr double kShortestSlew = 0.0002; // Smooth 0 %: 0.2 ms, so an edge is never a click
constexpr double kMiddleSlew = 0.005;    // Smooth 50 %
constexpr double kLongestSlew = 0.020;   // Smooth 100 %

// Bit 0: Sync off, bit 1: on (ParameterSpec::shownWhen).
constexpr unsigned kFreeRunning = 1u << 0;
constexpr unsigned kSynced = 1u << 1;

enum Param
{
    pSync,
    pRate,
    pDivision,
    pDepth,
    pSmooth,
    kNumParams
};

float finiteOr(float x, float fallback) noexcept
{
    return std::isfinite(x) ? x : fallback;
}

using Points = std::vector<CurvePoint>;

NamedCurve named(const char* name, const Points& points)
{
    return {name, EffectCurve::from(points)};
}

// A gate pattern: `repeats` times full for the first half of its share of the cycle, then silent.
Points gate(int repeats)
{
    Points points;
    const float share = 1.0f / static_cast<float>(repeats);
    for (int i = 0; i < repeats; ++i)
    {
        const float start = share * static_cast<float>(i);
        points.push_back({start, 1.0f, 0.0f});
        points.push_back({start + share * 0.5f, 1.0f, 0.0f});
        points.push_back({start + share * 0.5f, 0.0f, 0.0f});
        points.push_back({start + share, 0.0f, 0.0f});
    }
    points.front().x = 0.0f;
    points.back().x = 1.0f;
    return points;
}

// Cosine, from full level down to silence and back, as eight straight segments (a cosine cannot be
// followed closely by one bend per half).
Points sine()
{
    Points points;
    for (int i = 0; i <= 8; ++i)
    {
        const float x = static_cast<float>(i) / 8.0f;
        points.push_back({x, 0.5f + 0.5f * std::cos(2.0f * 3.14159265358979f * x), 0.0f});
    }
    points.front().y = 1.0f;
    points.back().y = 1.0f;
    return points;
}

const std::array<NamedCurve, 10>& presets()
{
    // The values are the agent's first voicing, for the product owner's ears (docs/specs/fx-volumelfo.md).
    static const std::array<NamedCurve, 10> list = {{
        named("Pump", {{0.0f, 1.0f, 0.0f}, {0.45f, 1.0f, -0.6f}, {0.9f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}),
        named("Sidechain", {{0.0f, 0.0f, 0.6f}, {0.35f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}),
        named("Gate 1/2", gate(1)),
        named("Gate 1/4", gate(2)),
        named("Gate 1/8", gate(4)),
        named("Saw Down", {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}),
        named("Saw Up", {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}),
        named("Triangle", {{0.0f, 1.0f, 0.0f}, {0.5f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}),
        named("Sine", sine()),
        named("Square", {{0.0f, 1.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {0.5f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}),
    }};
    return list;
}
} // namespace

VolumeLfoEffect::VolumeLfoEffect() : curve(presets()[0].curve) {}

const EffectDescriptor& VolumeLfoEffect::descriptor() const
{
    static const EffectDescriptor d{"volumelfo", "Volume LFO", 1, 1.0f, false};
    return d;
}

std::span<const ParameterSpec> VolumeLfoEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"sync", "Sync", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
        {"rate",
         "Rate",
         ParameterKind::number,
         0.05f,
         30.0f,
         0.01f,
         2.0f,
         2.0f,
         "Hz",
         {},
         "sync",
         kFreeRunning},
        {"division", "Rate", ParameterKind::choice, 0.0f, 13.0f, 1.0f, 9.0f, 0.0f, "", kLfoDivisionNames,
         "sync", kSynced},
        {"depth", "Depth", ParameterKind::number, 0.0f, 100.0f, 0.1f, 100.0f, 0.0f, "%", {}},
        {"smooth", "Smooth", ParameterKind::number, 0.0f, 100.0f, 0.1f, 10.0f, 0.0f, "%", {}},
    };
    return specs;
}

const EffectCurve* VolumeLfoEffect::defaultCurve() const noexcept
{
    return &presets()[0].curve; // Pump
}

std::span<const NamedCurve> VolumeLfoEffect::curvePresets() const noexcept
{
    return presets();
}

double VolumeLfoEffect::smoothSeconds(double s) noexcept
{
    s = std::clamp(std::isfinite(s) ? s : 0.0, 0.0, 1.0);
    return s <= 0.5 ? kShortestSlew * std::pow(kMiddleSlew / kShortestSlew, s * 2.0)
                    : kMiddleSlew * std::pow(kLongestSlew / kMiddleSlew, (s - 0.5) * 2.0);
}

void VolumeLfoEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    lfo.prepare(sampleRate);
    depth.reset(sampleRate, kDepthSmoothSeconds);
    curve = *defaultCurve();
    reset();
}

void VolumeLfoEffect::reset() noexcept
{
    lfo.reset();
    pending.clear();
    segment = 0;
    gain = 1.0;
    snapSmoothers = true;
}

void VolumeLfoEffect::setCurve(const EffectCurve* newCurve) noexcept
{
    // nullptr means the default; anything that is not valid is not used.
    const EffectCurve* wanted = newCurve != nullptr && newCurve->isValid() ? newCurve : defaultCurve();
    curve = *wanted;
}

void VolumeLfoEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < kNumParams)
        return;

    sync = finiteOr(values[pSync], 1.0f) >= 0.5f;
    division = std::clamp(static_cast<int>(std::lround(finiteOr(values[pDivision], 9.0f))), 0,
                          static_cast<int>(kLfoDivisions.size()) - 1);
    rateHz = std::clamp(finiteOr(values[pRate], 2.0f), 0.05f, 30.0f);
    smooth = std::clamp(finiteOr(values[pSmooth], 10.0f) * 0.01f, 0.0f, 1.0f);

    const float depthTarget = std::clamp(finiteOr(values[pDepth], 100.0f) * 0.01f, 0.0f, 1.0f);
    if (snapSmoothers)
    {
        depth.setCurrentAndTargetValue(depthTarget);
        snapSmoothers = false;
    }
    else
    {
        depth.setTargetValue(depthTarget);
    }
}

void VolumeLfoEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    std::array<float*, 2> data{};
    for (int c = 0; c < numChannels; ++c)
        data[static_cast<size_t>(c)] = buffer.getWritePointer(c);

    if (sync)
        lfo.setSynced(division, static_cast<double>(context.tempoBpm));
    else
        lfo.setRateHz(static_cast<double>(rateHz));
    const double slew = 1.0 - std::exp(-1.0 / (smoothSeconds(static_cast<double>(smooth)) * sampleRate));

    // Cut at the note-ons: the shape starts again at each. The gain slew goes on from where it is.
    pending.run(numSamples,
                [&](int begin, int end, bool restart)
                {
                    if (restart)
                    {
                        lfo.restart();
                        segment = 0;
                    }
                    for (int i = begin; i < end; ++i)
                    {
                        const double shape = static_cast<double>(curve.valueAt(lfo.next(), segment));
                        const double amount = static_cast<double>(depth.getNextValue());
                        const double target = 1.0 - amount * (1.0 - shape);
                        gain += slew * (target - gain);
                        const float g = static_cast<float>(gain);
                        for (int c = 0; c < numChannels; ++c)
                        {
                            float& sample = data[static_cast<size_t>(c)][i];
                            sample = std::isfinite(sample) ? sample * g : 0.0f;
                        }
                    }
                });
}

} // namespace fcm::dsp
