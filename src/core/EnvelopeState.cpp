// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EnvelopeState.h"

namespace fcm::core
{

namespace
{
const juce::Identifier kPointType{"Point"};
const juce::Identifier kReleaseMs{"releaseMs"};
const juce::Identifier kSyncBpm{"syncBpm"};
const juce::Identifier kTime{"time"};
const juce::Identifier kValue{"value"};
const juce::Identifier kCurve{"curve"};
const juce::Identifier kMain{"main"};
} // namespace

EnvelopeState::EnvelopeState(dsp::EnvelopeKind kind) : envelope(kind)
{
    publish();
}

void EnvelopeState::publish()
{
    published.publish(envelope.toShape());
    releaseSeconds.store(envelope.releaseMs() * 0.001f);
    sendChangeMessage();
}

void EnvelopeState::set(const dsp::BreakpointEnvelope& newEnvelope)
{
    envelope = newEnvelope;
    publish();
}

void EnvelopeState::edit(const std::function<void(dsp::BreakpointEnvelope&)>& change)
{
    change(envelope);
    publish();
}

juce::Identifier EnvelopeState::nodeType(dsp::EnvelopeKind kind)
{
    return kind == dsp::EnvelopeKind::amp ? juce::Identifier{"AmpEnvelope"}
                                          : juce::Identifier{"PitchEnvelope"};
}

juce::ValueTree EnvelopeState::toValueTree() const
{
    juce::ValueTree node(nodeType(envelope.kind()));
    node.setProperty(kReleaseMs, static_cast<double>(envelope.releaseMs()), nullptr);
    if (envelope.isSynced())
        node.setProperty(kSyncBpm, static_cast<double>(envelope.syncRefBpm()), nullptr);

    for (const auto& point : envelope.points())
    {
        juce::ValueTree child(kPointType);
        child.setProperty(kTime, static_cast<double>(point.timeMs), nullptr);
        child.setProperty(kValue, static_cast<double>(point.value), nullptr);
        child.setProperty(kCurve, static_cast<double>(point.curve), nullptr);
        child.setProperty(kMain, point.isMain, nullptr);
        node.appendChild(child, nullptr);
    }
    return node;
}

bool EnvelopeState::restoreFrom(const juce::ValueTree& node)
{
    const auto fallBackToDefault = [this]
    {
        set(dsp::BreakpointEnvelope(envelope.kind()));
        return false;
    };

    if (!node.isValid() || node.getType() != nodeType(envelope.kind()) || !node.hasProperty(kReleaseMs) ||
        node.getNumChildren() > dsp::kMaxEnvelopePoints)
        return fallBackToDefault();

    std::vector<dsp::EnvelopePoint> points;
    for (const auto& child : node)
    {
        if (child.getType() != kPointType || !child.hasProperty(kTime) || !child.hasProperty(kValue) ||
            !child.hasProperty(kCurve) || !child.hasProperty(kMain))
            return fallBackToDefault();

        points.push_back({static_cast<float>(static_cast<double>(child[kTime])),
                          static_cast<float>(static_cast<double>(child[kValue])),
                          static_cast<float>(static_cast<double>(child[kCurve])),
                          static_cast<bool>(child[kMain])});
    }

    // The sync tempo is optional: absent means the envelope is not synced.
    const auto syncBpm =
        node.hasProperty(kSyncBpm) ? static_cast<float>(static_cast<double>(node[kSyncBpm])) : 0.0f;
    const auto restored = dsp::BreakpointEnvelope::tryCreate(
        envelope.kind(), std::move(points), static_cast<float>(static_cast<double>(node[kReleaseMs])),
        syncBpm);
    if (!restored.has_value())
        return fallBackToDefault();

    set(*restored);
    return true;
}

} // namespace fcm::core
