// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/EffectRegistry.h"

namespace fcm::dsp
{

// The only place that names effects: one line per effect, appended at the end. The order here is the
// default chain order and the index of the effect; never reshuffle it (docs/ADDING_AN_EFFECT.md).
void registerEffects(EffectRegistry& registry)
{
    juce::ignoreUnused(registry);
}

} // namespace fcm::dsp
