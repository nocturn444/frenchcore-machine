// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
bool allSamplesAre(const juce::AudioBuffer<float>& buffer, float value)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (!juce::exactlyEqual(buffer.getSample(ch, i), value))
                return false;
    return true;
}
} // namespace

TEST_CASE("Engine renders silence before any DSP is implemented", "[dsp][engine]")
{
    fcm::dsp::Engine engine;
    engine.prepare({48000.0, 512, 2});

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    buffer.setSample(0, 10, 0.5f); // garbage left by the host must be overwritten
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);

    engine.process(buffer, midi);

    REQUIRE(allSamplesAre(buffer, 0.0f));
}

TEST_CASE("Engine keeps the prepared sample rate", "[dsp][engine]")
{
    fcm::dsp::Engine engine;
    engine.prepare({96000.0, 256, 2});
    REQUIRE(juce::exactlyEqual(engine.getSampleRate(), 96000.0));
}
