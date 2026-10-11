// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/WavetableImport.h"
#include "core/WavetableLoader.h"
#include "core/WavetableSelector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using namespace fcm::core;

namespace
{
constexpr int kFrame = 2048;

// A minimal WAV file written by hand, so the tests control every byte (including lying headers).
struct WavSpec
{
    int channels = 1;
    bool isFloat = true;
    uint32_t declaredDataBytes = 0xffffffff; // 0xffffffff: the real size
    bool truncateBy = false;
};

void put32(std::vector<uint8_t>& out, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

void put16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

void putTag(std::vector<uint8_t>& out, const char* tag)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(tag[i]));
}

// `samples` are interleaved if channels > 1.
std::vector<uint8_t> makeWav(const std::vector<float>& samples, const WavSpec& spec = {})
{
    const int bytesPerSample = spec.isFloat ? 4 : 2;
    const auto realBytes = static_cast<uint32_t>(samples.size() * static_cast<size_t>(bytesPerSample));
    const uint32_t dataBytes = spec.declaredDataBytes == 0xffffffff ? realBytes : spec.declaredDataBytes;

    std::vector<uint8_t> out;
    putTag(out, "RIFF");
    put32(out, 36 + dataBytes);
    putTag(out, "WAVE");
    putTag(out, "fmt ");
    put32(out, 16);
    put16(out, spec.isFloat ? 3 : 1);
    put16(out, static_cast<uint16_t>(spec.channels));
    put32(out, 48000);
    put32(out, 48000u * static_cast<uint32_t>(spec.channels * bytesPerSample));
    put16(out, static_cast<uint16_t>(spec.channels * bytesPerSample));
    put16(out, static_cast<uint16_t>(bytesPerSample * 8));
    putTag(out, "data");
    put32(out, dataBytes);

    for (float s : samples)
    {
        if (spec.isFloat)
        {
            uint32_t bits;
            std::memcpy(&bits, &s, 4);
            put32(out, bits);
        }
        else
        {
            put16(out, static_cast<uint16_t>(
                           static_cast<int16_t>(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767.0f))));
        }
    }
    if (spec.truncateBy)
        out.resize(out.size() / 2);
    return out;
}

std::vector<float> sineCycle(int length, int cycles = 1)
{
    std::vector<float> out(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i)
        out[static_cast<size_t>(i)] =
            static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * cycles * i / length));
    return out;
}

WavetableImportResult import(const std::vector<uint8_t>& bytes)
{
    return importWavetable(bytes.data(), bytes.size());
}
} // namespace

TEST_CASE("Whole frames of float and 16-bit WAVs are imported", "[core][wavetable][import]")
{
    for (const int frames : {1, 2, 256})
        for (const bool isFloat : {true, false})
        {
            std::vector<float> samples;
            for (int f = 0; f < frames; ++f)
            {
                auto frame = sineCycle(kFrame, 1 + f % 3);
                samples.insert(samples.end(), frame.begin(), frame.end());
            }
            WavSpec spec;
            spec.isFloat = isFloat;
            const auto result = import(makeWav(samples, spec));

            INFO(frames << " frames, float " << isFloat);
            REQUIRE(result.ok());
            REQUIRE(result.frames.size() == static_cast<size_t>(frames) * kFrame);
            REQUIRE(isValidStoredWavetable(result.frames));

            float peak = 0.0f;
            for (float s : result.frames)
                peak = std::max(peak, std::abs(s));
            REQUIRE_THAT(peak, WithinAbs(1.0, 1.0e-5));
        }
}

TEST_CASE("A single cycle of any length from 64 to 16384 becomes one frame", "[core][wavetable][import]")
{
    for (const int length : {64, 600, 2047, 4096 + 1, 16384})
    {
        const auto result = import(makeWav(sineCycle(length)));
        INFO("length " << length);
        if (length % kFrame == 0)
            continue; // a whole number of frames is not a single cycle
        REQUIRE(result.ok());
        REQUIRE(result.frames.size() == static_cast<size_t>(kFrame));

        // It is still one clean sine cycle after the stretch.
        for (int i = 0; i < kFrame; i += 97)
            REQUIRE_THAT(result.frames[static_cast<size_t>(i)],
                         WithinAbs(std::sin(2.0 * std::numbers::pi * i / kFrame), 0.02));
    }
}

