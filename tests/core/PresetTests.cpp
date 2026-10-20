// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/FactoryPresets.h"
#include "core/ParameterIds.h"
#include "core/PresetManager.h"
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <set>

using Catch::Matchers::WithinAbs;
using fcm::core::FactoryPresets;
using fcm::core::PresetLibrary;
using fcm::core::PresetManager;
using fcm::core::ProjectState;
using fcm::core::sanitizePresetName;
using fcm::dsp::EnvelopeKind;

namespace
{
const juce::Identifier kRoot{"FrenchcoreMachineState"};

struct Rig
{
    explicit Rig(const juce::File& folder)
        : project(ProjectState::Parts{
              processor.state, kRoot, amp, pitch, fx, {&wavetable1, &wavetable2}, tock, noise, curves}),
          presets(project, PresetLibrary(folder), kRoot)
    {
    }

    void setParameter(const std::string& id, float value)
    {
        auto* parameter = processor.state.getParameter(id);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    float getParameter(const std::string& id)
    {
        auto* parameter = processor.state.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValue());
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    fcm::dsp::Engine engine;
    fcm::core::EnvelopeState amp{EnvelopeKind::amp};
    fcm::core::EnvelopeState pitch{EnvelopeKind::pitch};
    fcm::core::FxRackState fx{processor.state,
                              fcm::dsp::defaultEffectRegistry(),
                              {&engine.rackValue(fcm::dsp::RackId::busA),
                               &engine.rackValue(fcm::dsp::RackId::busB),
                               &engine.rackValue(fcm::dsp::RackId::main)}};
    fcm::core::WavetableSelector wavetable1{processor.state, 0};
    fcm::core::WavetableSelector wavetable2{processor.state, 1};
    fcm::core::TockSelector tock{processor.state};
    fcm::core::NoiseSelector noise{processor.state};
    fcm::core::EffectCurveState curves{fcm::dsp::defaultEffectRegistry()};
    ProjectState project;
    PresetManager presets;
};

// A folder that is removed when the test ends.
struct TempFolder
{
    TempFolder()
        : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("fcm-preset-test-" +
                                      juce::String(juce::Random::getSystemRandom().nextInt64())))
    {
        directory.createDirectory();
    }
    ~TempFolder() { directory.deleteRecursively(); }
    juce::File directory;
    juce::File presets() const { return directory.getChildFile("Presets"); }
};

juce::ValueTree sampleState(Rig& rig)
{
    rig.setParameter("distortion.drive", 77.0f);
    rig.setParameter("osc1.wavetable", 3.0f);
    return rig.project.capture();
}
} // namespace

TEST_CASE("Preset names are reduced to a safe file name", "[core][preset]")
{
    REQUIRE(sanitizePresetName("My Kick") == "My Kick");
    REQUIRE(sanitizePresetName("  My   Kick  ") == "My Kick");
    REQUIRE(sanitizePresetName("../../etc/passwd") == "etcpasswd");
    REQUIRE(sanitizePresetName("a/b\\c:d*e?f\"g<h>i|j") == "abcdefghij");
    REQUIRE(sanitizePresetName("..") == "");
    REQUIRE(sanitizePresetName("...hidden...") == "hidden");
    REQUIRE(sanitizePresetName(juce::String("tab\there\nnew\x01")) == "tabherenew");
    REQUIRE(sanitizePresetName(juce::CharPointer_UTF8("k\xc3\xa1ick \xe2\x82\xac")) == "kick");
    REQUIRE(sanitizePresetName("") == "");
    REQUIRE(sanitizePresetName("!!!") == "");
    REQUIRE(sanitizePresetName("Kick (v2)_final-1.2") == "Kick (v2)_final-1.2");
    REQUIRE(sanitizePresetName(juce::String::repeatedString("a", 500)).length() == 64);

    for (const char* reserved : {"CON", "nul", "Aux", "PRN", "COM1", "lpt9", "con.txt"})
        REQUIRE(sanitizePresetName(reserved) != juce::String(reserved));
    REQUIRE(sanitizePresetName("COM0") == "COM0");
    REQUIRE(sanitizePresetName("Console") == "Console");

    // Idempotent.
    for (const char* text : {"My Kick", "  a  b ", "CON", "x.y."})
    {
        const auto once = sanitizePresetName(text);
        REQUIRE(sanitizePresetName(once) == once);
    }
}

