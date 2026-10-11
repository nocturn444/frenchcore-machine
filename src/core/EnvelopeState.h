// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/BreakpointEnvelope.h"
#include "dsp/envelope/EnvelopeShape.h"
#include "dsp/envelope/PublishedPointer.h"

#include <atomic>
#include <functional>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace fcm::core
{

// One envelope as the plugin keeps it: the editable model (message thread), the immutable copy
// the audio thread reads, and its saved form (docs/specs/envelope-breakpoint.md §4.3, §4.4).
// Every change is published to the audio thread at once and announced to listeners (the UI)
// asynchronously, so the display and knobs can follow.
class EnvelopeState : public juce::ChangeBroadcaster
{
  public:
    explicit EnvelopeState(dsp::EnvelopeKind kind);

    dsp::EnvelopeKind kind() const noexcept { return envelope.kind(); }

    // Message thread (or whichever single thread edits).
    const dsp::BreakpointEnvelope& get() const noexcept { return envelope; }
    void set(const dsp::BreakpointEnvelope& newEnvelope);
    void edit(const std::function<void(dsp::BreakpointEnvelope&)>& change);

    // The audio thread acquires its shape here, once per block.
    dsp::PublishedPointer<dsp::EnvelopeShape>& source() noexcept { return published; }

    // Any thread: how long the release lasts, for the tail length reported to the host.
    float getReleaseSeconds() const noexcept { return releaseSeconds.load(); }

    // Saved form: a node named AmpEnvelope or PitchEnvelope with a Point child per point.
    juce::ValueTree toValueTree() const;
    static juce::Identifier nodeType(dsp::EnvelopeKind kind);

    // Restores from a saved node. A missing, wrongly named or invalid node gives the default
    // shape and returns false.
    bool restoreFrom(const juce::ValueTree& node);

  private:
    void publish();

    dsp::BreakpointEnvelope envelope;
    dsp::PublishedPointer<dsp::EnvelopeShape> published;
    std::atomic<float> releaseSeconds{0.03f};
};

} // namespace fcm::core
