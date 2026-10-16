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

double eqGainToUnit(double db, double rangeDb) noexcept
{
    const double clamped = juce::jlimit(-rangeDb, rangeDb, std::isfinite(db) ? db : 0.0);
    return (clamped + rangeDb) / (2.0 * rangeDb);
}

double eqUnitToGain(double unit, double rangeDb) noexcept
{
    const double t = juce::jlimit(0.0, 1.0, std::isfinite(unit) ? unit : 0.5);
    return t * 2.0 * rangeDb - rangeDb;
}

double eqDisplayRangeDb(const EqBands& bands) noexcept
{
    for (const auto& band : bands)
        if (band.extreme)
            return kEqExtremeRangeDb;
    return kEqRangeDb;
}

bool EqBand::isActive() const noexcept
{
    return dsp::EqEffect::isActive(on, static_cast<dsp::EqEffect::Shape>(type), gain);
}

EqBands readEqBands(juce::AudioProcessorValueTreeState& state, const juce::String& instanceId)
{
    EqBands bands;
    for (int i = 0; i < kEqBands; ++i)
    {
        const juce::String prefix = instanceId + ".b" + juce::String(i + 1) + ".";
        auto& band = bands[static_cast<size_t>(i)];
        band.on = read(state, prefix + "on", 1.0) >= 0.5;
        band.type = juce::jlimit(0, 4, juce::roundToInt(read(state, prefix + "type", 0.0)));
        band.freq = read(state, prefix + "freq", band.freq);
        band.extreme = read(state, prefix + "extreme", 0.0) >= 0.5;
        band.keyTrack = read(state, prefix + "keytrack", 0.0);
        // Extreme swaps Gain and Q for its own, wider controls (docs/specs/fx-eq.md section 8).
        band.gain = read(state, prefix + (band.extreme ? "xgain" : "gain"), band.gain);
        band.q = read(state, prefix + (band.extreme ? "xq" : "q"), band.q);
    }
    return bands;
}

double EqBand::frequencyAt(double noteSemitones) const noexcept
{
    return dsp::EqEffect::keyTrackedFrequency(freq, keyTrack, noteSemitones, kDisplayRate);
}

double eqBandResponseDb(const EqBand& band, double hz, double noteSemitones) noexcept
{
    if (!band.isActive())
        return 0.0;
    const auto design = dsp::EqEffect::design(static_cast<dsp::EqEffect::Shape>(band.type), kDisplayRate,
                                              band.frequencyAt(noteSemitones), band.q, band.gain);
    return dsp::EqEffect::magnitudeDb(design, kDisplayRate, hz);
}

double eqResponseDb(const EqBands& bands, double hz, double noteSemitones) noexcept
{
    double total = 0.0;
    for (const auto& band : bands)
        total += eqBandResponseDb(band, hz, noteSemitones);
    return total;
}

} // namespace fcm::core