TEST_CASE("Only the first channel is used", "[core][wavetable][import]")
{
    const auto left = sineCycle(kFrame);
    std::vector<float> interleaved;
    for (float s : left)
    {
        interleaved.push_back(s);
        interleaved.push_back(1.0f); // the right channel is a constant that would show as DC
    }
    WavSpec spec;
    spec.channels = 2;
    const auto result = import(makeWav(interleaved, spec));

    REQUIRE(result.ok());
    REQUIRE(result.frames.size() == static_cast<size_t>(kFrame));
    REQUIRE_THAT(result.frames[512], WithinAbs(1.0, 1.0e-4));
}

TEST_CASE("Frames are centred and keep their relative level", "[core][wavetable][import]")
{
    std::vector<float> samples = sineCycle(kFrame);
    auto quiet = sineCycle(kFrame);
    for (float& s : quiet)
        s = s * 0.25f + 0.3f; // quieter, with a DC offset
    samples.insert(samples.end(), quiet.begin(), quiet.end());

    const auto result = import(makeWav(samples));
    REQUIRE(result.ok());

    double mean = 0.0;
    float peakQuiet = 0.0f;
    for (int i = 0; i < kFrame; ++i)
    {
        mean += result.frames[static_cast<size_t>(kFrame + i)];
        peakQuiet = std::max(peakQuiet, std::abs(result.frames[static_cast<size_t>(kFrame + i)]));
    }
    REQUIRE_THAT(mean / kFrame, WithinAbs(0.0, 1.0e-5));
    // The loud frame peaks at 1 after scaling, so the quiet one (a quarter as loud) peaks at 0.25.
    REQUIRE_THAT(peakQuiet, WithinAbs(0.25, 1.0e-3));
}

TEST_CASE("Unusable files are refused with a message", "[core][wavetable][import]")
{
    const auto refused = [](const WavetableImportResult& r)
    {
        REQUIRE_FALSE(r.ok());
        REQUIRE(r.error.isNotEmpty());
    };

    // Not a WAV file at all.
    refused(importWavetable(nullptr, 0));
    refused(import({}));
    refused(import(std::vector<uint8_t>(1000, 0x41)));
    const std::string text = "This is not audio, just text.";
    refused(importWavetable(text.data(), text.size()));

    // Truncated in the middle of the data: refused or silent-filled, never a crash or a bad table.
    {
        WavSpec spec;
        spec.truncateBy = true;
        const auto result = import(makeWav(sineCycle(kFrame * 2), spec));
        if (result.ok())
            REQUIRE(isValidStoredWavetable(result.frames));
    }

    // A header that declares a huge data chunk over a few bytes of data.
    {
        WavSpec spec;
        spec.declaredDataBytes = 0x7ffffff0;
        const auto result = import(makeWav(sineCycle(256), spec));
        if (result.ok())
            REQUIRE(isValidStoredWavetable(result.frames));
    }

    // Too many frames (257), and a length that is neither whole frames nor a short cycle.
    refused(import(makeWav(std::vector<float>(static_cast<size_t>(257) * kFrame, 0.5f))));
    refused(import(makeWav(sineCycle(20000))));
    refused(import(makeWav(sineCycle(32)))); // too short

    // Invalid samples.
    {
        auto samples = sineCycle(kFrame);
        samples[100] = std::numeric_limits<float>::quiet_NaN();
        refused(import(makeWav(samples)));
        samples[100] = std::numeric_limits<float>::infinity();
        refused(import(makeWav(samples)));
    }

    // Silent.
    refused(import(makeWav(std::vector<float>(kFrame, 0.0f))));
    refused(import(makeWav(std::vector<float>(kFrame, 0.4f)))); // a constant is silent once centred

    // Above the byte limit: refused without being read (the buffer is never touched).
    {
        static const std::vector<uint8_t> big(kMaxWavetableFileBytes + 1, 0);
        refused(importWavetable(big.data(), big.size()));
    }
}