TEST_CASE("A preset round-trips through its text", "[core][preset]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    const auto state = sampleState(rig);

    const auto text = fcm::core::encodePreset("Round Trip", state);
    const auto parsed = fcm::core::parsePreset(text);
    REQUIRE(parsed.ok);
    REQUIRE(parsed.name == "Round Trip");
    REQUIRE(parsed.state.isEquivalentTo(juce::ValueTree::fromXml(*state.createXml())));

    // A second round trip gives the same text.
    REQUIRE(fcm::core::encodePreset(parsed.name, parsed.state) == text);
}

TEST_CASE("Unusable preset files are refused with a message", "[core][preset][untrusted]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    const auto goodState = rig.project.capture();
    const auto good = fcm::core::encodePreset("Good", goodState);

    const auto refusedWith = [](const juce::String& text)
    {
        const auto result = fcm::core::parsePreset(text);
        INFO(text.substring(0, 80));
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.isNotEmpty());
    };

    refusedWith("");
    refusedWith("not xml at all \x01\x02\x03");
    refusedWith(good.substring(0, good.length() / 2)); // truncated
    refusedWith("<Other presetFormat=\"1\"><FrenchcoreMachineState/></Other>");
    refusedWith("<FrenchcorePreset name=\"x\"><FrenchcoreMachineState/></FrenchcorePreset>"); // no format
    refusedWith(
        "<FrenchcorePreset presetFormat=\"0\" name=\"x\"><FrenchcoreMachineState/></FrenchcorePreset>");
    refusedWith(
        "<FrenchcorePreset presetFormat=\"2\" name=\"x\"><FrenchcoreMachineState/></FrenchcorePreset>");
    refusedWith("<FrenchcorePreset presetFormat=\"1\" name=\"x\"/>"); // no state

    // Nested far deeper than any preset: refused before the parser can run out of stack.
    const juce::String deep =
        "<FrenchcorePreset presetFormat=\"1\" name=\"x\">" + juce::String::repeatedString("<a>", 200000);
    refusedWith(deep);

    // Quotes and comments cannot hide the nesting.
    const juce::String sneaky = "<FrenchcorePreset presetFormat=\"1\" name=\"x\">" +
                                juce::String::repeatedString("<a x=\"</</\"><!-- </a> -->", 200);
    refusedWith(sneaky);

    // A DTD or an entity is never written by the plugin.
    refusedWith("<!DOCTYPE x [<!ENTITY a \"b\">]><FrenchcorePreset presetFormat=\"1\" name=\"&a;\">"
                "<FrenchcoreMachineState/></FrenchcorePreset>");
    refusedWith("<FrenchcorePreset presetFormat=\"1\" name=\"x\"><!ENTITY a \"b\">"
                "<FrenchcoreMachineState/></FrenchcorePreset>");

    // Too big.
    const juce::String huge =
        "<FrenchcorePreset presetFormat=\"1\" name=\"" +
        juce::String::repeatedString("a", static_cast<int>(fcm::core::kMaxPresetBytes) + 10) + "\"/>";
    refusedWith(huge);

    // An unreadable name is replaced, not a reason to refuse.
    const auto unnamed = fcm::core::parsePreset(
        "<FrenchcorePreset presetFormat=\"1\" name=\"../..\"><FrenchcoreMachineState/></FrenchcorePreset>");
    REQUIRE(unnamed.ok);
    REQUIRE(unnamed.name == "Untitled");
}

TEST_CASE("A preset with bad numbers loads clamped and a wrong state changes nothing",
          "[core][preset][untrusted]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    rig.setParameter("distortion.drive", 33.0f);
    const auto before = rig.project.capture();

    // Values far outside their range, NaN and text.
    const auto hostile =
        fcm::core::parsePreset("<FrenchcorePreset presetFormat=\"1\" name=\"h\"><FrenchcoreMachineState>"
                               "<PARAM id=\"distortion.drive\" value=\"1e308\"/>"
                               "<PARAM id=\"global.outputGain\" value=\"nan\"/>"
                               "<PARAM id=\"osc1.level\" value=\"-5\"/>"
                               "</FrenchcoreMachineState></FrenchcorePreset>");
    REQUIRE(hostile.ok);
    REQUIRE(rig.project.restore(hostile.state) != fcm::core::StateLoadResult::rejected);
    REQUIRE_THAT(rig.getParameter("distortion.drive"), WithinAbs(100.0, 1.0e-3));
    REQUIRE_THAT(rig.getParameter("osc1.level"), WithinAbs(0.0, 1.0e-6));
    REQUIRE(std::isfinite(rig.getParameter("global.outputGain")));

    // A state of another type is rejected, and nothing changes.
    rig.setParameter("distortion.drive", 33.0f);
    const auto wrong = fcm::core::parsePreset(
        "<FrenchcorePreset presetFormat=\"1\" name=\"w\"><SomethingElse/></FrenchcorePreset>");
    REQUIRE(wrong.ok);
    REQUIRE(rig.project.restore(wrong.state) == fcm::core::StateLoadResult::rejected);
    REQUIRE_THAT(rig.getParameter("distortion.drive"), WithinAbs(33.0, 1.0e-3));
}

