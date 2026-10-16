// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EffectDecorations.h"

#include "ui/EqDisplay.h"

namespace fcm::ui
{

const EffectDecoration* decorationFor(std::string_view effectId)
{
    static const EffectDecoration eq{96,
                                     [](juce::AudioProcessorValueTreeState& state,
                                        const juce::String& instanceId) -> std::unique_ptr<juce::Component>
                                     { return std::make_unique<EqDisplay>(state, instanceId); }};

    return effectId == "eq" ? &eq : nullptr;
}

} // namespace fcm::ui
