// SPDX-License-Identifier: AGPL-3.0-only
//
// Backward-compatibility guard for version 0.2 (docs/specs/mixer-and-buses.md §5, docs/PLAN.md).
// The files in tests/data/reference-0.1.1 were rendered with the released 0.1.1 code: every factory
// preset and two test projects with the Tock on, one note each. Every later change must reproduce
// them within 1e-6 per sample, after aligning by the reported latency.
//
// To make the references again (only ever from the 0.1.1 code), run this executable with the
// environment variable FCM_WRITE_REFERENCES set to 1.
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/FactoryPresets.h"
#include "core/ParameterIds.h"
#include "core/ProjectState.h"
#include "dsp/engine/Engine.h"
#include "dsp/envelope/EnvelopePresets.h"
#include "dsp/tock/FactoryTocks.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#ifndef FCM_REFERENCE_DIR
#error "FCM_REFERENCE_DIR must point at tests/data/reference-0.1.1"
#endif

namespace
{
const juce::Identifier kRoot{"FrenchcoreMachineState"};

constexpr double kSampleRate = 44100.0;
constexpr int kBlock = 512;
constexpr int kNote = 36;
constexpr int kVelocity = 100;
constexpr int kNoteOffAt = 6615;        // 0.15 s
constexpr int kReferenceLength = 13230; // 0.30 s
// The references were rendered on Linux, so there the 1e-6 of docs/specs/mixer-and-buses.md section 5
// holds as written. Other platforms use other maths libraries and instruction sets (the oversampled
// distortion and the oscillator differ by up to about 3e-5, -90 dBFS, on Windows), so they are held to
// 1e-4 (-80 dBFS), far below anything audible and still tight enough to catch any real change.
#if JUCE_LINUX
constexpr float kTolerance = 1.0e-6f;
#else
constexpr float kTolerance = 1.0e-4f;
#endif

// A project made of a factory preset plus parameter overrides (the "test projects").
struct Case
{
    std::string file;                                  // reference file name without extension
    std::string presetName;                            // factory preset the state starts from
    std::vector<std::pair<std::string, double>> extra; // parameters set on top
};

std::vector<Case> allCases()
{
    std::vector<Case> cases;
    for (const char* name : {"Init", "Classic Punch", "Rumble Kick", "Hardcore Crunch", "Gabber Fold"})
    {
        std::string file = name;
        for (auto& c : file)
            c = c == ' ' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        cases.push_back({file, name, {}});
    }

    double click = 0;
    for (int i = 0; i < fcm::dsp::FactoryTocks::count(); ++i)
        if (std::strcmp(fcm::dsp::FactoryTocks::name(i), "Click") == 0)
            click = i;

    // Tock on, sample mode, no other effect: the tock layer alone on top of the oscillator.
    cases.push_back({"tock-sample",
                     "Init",
                     {{"tock.on", 1}, {"tock.mode", 1}, {"tock.sample", click}, {"tock.ampEnv", 1}}});
    // Tock on, synth mode, through the distortion.
    cases.push_back({"tock-synth",
                     "Init",
                     {{"tock.on", 1},
                      {"tock.mode", 0},
                      {"tock.level", -6},
                      {"tock.ampEnv", 1},
                      {"distortion.enabled", 1},
                      {"distortion.drive", 35}}});
    return cases;
}

int presetIndex(const std::string& name)
{
    for (int i = 0; i < fcm::core::FactoryPresets::count(); ++i)
        if (fcm::core::FactoryPresets::name(i).toStdString() == name)
            return i;
    return -1;
}

// The pieces of the plugin that a render needs, wired the way PluginProcessor wires them.
struct Rig
{
    Rig() : project(parts())
    {
        engine.setEnvelopeSources(&amp.source(), &pitch.source());
        engine.setWavetableSources(&wavetable1.source(), &wavetable2.source());
        engine.setTockSource(&tock.source());
        engine.setNoiseSource(&noise.source());
    }

    fcm::core::ProjectState::Parts parts()
    {
        return {processor.state, kRoot, amp, pitch, fx, {&wavetable1, &wavetable2}, tock, noise};
    }

    void load(const Case& testCase)
    {
        const int index = presetIndex(testCase.presetName);
        REQUIRE(index >= 0);
        auto tree = fcm::core::FactoryPresets::makeState(index, kRoot);
        for (const auto& [id, value] : testCase.extra)
        {
            bool found = false;
            for (auto child : tree)
                if (child.getType().toString() == "PARAM" && child["id"].toString().toStdString() == id)
                {
                    child.setProperty("value", value, nullptr);
                    found = true;
                }
            if (!found)
            {
                juce::ValueTree parameter("PARAM");
                parameter.setProperty("id", juce::String(id), nullptr);
                parameter.setProperty("value", value, nullptr);
                tree.appendChild(parameter, nullptr);
            }
        }
        REQUIRE(project.restore(tree) != fcm::core::StateLoadResult::rejected);
    }

