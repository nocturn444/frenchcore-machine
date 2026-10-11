// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/ParameterLayout.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::test
{

// A processor with no audio, only to host the parameter tree.
class TestProcessor : public juce::AudioProcessor
{
  public:
    TestProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "FrenchcoreMachineState", fcm::core::createParameterLayout())
    {
    }

    const juce::String getName() const override { return "Test"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState state;
};

} // namespace fcm::test
