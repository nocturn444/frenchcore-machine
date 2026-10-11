// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/Engine.h"

namespace fcm::dsp
{

void Engine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = spec.maximumBlockSize;
    numChannels = spec.numChannels;
    reset();
}

void Engine::reset() noexcept {}

void Engine::process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept
{
    juce::ignoreUnused(midi);
    buffer.clear();
}

} // namespace fcm::dsp