    // Left channel of one note, `length` samples long.
    std::vector<float> render(int length, int& latency)
    {
        engine.setParameters(reader.read());
        engine.prepare({kSampleRate, static_cast<juce::uint32>(kBlock), 2});
        latency = engine.latencySamples(reader.read().oversamplingFactor);

        std::vector<float> out;
        out.reserve(static_cast<size_t>(length));
        juce::AudioBuffer<float> buffer(2, kBlock);
        juce::MidiBuffer midi;
        for (int start = 0; start < length; start += kBlock)
        {
            const int count = std::min(kBlock, length - start);
            buffer.clear();
            midi.clear();
            if (start == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, kNote, static_cast<juce::uint8>(kVelocity)), 0);
            if (kNoteOffAt >= start && kNoteOffAt < start + count)
                midi.addEvent(juce::MidiMessage::noteOff(1, kNote), kNoteOffAt - start);

            juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(), 2, count);
            engine.setParameters(reader.read());
            engine.process(view, midi);
            for (int i = 0; i < count; ++i)
                out.push_back(buffer.getSample(0, i));
        }
        return out;
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    fcm::dsp::Engine engine;
    fcm::core::EngineParameterReader reader{processor.state};
    fcm::core::EnvelopeState amp{fcm::dsp::EnvelopeKind::amp};
    fcm::core::EnvelopeState pitch{fcm::dsp::EnvelopeKind::pitch};
    fcm::core::FxRackState fx{processor.state,
                              fcm::dsp::defaultEffectRegistry(),
                              {&engine.rackValue(fcm::dsp::RackId::busA),
                               &engine.rackValue(fcm::dsp::RackId::busB),
                               &engine.rackValue(fcm::dsp::RackId::main)}};
    fcm::core::WavetableSelector wavetable1{processor.state, 0};
    fcm::core::WavetableSelector wavetable2{processor.state, 1};
    fcm::core::TockSelector tock{processor.state};
    fcm::core::NoiseSelector noise{processor.state};
    fcm::core::ProjectState project;
};

// File format: int32 latency in samples, int32 sample count, then float32 samples (little endian).
juce::File referenceFile(const std::string& name)
{
    return juce::File(FCM_REFERENCE_DIR).getChildFile(juce::String(name) + ".f32");
}

void writeReference(const std::string& name, int latency, const std::vector<float>& samples)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream out(block, false);
    out.writeInt(latency);
    out.writeInt(static_cast<int>(samples.size()));
    for (float sample : samples)
        out.writeFloat(sample);
    out.flush();
    REQUIRE(referenceFile(name).replaceWithData(block.getData(), block.getSize()));
}

bool readReference(const std::string& name, int& latency, std::vector<float>& samples)
{
    juce::MemoryBlock block;
    if (!referenceFile(name).loadFileAsData(block))
        return false;
    juce::MemoryInputStream in(block, false);
    latency = in.readInt();
    const int count = in.readInt();
    if (count <= 0 || static_cast<size_t>(count) * 4 + 8 != block.getSize())
        return false;
    samples.resize(static_cast<size_t>(count));
    for (auto& sample : samples)
        sample = in.readFloat();
    return true;
}
} // namespace

TEST_CASE("References made with 0.1.1 still render the same", "[reference][compat]")
{
    const bool writing = std::getenv("FCM_WRITE_REFERENCES") != nullptr;

    for (const auto& testCase : allCases())
    {
        DYNAMIC_SECTION(testCase.file)
        {
            Rig rig;
            rig.load(testCase);

            if (writing)
            {
                int latency = 0;
                const auto samples = rig.render(kReferenceLength, latency);
                writeReference(testCase.file, latency, samples);
                continue;
            }

            int referenceLatency = 0;
            std::vector<float> reference;
            REQUIRE(readReference(testCase.file, referenceLatency, reference));

            // A later version may report more latency (a constant, the same whatever is on); the
            // sound arrives that much later, so the render is longer by the difference.
            int latency = 0;
            const int extra = 4096; // room for any growth of the reported latency
            const auto rendered = rig.render(static_cast<int>(reference.size()) + extra, latency);
            const int shift = latency - referenceLatency;
            REQUIRE(shift >= 0);
            REQUIRE(shift <= extra);

            float worst = 0.0f;
            int worstAt = 0;
            for (size_t i = 0; i < reference.size(); ++i)
            {
                const float difference = std::abs(rendered[i + static_cast<size_t>(shift)] - reference[i]);
                if (!std::isfinite(difference) || difference > worst)
                {
                    worst = std::isfinite(difference) ? difference : std::numeric_limits<float>::infinity();
                    worstAt = static_cast<int>(i);
                }
            }
            INFO("largest difference " << worst << " at sample " << worstAt);
            REQUIRE(worst <= kTolerance);
        }
    }
}
