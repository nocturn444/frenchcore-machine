// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/ParameterIds.h"
#include "core/ParameterLayout.h"
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <set>
#include <string>

using fcm::test::TestProcessor;

namespace
{
// Sets a parameter to a real (not normalized) value.
void setValue(juce::AudioProcessorValueTreeState& state, const std::string& id, float value)
{
    auto* parameter = state.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
} // namespace

TEST_CASE("Released parameter IDs are unchanged", "[core][parameters]")
{
    // Parameter IDs are permanent once released (docs/ARCHITECTURE.md §5). If this test needs
    // editing, a saved project or automation lane would break: add parameters, never rename.
    const std::set<std::string> expected{
        "global.outputGain",
        "global.limiterOn",
        "global.keyTrack",
        "global.rootNote",
        "global.glideOn",
        "global.glideTime",
        "global.velocitySensitivity",
        "global.quality",
        "ampEnv.timeScale",
        "pitchEnv.timeScale",
        "pitchEnv.amount",
        "punchTail.split",
        "punchTail.punchLevel",
        "punchTail.tailLevel",
        "tock.on",
        "tock.mode",
        "tock.sample",
        "tock.level",
        "tock.tune",
        "tock.start",
        "tock.decay",
        "tock.tone",
        "distortion.enabled",
        "distortion.mix",
        "distortion.type",
        "distortion.drive",
        "distortion.pregain",
        "distortion.postgain",
        "distortion.tone",
        "distortion.bits",
        "distortion.rate",
        "eq.enabled",
        "eq.mix",
        "eq.b1.on",
        "eq.b1.type",
        "eq.b1.freq",
        "eq.b1.gain",
        "eq.b1.q",
        "eq.b2.on",
        "eq.b2.type",
        "eq.b2.freq",
        "eq.b2.gain",
        "eq.b2.q",
        "eq.b3.on",
        "eq.b3.type",
        "eq.b3.freq",
        "eq.b3.gain",
        "eq.b3.q",
        "eq.b4.on",
        "eq.b4.type",
        "eq.b4.freq",
        "eq.b4.gain",
        "eq.b4.q",
        "stereo.enabled",
        "stereo.mix",
        "stereo.xover1",
        "stereo.xover2",
        "stereo.lowwidth",
        "stereo.midwidth",
        "stereo.highwidth",
        "stereo.monobelow",
        "osc1.on",
        "osc1.wavetable",
        "osc1.level",
        "osc1.pitch",
        "osc1.octave",
        "osc1.fine",
        "osc1.position",
        "osc1.phase",
        "osc2.on",
        "osc2.wavetable",
        "osc2.level",
        "osc2.pitch",
        "osc2.octave",
        "osc2.fine",
        "osc2.position",
        "osc2.phase",
    };

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;

    std::set<std::string> actual;
    for (auto* parameter : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        REQUIRE(withId != nullptr);
        REQUIRE(actual.insert(withId->paramID.toStdString()).second); // unique
    }

    // Version 0.2 gives every effect four copies; copy 1 keeps the 0.1 IDs and copies 2 to 4 add
    // the number after the effect ID (docs/decisions/0010). Each stage adds its new IDs here.
    std::set<std::string> allowed = expected;

    // Added in 0.2, stage 2 (docs/specs/mixer-and-buses.md section 3): the mixer and the two buses.
    for (const char* source : {"osc1", "osc2", "tock"})
        for (const char* control : {"dest", "mute", "solo"})
            allowed.insert(std::string("mixer.") + source + "." + control);
    for (const char* bus : {"bus.a", "bus.b"})
        for (const char* control : {"level", "region"})
            allowed.insert(std::string(bus) + "." + control);
    for (const auto& id : expected)
        for (const char* effect : {"distortion", "eq", "stereo"})
            if (id.rfind(std::string(effect) + ".", 0) == 0)
                for (int copy = 2; copy <= 4; ++copy)
                    allowed.insert(effect + std::to_string(copy) + id.substr(std::string(effect).size()));

    // Nothing released is missing, and nothing unexpected has appeared.
    for (const auto& id : expected)
        REQUIRE(actual.count(id) == 1);
    REQUIRE(actual == allowed);
}

TEST_CASE("Default parameters map to the default EngineParameters", "[core][parameters]")
{
    // Values go through a 0..1 normalisation and back, which on some CPUs (arm64 fused multiply-add)
    // leaves rounding noise of about 1e-6 on a range of 200 (the Fine control).
    constexpr double kRoundingTolerance = 1.0e-4;

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;
    fcm::core::EngineParameterReader reader(processor.state);

    const auto read = reader.read();
    const fcm::dsp::EngineParameters defaults;

    REQUIRE_THAT(read.outputGain, Catch::Matchers::WithinAbs(defaults.outputGain, kRoundingTolerance));
    REQUIRE(read.limiterOn == defaults.limiterOn);
    REQUIRE(read.keyTrack == defaults.keyTrack);
    REQUIRE(read.rootNote == defaults.rootNote);
    REQUIRE(read.glideOn == defaults.glideOn);
    REQUIRE_THAT(read.glideMs, Catch::Matchers::WithinAbs(defaults.glideMs, 1.0e-3));
    REQUIRE_THAT(read.velocitySensitivity,
                 Catch::Matchers::WithinAbs(defaults.velocitySensitivity, kRoundingTolerance));
    REQUIRE_THAT(read.ampTimeScale, Catch::Matchers::WithinAbs(defaults.ampTimeScale, kRoundingTolerance));
    REQUIRE_THAT(read.pitchTimeScale,
                 Catch::Matchers::WithinAbs(defaults.pitchTimeScale, kRoundingTolerance));
    REQUIRE_THAT(read.pitchAmount, Catch::Matchers::WithinAbs(defaults.pitchAmount, kRoundingTolerance));
    REQUIRE_THAT(read.punchSplitMs, Catch::Matchers::WithinAbs(defaults.punchSplitMs, 1.0e-2));
    REQUIRE_THAT(read.punchLevel, Catch::Matchers::WithinAbs(defaults.punchLevel, kRoundingTolerance));
    REQUIRE_THAT(read.tailLevel, Catch::Matchers::WithinAbs(defaults.tailLevel, kRoundingTolerance));

    REQUIRE(read.tock.on == defaults.tock.on);
    REQUIRE(read.tock.sampleMode == defaults.tock.sampleMode);
    REQUIRE_THAT(read.tock.level, Catch::Matchers::WithinAbs(defaults.tock.level, kRoundingTolerance));
    REQUIRE_THAT(read.tock.tuneSemitones,
                 Catch::Matchers::WithinAbs(defaults.tock.tuneSemitones, kRoundingTolerance));
    REQUIRE_THAT(read.tock.startMs, Catch::Matchers::WithinAbs(defaults.tock.startMs, kRoundingTolerance));
    REQUIRE_THAT(read.tock.decayMs, Catch::Matchers::WithinAbs(defaults.tock.decayMs, 1.0e-2));
    REQUIRE_THAT(read.tock.tone, Catch::Matchers::WithinAbs(defaults.tock.tone, kRoundingTolerance));

    for (size_t i = 0; i < read.oscillators.size(); ++i)
    {
        INFO("oscillator " << i);
        const auto& a = read.oscillators[i];
        const auto& b = defaults.oscillators[i];
        REQUIRE(a.on == b.on);
        REQUIRE_THAT(a.level, Catch::Matchers::WithinAbs(b.level, kRoundingTolerance));
        REQUIRE(a.pitchSemitones == b.pitchSemitones);
        REQUIRE(a.octave == b.octave);
        REQUIRE_THAT(a.fineCents, Catch::Matchers::WithinAbs(b.fineCents, kRoundingTolerance));
        REQUIRE_THAT(a.position, Catch::Matchers::WithinAbs(b.position, kRoundingTolerance));
        REQUIRE_THAT(a.startPhase, Catch::Matchers::WithinAbs(b.startPhase, kRoundingTolerance));
    }
}

TEST_CASE("Parameter values are converted to DSP units", "[core][parameters]")
{
    namespace Ids = fcm::core::ParameterIds;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;
    fcm::core::EngineParameterReader reader(processor.state);

    setValue(processor.state, Ids::kOutputGain, -6.0f);
    setValue(processor.state, Ids::kLimiterOn, 0.0f);
    setValue(processor.state, Ids::kKeyTrack, 0.0f);
    setValue(processor.state, Ids::kRootNote, 45.0f);
    setValue(processor.state, Ids::kGlideOn, 1.0f);
    setValue(processor.state, Ids::kGlideTime, 250.0f);
    setValue(processor.state, Ids::kVelocitySensitivity, 40.0f);
    setValue(processor.state, Ids::kAmpEnvTimeScale, 2.0f);
    setValue(processor.state, Ids::kPitchEnvTimeScale, 0.5f);
    setValue(processor.state, Ids::kPitchEnvAmount, 36.0f);
    setValue(processor.state, Ids::kPunchTailSplit, 120.0f);
    setValue(processor.state, Ids::kPunchLevel, -6.0f);
    setValue(processor.state, Ids::kTailLevel, 3.0f);
    setValue(processor.state, Ids::kTockOn, 1.0f);
    setValue(processor.state, Ids::kTockMode, 1.0f);
    setValue(processor.state, Ids::kTockLevel, -6.0f);
    setValue(processor.state, Ids::kTockTune, 7.0f);
    setValue(processor.state, Ids::kTockStart, 4.5f);
    setValue(processor.state, Ids::kTockDecay, 60.0f);
    setValue(processor.state, Ids::kTockTone, 0.25f);

    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kOn), 1.0f);
    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kLevel), 0.25f);
    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kPitch), -7.0f);
    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kOctave), 2.0f);
    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kFine), 12.5f);
    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kPosition), 0.75f);
    setValue(processor.state, Ids::oscillator(1, Ids::Oscillator::kPhase), 90.0f);

    const auto read = reader.read();
    REQUIRE_THAT(read.outputGain, Catch::Matchers::WithinAbs(0.5012, 1.0e-3)); // -6 dB
    REQUIRE_FALSE(read.limiterOn);
    REQUIRE_FALSE(read.keyTrack);
    REQUIRE(read.rootNote == 45);
    REQUIRE(read.glideOn);
    REQUIRE_THAT(read.glideMs, Catch::Matchers::WithinAbs(250.0, 1.0));
    REQUIRE_THAT(read.velocitySensitivity, Catch::Matchers::WithinAbs(0.4, 1.0e-3));
    REQUIRE_THAT(read.ampTimeScale, Catch::Matchers::WithinAbs(2.0, 1.0e-2));
    REQUIRE_THAT(read.pitchTimeScale, Catch::Matchers::WithinAbs(0.5, 1.0e-2));
    REQUIRE_THAT(read.pitchAmount, Catch::Matchers::WithinAbs(36.0, 0.06));
    REQUIRE_THAT(read.punchSplitMs, Catch::Matchers::WithinAbs(120.0, 1.0));
    REQUIRE_THAT(read.punchLevel, Catch::Matchers::WithinAbs(0.5012, 1.0e-3)); // -6 dB
    REQUIRE_THAT(read.tailLevel, Catch::Matchers::WithinAbs(1.4125, 1.0e-3));  // +3 dB
    REQUIRE(read.tock.on);
    REQUIRE(read.tock.sampleMode);
    REQUIRE_THAT(read.tock.level, Catch::Matchers::WithinAbs(0.5012, 1.0e-3)); // -6 dB
    REQUIRE_THAT(read.tock.tuneSemitones, Catch::Matchers::WithinAbs(7.0, 1.0e-6));
    REQUIRE_THAT(read.tock.startMs, Catch::Matchers::WithinAbs(4.5, 1.0e-3));
    REQUIRE_THAT(read.tock.decayMs, Catch::Matchers::WithinAbs(60.0, 0.1));
    REQUIRE_THAT(read.tock.tone, Catch::Matchers::WithinAbs(0.25, 1.0e-3));

    // The first oscillator keeps its own settings: both are the same code with different IDs.
    REQUIRE(read.oscillators[0].on);
    REQUIRE(read.oscillators[0].pitchSemitones == 0);

    const auto& osc = read.oscillators[1];
    REQUIRE(osc.on);
    REQUIRE_THAT(osc.level, Catch::Matchers::WithinAbs(0.25, 1.0e-3));
    REQUIRE(osc.pitchSemitones == -7);
    REQUIRE(osc.octave == 2);
    REQUIRE_THAT(osc.fineCents, Catch::Matchers::WithinAbs(12.5, 0.06));
    REQUIRE_THAT(osc.position, Catch::Matchers::WithinAbs(0.75, 1.0e-3));
    REQUIRE_THAT(osc.startPhase, Catch::Matchers::WithinAbs(0.25, 1.0e-3)); // 90 degrees
}

