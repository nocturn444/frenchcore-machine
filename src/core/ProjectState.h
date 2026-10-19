// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/EnvelopeState.h"
#include "core/FxRackState.h"
#include "core/NoiseSelector.h"
#include "core/StateLoader.h"
#include "core/TockSelector.h"
#include "core/WavetableSelector.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::core
{

// Everything that makes up a sound, gathered so that the processor, the presets and the tests share
// one way to save and load it (docs/specs/project-state.md). It does not own the pieces.
class ProjectState
{
  public:
    struct Parts
    {
        juce::AudioProcessorValueTreeState& parameters;
        juce::Identifier rootType;
        EnvelopeState& ampEnvelope;
        EnvelopeState& pitchEnvelope;
        FxRackState& fxRacks;
        std::array<WavetableSelector*, 2> wavetables;
        TockSelector& tock;
        NoiseSelector& noise;
    };

    explicit ProjectState(const Parts& parts) : parts(parts) {}

    // The whole state as one tree, with `stateVersion` written. Message thread.
    juce::ValueTree capture() const;

    // Loads a tree into everything, treating it as untrusted input. A tree that is not the plugin's
    // state changes nothing and returns `rejected`. Message thread.
    StateLoadResult restore(const juce::ValueTree& saved);

    // The XML form the host stores.
    std::unique_ptr<juce::XmlElement> toXml() const;
    StateLoadResult restoreFromXml(const juce::XmlElement& xml);

  private:
    Parts parts;
};

} // namespace fcm::core
