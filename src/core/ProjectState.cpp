// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ProjectState.h"

#include "core/ParameterIds.h"

namespace fcm::core
{

namespace
{
// Replaces the node of the same type, if any, by `node`.
void putNode(juce::ValueTree& tree, const juce::ValueTree& node)
{
    if (!node.isValid())
        return;

    tree.removeChild(tree.getChildWithName(node.getType()), nullptr);
    tree.appendChild(node, nullptr);
}
} // namespace

juce::ValueTree ProjectState::capture() const
{
    // The version is always written, whatever the tree held before (docs/specs/state-safe-loading.md).
    auto tree = parts.parameters.copyState();
    tree.setProperty("stateVersion", kStateVersion, nullptr);

    putNode(tree, parts.ampEnvelope.toValueTree());
    putNode(tree, parts.pitchEnvelope.toValueTree());
    putNode(tree, parts.fxRacks.toValueTree());
    for (auto* wavetable : parts.wavetables)
        putNode(tree, wavetable->toValueTree());
    putNode(tree, parts.tock.toValueTree());

    return tree;
}

StateLoadResult ProjectState::restore(const juce::ValueTree& saved)
{
    const auto result = loadState(parts.parameters, saved, parts.rootType);
    if (result == StateLoadResult::rejected)
        return result;

    // A missing or damaged node gives the default (or no user data) instead of failing the load.
    parts.ampEnvelope.restoreFrom(saved.getChildWithName(EnvelopeState::nodeType(dsp::EnvelopeKind::amp)));
    parts.pitchEnvelope.restoreFrom(
        saved.getChildWithName(EnvelopeState::nodeType(dsp::EnvelopeKind::pitch)));
    parts.fxRacks.restoreFrom(saved.getChildWithName(FxRackState::nodeType()));
    parts.tock.restoreFrom(saved.getChildWithName("UserTockSample"));
    parts.tock.update();

    // User wavetables first, then the table of each current Wavetable value (the new parameter
    // values schedule that too; doing it now makes the load complete at once).
    for (size_t i = 0; i < parts.wavetables.size(); ++i)
    {
        parts.wavetables[i]->restoreFrom(saved.getChildWithName("UserWavetable" + juce::String(i + 1)));
        parts.wavetables[i]->update();
    }

    return result;
}

std::unique_ptr<juce::XmlElement> ProjectState::toXml() const
{
    return capture().createXml();
}

StateLoadResult ProjectState::restoreFromXml(const juce::XmlElement& xml)
{
    if (!xml.hasTagName(parts.rootType))
        return StateLoadResult::rejected;
    return restore(juce::ValueTree::fromXml(xml));
}

} // namespace fcm::core
