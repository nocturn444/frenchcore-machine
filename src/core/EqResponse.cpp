// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EqResponse.h"

#include "dsp/fx/eq/EqEffect.h"

#include <cmath>
#include <string>

namespace fcm::core
{

namespace
{
constexpr double kDisplayRate = 48000.0;

double read(juce::AudioProcessorValueTreeState& state, const juce::String& id, double fallback)
{
    if (auto* value = state.getRawParameterValue(id))
    {
        const double v = value->load();
        return std::isfinite(v) ? v : fallback;
    }
    return fallback;
}
} // namespace

double eqFrequencyToUnit(double hz) noexcept
{
    const double clamped = juce::jlimit(kEqMinHz, kEqMaxHz, std::isfinite(hz) ? hz : kEqMinHz);
    return std::log(clamped / kEqMinHz) / std::log(kEqMaxHz / kEqMinHz);
}

double eqUnitToFrequency(double unit) noexcept
{
    const double t = juce::jlimit(0.0, 1.0, std::isfinite(unit) ? unit : 0.0);
    return kEqMinHz * std::pow(kEqMaxHz / kEqMinHz, t);
}

double eqGainToUnit(double db) noexcept
{
    const double clamped = juce::jlimit(-kEqRangeDb, kEqRangeDb, std::isfinite(db) ? db : 0.0);
    return (clamped + kEqRangeDb) / (2.0 * kEqRangeDb);
}

double eqUnitToGain(double unit) noexcept
{
    const double t = juce::jlimit(0.0, 1.0, std::isfinite(unit) ? unit : 0.5);
    return t * 2.0 * kEqRangeDb - kEqRangeDb;
}

bool EqBand::isActive() const noexcept
{
    return dsp::EqEffect::isActive(on, static_cast<dsp::EqEffect::Shape>(type), gain);
}

EqBands readEqBands(juce::AudioProcessorValueTreeState& state)
{
    EqBands bands;
    for (int i = 0; i < kEqBands; ++i)
    {
        const juce::String prefix = "eq.b" + juce::String(i + 1) + ".";
        auto& band = bands[static_cast<size_t>(i)];
        band.on = read(state, prefix + "on", 1.0) >= 0.5;
        band.type = juce::jlimit(0, 4, juce::roundToInt(read(state, prefix + "type", 0.0)));
        band.freq = read(state, prefix + "freq", band.freq);
        band.gain = read(state, prefix + "gain", band.gain);
        band.q = read(state, prefix + "q", band.q);
    }
    return bands;
}

double eqBandResponseDb(const EqBand& band, double hz) noexcept
{
    if (!band.isActive())
        return 0.0;
    const auto design = dsp::EqEffect::design(static_cast<dsp::EqEffect::Shape>(band.type), kDisplayRate,
                                              band.freq, band.q, band.gain);
    return dsp::EqEffect::magnitudeDb(design, kDisplayRate, hz);
}

double eqResponseDb(const EqBands& bands, double hz) noexcept
{
    double total = 0.0;
    for (const auto& band : bands)
        total += eqBandResponseDb(band, hz);
    return total;
}

} // namespace fcm::core
