// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TockSelector.h"

#include "core/ParameterIds.h"
#include "dsp/tock/FactoryTocks.h"

#include <algorithm>
#include <cmath>

namespace fcm::core
{

TockSelector::TockSelector(juce::AudioProcessorValueTreeState& stateIn) : state(stateIn)
{
    raw = state.getRawParameterValue(ParameterIds::kTockSample);
    jassert(raw != nullptr); // layout and selector out of sync
    update();
    state.addParameterListener(ParameterIds::kTockSample, this);
}

TockSelector::~TockSelector()
{
    state.removeParameterListener(ParameterIds::kTockSample, this);
    cancelPendingUpdate();
}

int TockSelector::wantedEntry() const
{
    // A damaged value never gets here (state loading sanitizes it), but the index is clamped anyway.
    const float value = raw != nullptr ? raw->load() : 0.0f;
    const int index =
        std::isfinite(value) ? static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0e6f))) : 0;
    return std::clamp(index, 0, dsp::FactoryTocks::count() - 1);
}

void TockSelector::update()
{
    const int wanted = wantedEntry();
    if (wanted == entry)
        return;

    if (auto sample = dsp::FactoryTocks::build(wanted))
    {
        published.publish(std::move(sample));
        entry = wanted;
    }
}

void TockSelector::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void TockSelector::handleAsyncUpdate()
{
    update();
}

} // namespace fcm::core