TEST_CASE("Stored wavetables are checked before use", "[core][wavetable][import]")
{
    REQUIRE_FALSE(isValidStoredWavetable({}));
    REQUIRE_FALSE(isValidStoredWavetable(std::vector<float>(kFrame - 1, 0.1f)));
    REQUIRE_FALSE(isValidStoredWavetable(std::vector<float>(static_cast<size_t>(257) * kFrame, 0.1f)));
    REQUIRE(isValidStoredWavetable(std::vector<float>(static_cast<size_t>(256) * kFrame, 0.1f)));

    auto frames = std::vector<float>(kFrame, 0.1f);
    frames[7] = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(isValidStoredWavetable(frames));
    frames[7] = -std::numeric_limits<float>::infinity();
    REQUIRE_FALSE(isValidStoredWavetable(frames));
    frames[7] = 4.5f;
    REQUIRE_FALSE(isValidStoredWavetable(frames));
    frames[7] = -4.0f;
    REQUIRE(isValidStoredWavetable(frames));
}

TEST_CASE("Display names are cleaned and cut", "[core][wavetable][import]")
{
    REQUIRE(sanitiseWavetableName("  Kick 01  ") == "Kick 01");
    REQUIRE(sanitiseWavetableName(juce::String("A\nB\tC") + juce::String::charToString(0)) == "ABC");
    REQUIRE(sanitiseWavetableName(juce::String::repeatedString("x", 200)).length() == 64);
    REQUIRE(sanitiseWavetableName("").isEmpty());
}

TEST_CASE("The user wavetable of a selector plays, saves, restores and refuses bad data",
          "[core][wavetable][user]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    WavetableSelector selector(processor.state, 0);

    // Nothing loaded: the User entry plays the sine.
    auto* parameter = processor.state.getParameter("osc1.wavetable");
    parameter->setValueNotifyingHost(
        parameter->convertTo0to1(static_cast<float>(WavetableSelector::userEntry())));
    selector.update();
    REQUIRE(selector.currentEntry() == WavetableSelector::userEntry());
    REQUIRE(selector.source().acquire()->numFrames() == 1);
    REQUIRE_FALSE(selector.hasUser());
    REQUIRE_FALSE(selector.toValueTree().isValid());

    // Loading frames publishes them.
    std::vector<float> frames;
    for (int f = 0; f < 3; ++f)
        for (int i = 0; i < kFrame; ++i)
            frames.push_back(
                static_cast<float>(0.8 * std::sin(2.0 * std::numbers::pi * (f + 1) * i / kFrame)));
    REQUIRE(selector.loadUser(frames, "My\nKick"));
    REQUIRE(selector.hasUser());
    REQUIRE(selector.userName() == "MyKick");
    REQUIRE(selector.source().acquire()->numFrames() == 3);

    // Round trip through the saved node: the same frames, bit for bit, and the same name.
    const auto node = selector.toValueTree();
    REQUIRE(node.isValid());
    REQUIRE(node.getType().toString() == "UserWavetable1");

    fcm::test::TestProcessor other;
    WavetableSelector restored(other.state, 0);
    REQUIRE(restored.restoreFrom(node));
    REQUIRE(restored.hasUser());
    REQUIRE(restored.userName() == "MyKick");
    REQUIRE(restored.toValueTree()["frames"].toString() == node["frames"].toString());

    // Clearing goes back to the sine.
    selector.clearUser();
    REQUIRE_FALSE(selector.hasUser());
    REQUIRE(selector.source().acquire()->numFrames() == 1);

    // Invalid frames are refused and change nothing.
    REQUIRE_FALSE(selector.loadUser(std::vector<float>(100, 0.1f), "bad"));
    REQUIRE_FALSE(selector.hasUser());
}

