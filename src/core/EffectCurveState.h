// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/PublishedPointer.h"
#include "dsp/fx/EffectCurve.h"
#include "dsp/fx/EffectRegistry.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <memory>
#include <vector>

namespace fcm::core
{

// The drawn curves of the effects that have one (docs/decisions/0012 item 3): one editable curve per
// instance of every such effect, published to the audio thread through a PublishedPointer like the
// envelopes (ADR 0006) and announced to the UI through the ChangeBroadcaster. Message thread only,
// except that the audio thread acquires from the publishers. A curve is not a parameter: it is saved
// in a child node of the state, and an instance without a saved curve has its effect's default.
class EffectCurveState : public juce::ChangeBroadcaster
{
  public:
    // `registry` must outlive this object.
    explicit EffectCurveState(const dsp::EffectRegistry& registry);

    // Whether the instance's effect has a drawn curve at all.
    bool hasCurve(int instance) const noexcept;

    // The instance's current curve (its effect's default for an effect without edits). For an
    // instance without a curve, an empty curve (count 0).
    const dsp::EffectCurve& get(int instance) const noexcept;

    // Where the audio thread acquires the instance's curve, or nullptr when it has no curve.
    dsp::PublishedPointer<dsp::EffectCurve>* source(int instance) noexcept;

    // Replaces the instance's curve and publishes it. A curve that is not valid, or an instance that
    // has no curve, changes nothing and returns false.
    bool set(int instance, const dsp::EffectCurve& curve);

    // The instance's curve goes back to the effect's default (what adding the effect does).
    void reset(int instance);

    // Copies one instance's curve to another of the same effect (what duplicating does).
    void copy(int from, int to);

    // Saved form: a node "EffectCurves" with a "Curve" child per instance that has a curve (property
    // "instance": "volumelfo2") holding "Point" children (x, y, curve). Every curve is saved.
    juce::ValueTree toValueTree() const;

    // Restores from a saved node, which is untrusted input (docs/specs/state-safe-loading.md): every
    // curve first goes back to the default; an unknown instance, an instance of an effect without a
    // curve, an instance listed twice (the first counts) and a curve that is not valid (fewer than 2
    // or more than 32 points, a number that is not finite, `x` outside 0..1 or going backwards, ends
    // not at 0 and 1) leave that instance at its default. A missing node leaves everything default.
    void restoreFrom(const juce::ValueTree& node);

    static const juce::Identifier& nodeType();

  private:
    struct Entry
    {
        bool has = false;
        dsp::EffectCurve curve;
        std::unique_ptr<dsp::PublishedPointer<dsp::EffectCurve>> published;
    };

    void publish(Entry& entry);
    Entry* entryFor(int instance) noexcept;
    const Entry* entryFor(int instance) const noexcept;

    const dsp::EffectRegistry& registry;
    std::vector<Entry> entries; // one per pool index
    dsp::EffectCurve none;      // returned for instances without a curve
};

} // namespace fcm::core