TEST_CASE("A preset with embedded audio of a false size loads without user audio",
          "[core][preset][untrusted][security]")
{
    TempFolder folder;
    Rig source(folder.presets());
    REQUIRE(source.wavetable1.loadUser(std::vector<float>(2048, 0.25f), "Mine"));
    fcm::dsp::TockSample sample;
    sample.sampleRate = 48000.0;
    sample.samples.assign(400, 0.5f);
    REQUIRE(source.tock.loadUser(sample, "my tock"));
    source.setParameter("distortion.drive", 77.0f);
    auto state = source.project.capture();
    state.getChildWithName("UserWavetable1").setProperty("frames", "-1.A", nullptr);
    state.getChildWithName("UserTockSample").setProperty("samples", "2000000000.A", nullptr);

    const auto parsed = fcm::core::parsePreset(fcm::core::encodePreset("Liar", state));
    REQUIRE(parsed.ok);

    Rig target(folder.presets());
    REQUIRE(target.wavetable1.loadUser(std::vector<float>(2048, 0.25f), "Before"));
    REQUIRE(target.project.restore(parsed.state) == fcm::core::StateLoadResult::loaded);
    REQUIRE_FALSE(target.wavetable1.hasUser());
    REQUIRE_FALSE(target.tock.hasUser());
    REQUIRE_THAT(target.getParameter("distortion.drive"), WithinAbs(77.0, 1.0e-3)); // the rest loaded
}

TEST_CASE("The library writes only inside its folder, atomically, and lists safely",
          "[core][preset][library]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    const auto state = sampleState(rig);
    PresetLibrary library(folder.presets());

    REQUIRE(library.list().empty()); // the folder does not exist yet

    const auto saved = library.save("My Kick", state);
    REQUIRE(saved.ok);
    REQUIRE(saved.file == folder.presets().getChildFile("My Kick.fcmpreset"));
    REQUIRE(saved.file.existsAsFile());

    // Names that try to leave the folder end up inside it.
    const auto sneaky = library.save("../../escape", state);
    REQUIRE(sneaky.ok);
    REQUIRE(sneaky.file.getParentDirectory() == folder.presets());
    REQUIRE_FALSE(folder.directory.getChildFile("escape.fcmpreset").exists());
    REQUIRE_FALSE(folder.directory.getParentDirectory().getChildFile("escape.fcmpreset").exists());

    // Unusable names and states are refused without writing.
    REQUIRE_FALSE(library.save("///", state).ok);
    REQUIRE_FALSE(library.save("", state).ok);
    REQUIRE_FALSE(library.save("x", juce::ValueTree()).ok);

    // Overwriting replaces; no temporary file stays behind.
    REQUIRE(library.exists("My Kick"));
    REQUIRE(library.save("My Kick", state).ok);
    int files = 0;
    for (const auto& entry : folder.presets().findChildFiles(juce::File::findFilesAndDirectories, true, "*"))
    {
        REQUIRE(entry.hasFileExtension(".fcmpreset"));
        ++files;
    }
    REQUIRE(files == 2);

    // Listing: only .fcmpreset regular files, not sub-folders, sorted without regard to case.
    folder.presets().getChildFile("notes.txt").replaceWithText("hi");
    folder.presets().getChildFile("folder.fcmpreset").createDirectory();
    folder.presets().getChildFile("apple.fcmpreset").replaceWithText("x");
    const auto listed = library.list();
    REQUIRE(listed.size() == 3);
    REQUIRE(listed[0].getFileName() == "apple.fcmpreset");
    REQUIRE(listed[1].getFileName() == "escape.fcmpreset");
    REQUIRE(listed[2].getFileName() == "My Kick.fcmpreset");

    // What was saved loads back to the same state.
    const auto loaded = PresetLibrary::load(saved.file);
    REQUIRE(loaded.ok);
    REQUIRE(loaded.name == "My Kick");
    REQUIRE(loaded.state.isEquivalentTo(juce::ValueTree::fromXml(*state.createXml())));

    // Damaged and missing files are refused.
    REQUIRE_FALSE(PresetLibrary::load(folder.presets().getChildFile("apple.fcmpreset")).ok);
    REQUIRE_FALSE(PresetLibrary::load(folder.presets().getChildFile("missing.fcmpreset")).ok);
}