TEST_CASE("Parameter ranges match the spec", "[core][parameters]")
{
    namespace Ids = fcm::core::ParameterIds;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;

    auto expectRange = [&](const std::string& id, float start, float end)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor.state.getParameter(id));
        REQUIRE(parameter != nullptr);
        INFO(id);
        REQUIRE(juce::exactlyEqual(parameter->getNormalisableRange().start, start));
        REQUIRE(juce::exactlyEqual(parameter->getNormalisableRange().end, end));
    };

    expectRange(Ids::kOutputGain, -48.0f, 12.0f);
    expectRange(Ids::kRootNote, 12.0f, 84.0f);
    expectRange(Ids::kGlideTime, 1.0f, 1000.0f);
    expectRange(Ids::kVelocitySensitivity, 0.0f, 100.0f);
    expectRange(Ids::kAmpEnvTimeScale, 0.25f, 4.0f);
    expectRange(Ids::kPitchEnvTimeScale, 0.25f, 4.0f);
    expectRange(Ids::kPitchEnvAmount, 0.0f, 96.0f);
    expectRange(Ids::kPunchTailSplit, 5.0f, 500.0f);
    expectRange(Ids::kPunchLevel, -48.0f, 12.0f);
    expectRange(Ids::kTailLevel, -48.0f, 12.0f);
    expectRange(Ids::kTockLevel, -48.0f, 12.0f);
    expectRange(Ids::kTockTune, -24.0f, 24.0f);
    expectRange(Ids::kTockStart, 0.0f, 20.0f);
    expectRange(Ids::kTockDecay, 2.0f, 200.0f);
    expectRange(Ids::kTockTone, 0.0f, 1.0f);

    for (int index = 0; index < 2; ++index)
    {
        namespace Osc = Ids::Oscillator;
        expectRange(Ids::oscillator(index, Osc::kLevel), 0.0f, 1.0f);
        expectRange(Ids::oscillator(index, Osc::kPitch), -24.0f, 24.0f);
        expectRange(Ids::oscillator(index, Osc::kOctave), -4.0f, 4.0f);
        expectRange(Ids::oscillator(index, Osc::kFine), -100.0f, 100.0f);
        expectRange(Ids::oscillator(index, Osc::kPosition), 0.0f, 1.0f);
        expectRange(Ids::oscillator(index, Osc::kPhase), 0.0f, 360.0f);
    }

    // Root note is shown as a note name (JUCE convention: C3 is MIDI 60) and accepts one back.
    auto* rootNote = processor.state.getParameter(Ids::kRootNote);
    REQUIRE(rootNote->getText(rootNote->convertTo0to1(36.0f), 0) == "C1");
    REQUIRE_THAT(rootNote->convertFrom0to1(rootNote->getValueForText("C1")),
                 Catch::Matchers::WithinAbs(36.0, 0.01));
    REQUIRE_THAT(rootNote->convertFrom0to1(rootNote->getValueForText("45")),
                 Catch::Matchers::WithinAbs(45.0, 0.01));
}

TEST_CASE("Default patch plays a kick through the reader and the engine", "[core][parameters][engine]")
{
    namespace Ids = fcm::core::ParameterIds;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;
    fcm::core::EngineParameterReader reader(processor.state);

    auto playNote = [&]
    {
        fcm::dsp::Engine engine;
        engine.setParameters(reader.read());
        engine.prepare({48000.0, 512, 2});

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);

        float peak = 0.0f;
        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            engine.setParameters(reader.read()); // as the processor does at the start of every block
            engine.process(buffer, block == 0 ? midi : juce::MidiBuffer{});
            peak = std::max(peak, buffer.getMagnitude(0, 512));
        }
        return peak;
    };

    const float peak = playNote();
    REQUIRE(peak > 0.5f);
    REQUIRE(peak <= 1.0f);

    // Turning the only active oscillator off silences the plugin: the controls are wired to audio.
    setValue(processor.state, Ids::oscillator(0, Ids::Oscillator::kOn), 0.0f);
    REQUIRE(juce::exactlyEqual(playNote(), 0.0f));
}
