// SPDX-License-Identifier: AGPL-3.0-only
#include "core/WavetableSelector.h"

#include "core/ParameterIds.h"
#include "dsp/oscillator/FactoryWavetables.h"

#include <algorithm>
#include <cmath>

namespace fcm::core
{

WavetableSelector::WavetableSelector(juce::AudioProcessorValueTreeState& stateIn, int oscillatorIndex)
    : state(stateIn),
      parameterId(ParameterIds::oscillator(oscillatorIndex, ParameterIds::Oscillator::kWavetable))
{
    raw = state.getRawParameterValue(parameterId);
    jassert(raw != nullptr); // layout and selector out of sync
    update();
    state.addParameterListener(parameterId, this);
}

WavetableSelector::~WavetableSelector()
{
    state.removeParameterListener(parameterId, this);
    cancelPendingUpdate();
}

int WavetableSelector::wantedEntry() const
{
    // A damaged value never gets here (state loading sanitizes it), but the index is clamped anyway.
    const float value = raw != nullptr ? raw->load() : 0.0f;
    const int index =
        std::isfinite(value) ? static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0e6f))) : 0;
    return std::clamp(index, 0, dsp::FactoryWavetables::count() - 1);
}

void WavetableSelector::update()
{
    const int wanted = wantedEntry();
    if (wanted == entry)
        return;

    if (auto table = dsp::FactoryWavetables::build(wanted))
    {
        published.publish(std::move(table));
        entry = wanted;
    }
}

void WavetableSelector::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void WavetableSelector::handleAsyncUpdate()
{
    update();
}

} // namespace fcm::core