TEST_CASE("The listing is bounded", "[core][preset][library]")
{
    TempFolder folder;
    const auto presets = folder.presets();
    presets.createDirectory();
    for (int i = 0; i < PresetLibrary::kMaxListed + 25; ++i)
        presets.getChildFile("p" + juce::String(i).paddedLeft('0', 5) + ".fcmpreset").replaceWithText("x");
    REQUIRE(PresetLibrary(presets).list().size() == static_cast<size_t>(PresetLibrary::kMaxListed));
}

TEST_CASE("Every factory preset is valid and loads", "[core][preset][factory]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    REQUIRE(FactoryPresets::count() >= 5);
    REQUIRE(FactoryPresets::name(0) == "Init");

    std::set<juce::String> names;
    for (int i = 0; i < FactoryPresets::count(); ++i)
    {
        const auto name = FactoryPresets::name(i);
        INFO(name);
        REQUIRE(names.insert(name).second);
        REQUIRE(sanitizePresetName(name) == name);

        const auto tree = FactoryPresets::makeState(i, kRoot);
        REQUIRE(tree.isValid());
        for (const auto& child : tree)
        {
            if (child.getType() != juce::Identifier("PARAM"))
                continue;
            // Every id exists and the value is inside the range (it would be clamped silently otherwise).
            const auto id = child["id"].toString();
            INFO(id);
            auto* parameter = rig.processor.state.getParameter(id);
            REQUIRE(parameter != nullptr);
            const auto& range = parameter->getNormalisableRange();
            const auto value = static_cast<float>(static_cast<double>(child["value"]));
            REQUIRE(value >= range.start);
            REQUIRE(value <= range.end);
        }
        // Both envelope shapes come with it and are valid.
        REQUIRE(tree.getChildWithName(fcm::core::EnvelopeState::nodeType(EnvelopeKind::amp)).isValid());
        REQUIRE(tree.getChildWithName(fcm::core::EnvelopeState::nodeType(EnvelopeKind::pitch)).isValid());

        REQUIRE(rig.presets.load(i));
        REQUIRE(rig.presets.currentName() == name);
        const auto once = rig.project.capture();
        REQUIRE(rig.presets.load(i));
        REQUIRE(rig.project.capture().isEquivalentTo(once));
    }
    REQUIRE_FALSE(FactoryPresets::makeState(-1, kRoot).isValid());
    REQUIRE_FALSE(FactoryPresets::makeState(FactoryPresets::count(), kRoot).isValid());
    REQUIRE(FactoryPresets::name(FactoryPresets::count()).isEmpty());
}

TEST_CASE("A factory preset sets what it names and everything else to its default", "[core][preset][factory]")
{
    TempFolder folder;
    Rig rig(folder.presets());

    // Things a preset never mentions, changed first.
    rig.setParameter("global.outputGain", -9.0f);
    rig.setParameter("osc2.on", 1.0f);
    rig.setParameter("stereo.lowwidth", 20.0f);

    REQUIRE(rig.presets.load(1)); // Classic Punch
    REQUIRE(rig.getParameter("distortion.enabled") > 0.5f);
    REQUIRE(rig.getParameter("distortion.drive") == 40.0f);
    REQUIRE_THAT(rig.getParameter("global.outputGain"), WithinAbs(0.0, 1.0e-3));
    REQUIRE(rig.getParameter("osc2.on") < 0.5f);
    REQUIRE_THAT(rig.getParameter("stereo.lowwidth"), WithinAbs(100.0, 1.0e-3));

    // Init brings back the plain sound.
    REQUIRE(rig.presets.load(0));
    REQUIRE(rig.getParameter("distortion.enabled") < 0.5f);
}