TEST_CASE("A saved user wavetable node is untrusted", "[core][wavetable][user]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    WavetableSelector selector(processor.state, 1);

    const auto nodeWith = [](const juce::String& frames)
    {
        juce::ValueTree node("UserWavetable2");
        node.setProperty("name", "x", nullptr);
        node.setProperty("frames", frames, nullptr);
        return node;
    };
    const auto encode = [](const std::vector<float>& values)
    { return juce::MemoryBlock(values.data(), values.size() * sizeof(float)).toBase64Encoding(); };

    const std::vector<float> good(kFrame, 0.5f);
    REQUIRE(selector.restoreFrom(nodeWith(encode(good))));
    REQUIRE(selector.hasUser());

    // Every bad node leaves no user wavetable.
    auto withNan = good;
    withNan[3] = std::numeric_limits<float>::quiet_NaN();
    auto withInf = good;
    withInf[3] = std::numeric_limits<float>::infinity();
    auto tooLoud = good;
    tooLoud[3] = 5.0f;

    const std::vector<juce::String> bad{
        "",
        "!!! not base64 !!!",
        encode(std::vector<float>(kFrame - 1, 0.5f)),                        // not whole frames
        encode(std::vector<float>(static_cast<size_t>(257) * kFrame, 0.5f)), // too many frames
        encode(withNan),
        encode(withInf),
        encode(tooLoud),
        juce::String::repeatedString("A", 3'000'000),         // oversized text
        juce::MemoryBlock(good.data(), 7).toBase64Encoding(), // not a whole number of floats
    };
    for (const auto& text : bad)
    {
        REQUIRE(
            selector.restoreFrom(nodeWith(good.empty() ? juce::String() : encode(good)))); // set up a state
        REQUIRE_FALSE(selector.restoreFrom(nodeWith(text)));
        REQUIRE_FALSE(selector.hasUser());
    }

    // A missing node, or one of the wrong type, also leaves nothing.
    REQUIRE(selector.restoreFrom(nodeWith(encode(good))));
    REQUIRE_FALSE(selector.restoreFrom(juce::ValueTree()));
    REQUIRE_FALSE(selector.hasUser());
    REQUIRE(selector.restoreFrom(nodeWith(encode(good))));
    REQUIRE_FALSE(selector.restoreFrom(juce::ValueTree("UserWavetable1")));
    REQUIRE_FALSE(selector.hasUser());
}

TEST_CASE("The loader delivers on the message thread and reports a missing file", "[core][wavetable][loader]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto dir =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fcm_wavetable_tests");
    dir.createDirectory();
    const auto wavFile = dir.getChildFile("my cycle.wav");
    const auto bytes = makeWav(sineCycle(kFrame * 2));
    REQUIRE(wavFile.replaceWithData(bytes.data(), bytes.size()));

    struct Outcome
    {
        bool done = false;
        WavetableImportResult result;
        juce::String name;
    };
    const auto run = [&](const juce::File& file)
    {
        Outcome outcome;
        WavetableLoader loader;
        loader.load(file, [&outcome](WavetableImportResult r, juce::String n)
                    { outcome = {true, std::move(r), n}; });
        for (int i = 0; i < 200 && !outcome.done; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        return outcome;
    };

    const auto good = run(wavFile);
    REQUIRE(good.done);
    REQUIRE(good.result.ok());
    REQUIRE(good.result.frames.size() == static_cast<size_t>(kFrame) * 2);
    REQUIRE(good.name == "my cycle");

    const auto missing = run(dir.getChildFile("nothing here.wav"));
    REQUIRE(missing.done);
    REQUIRE_FALSE(missing.result.ok());
    REQUIRE(missing.result.error.isNotEmpty());

    dir.deleteRecursively();
}

TEST_CASE("Destroying the loader with a job pending never calls back", "[core][wavetable][loader]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    bool called = false;
    {
        WavetableLoader loader;
        loader.load(juce::File("/does/not/exist.wav"),
                    [&called](WavetableImportResult, juce::String) { called = true; });
    } // destroyed at once

    juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    REQUIRE_FALSE(called);
}

TEST_CASE("Randomly damaged WAV files never crash and never give an unusable table",
          "[core][wavetable][import]")
{
    // A fixed seed keeps the test reproducible. Each round flips a few bytes of a valid file, cuts
    // it, or both; the importer must either refuse or return a valid table.
    const auto original = makeWav(sineCycle(kFrame * 2));
    juce::Random random(12345);

    for (int round = 0; round < 3000; ++round)
    {
        auto bytes = original;
        const int flips = 1 + random.nextInt(6);
        for (int i = 0; i < flips; ++i)
            bytes[static_cast<size_t>(random.nextInt(static_cast<int>(bytes.size())))] =
                static_cast<uint8_t>(random.nextInt(256));
        if (random.nextBool())
            bytes.resize(static_cast<size_t>(random.nextInt(static_cast<int>(bytes.size()) + 1)));

        const auto result = import(bytes);
        if (result.ok())
            REQUIRE(isValidStoredWavetable(result.frames));
        else
            REQUIRE(result.error.isNotEmpty());
    }
}