TEST_CASE("The manager steps through factory and user presets and keeps the sound on a failed load",
          "[core][preset][manager]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    const int numFactory = rig.presets.numFactory();
    REQUIRE(rig.presets.currentName() == "Init");
    REQUIRE(rig.presets.currentIndex() == 0);

    // Saving makes a user preset, listed after the factory ones, and the current one.
    rig.setParameter("distortion.drive", 61.0f);
    REQUIRE(rig.presets.wouldReplace("Mine") == false);
    REQUIRE(rig.presets.save("Mine"));
    REQUIRE(rig.presets.currentName() == "Mine");
    REQUIRE(rig.presets.entries().size() == static_cast<size_t>(numFactory + 1));
    REQUIRE_FALSE(rig.presets.entries().back().factory);
    REQUIRE(rig.presets.currentIndex() == numFactory);
    REQUIRE(rig.presets.wouldReplace("Mine"));

    // Next wraps from the last entry to the first, previous goes back.
    REQUIRE(rig.presets.next());
    REQUIRE(rig.presets.currentIndex() == 0);
    REQUIRE(rig.presets.previous());
    REQUIRE(rig.presets.currentIndex() == numFactory);
    REQUIRE_THAT(rig.getParameter("distortion.drive"), WithinAbs(61.0, 1.0e-3));

    // Loading a user preset by index restores its sound.
    rig.setParameter("distortion.drive", 5.0f);
    REQUIRE(rig.presets.load(numFactory));
    REQUIRE_THAT(rig.getParameter("distortion.drive"), WithinAbs(61.0, 1.0e-3));

    // A damaged file is refused: sound and name stay.
    folder.presets().getChildFile("Broken.fcmpreset").replaceWithText("<nonsense");
    rig.presets.refresh();
    int broken = -1;
    for (size_t i = 0; i < rig.presets.entries().size(); ++i)
        if (rig.presets.entries()[i].name == "Broken")
            broken = static_cast<int>(i);
    REQUIRE(broken >= 0);
    rig.setParameter("distortion.drive", 12.0f);
    const auto nameBefore = rig.presets.currentName();
    REQUIRE_FALSE(rig.presets.load(broken));
    REQUIRE(rig.presets.lastMessage().isNotEmpty());
    REQUIRE(rig.presets.currentName() == nameBefore);
    REQUIRE_THAT(rig.getParameter("distortion.drive"), WithinAbs(12.0, 1.0e-3));

    // Out-of-range indices and unusable names change nothing.
    REQUIRE_FALSE(rig.presets.load(-1));
    REQUIRE_FALSE(rig.presets.load(9999));
    REQUIRE_FALSE(rig.presets.save("///"));
    REQUIRE(rig.presets.currentName() == nameBefore);
}

TEST_CASE("Every factory preset plays a finite, audible, limited kick", "[core][preset][factory]")
{
    TempFolder folder;
    Rig rig(folder.presets());
    rig.engine.setEnvelopeSources(&rig.amp.source(), &rig.pitch.source());
    rig.engine.setWavetableSources(&rig.wavetable1.source(), &rig.wavetable2.source());
    rig.engine.setTockSource(&rig.tock.source());
    rig.engine.setNoiseSource(&rig.noise.source());
    fcm::core::EngineParameterReader reader(rig.processor.state);

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 512;
    for (int preset = 0; preset < FactoryPresets::count(); ++preset)
    {
        INFO(FactoryPresets::name(preset));
        REQUIRE(rig.presets.load(preset));

        rig.engine.setParameters(reader.read());
        rig.engine.prepare({kSampleRate, kBlock, 2});

        juce::AudioBuffer<float> buffer(2, kBlock);
        float peak = 0.0f;
        bool finite = true;
        for (int block = 0; block < 100; ++block) // about one second
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
            if (block == 60)
                midi.addEvent(juce::MidiMessage::noteOff(1, 36), 0);
            rig.engine.setParameters(reader.read());
            rig.engine.process(buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < kBlock; ++i)
                {
                    const float sample = buffer.getSample(channel, i);
                    finite = finite && std::isfinite(sample);
                    peak = std::max(peak, std::abs(sample));
                }
        }
        REQUIRE(finite);
        REQUIRE(peak > 0.05f);
        REQUIRE(peak <= 1.0f);
    }
}
