// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/AuditionSequencer.h"
#include "dsp/engine/Engine.h"
#include "dsp/engine/NoteQueue.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using fcm::dsp::AuditionSequencer;
using fcm::dsp::LiveNote;
using fcm::dsp::NoteQueue;

namespace
{
struct Event
{
    long long sample; // absolute position
    bool noteOn;
    bool allNotesOff;
    int note;
};

// Runs the sequencer for totalSamples in blocks of the given sizes (cycled) and collects the events.
std::vector<Event> run(AuditionSequencer& sequencer, long long totalSamples,
                       const std::vector<int>& blockSizes)
{
    std::vector<Event> events;
    juce::MidiBuffer midi;
    long long start = 0;
    size_t blockIndex = 0;

    while (start < totalSamples)
    {
        const int size = blockSizes[blockIndex++ % blockSizes.size()];
        midi.clear();
        sequencer.process(size, midi);

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            events.push_back({start + metadata.samplePosition, message.isNoteOn(), message.isAllNotesOff(),
                              message.isNoteOnOrOff() ? message.getNoteNumber() : -1});
        }
        start += size;
    }
    return events;
}

AuditionSequencer makeSequencer(double sampleRate, float bpm, bool enabled = true)
{
    AuditionSequencer sequencer;
    sequencer.prepare(sampleRate);
    sequencer.setBpm(bpm);
    sequencer.setEnabled(enabled);
    return sequencer;
}
} // namespace

TEST_CASE("NoteQueue returns events in order and reports empty", "[dsp][audition][queue]")
{
    NoteQueue queue;
    LiveNote event;
    REQUIRE_FALSE(queue.pop(event));

    REQUIRE(queue.push({true, 36, 100}));
    REQUIRE(queue.push({false, 36, 0}));
    REQUIRE(queue.push({true, 40, 90}));

    REQUIRE(queue.pop(event));
    REQUIRE((event.on && event.note == 36 && event.velocity == 100));
    REQUIRE(queue.pop(event));
    REQUIRE((!event.on && event.note == 36));
    REQUIRE(queue.pop(event));
    REQUIRE((event.on && event.note == 40 && event.velocity == 90));
    REQUIRE_FALSE(queue.pop(event));
}

TEST_CASE("NoteQueue drops new events when full and keeps the old ones", "[dsp][audition][queue]")
{
    NoteQueue queue;
    int accepted = 0;
    for (int i = 0; i < NoteQueue::kCapacity + 10; ++i)
        if (queue.push({true, i % 128, 100}))
            ++accepted;

    REQUIRE(accepted > 0);
    REQUIRE(accepted <= NoteQueue::kCapacity);

    LiveNote event;
    for (int i = 0; i < accepted; ++i)
    {
        REQUIRE(queue.pop(event));
        REQUIRE(event.note == i % 128); // the oldest survived
    }
    REQUIRE_FALSE(queue.pop(event));

    // And it can be used again afterwards.
    REQUIRE(queue.push({true, 7, 1}));
    REQUIRE(queue.pop(event));
    REQUIRE(event.note == 7);
}

TEST_CASE("NoteQueue passes events between two threads without loss or reordering", "[dsp][audition][queue]")
{
    constexpr int kEvents = 100000;
    NoteQueue queue;

    std::thread producer(
        [&]
        {
            for (int i = 0; i < kEvents;)
                if (queue.push({true, i % 128, (i / 128) % 128}))
                    ++i;
                else
                    std::this_thread::yield();
        });

    int received = 0;
    bool inOrder = true;
    LiveNote event;
    while (received < kEvents)
    {
        if (queue.pop(event))
        {
            inOrder = inOrder && event.note == received % 128 && event.velocity == (received / 128) % 128;
            ++received;
        }
        else
        {
            std::this_thread::yield();
        }
    }
    producer.join();

    REQUIRE(inOrder);
    REQUIRE_FALSE(queue.pop(event));
}

TEST_CASE("AuditionSequencer is silent while disabled", "[dsp][audition]")
{
    auto sequencer = makeSequencer(48000.0, 200.0f, false);
    REQUIRE(run(sequencer, 48000, {512}).empty());
}

TEST_CASE("AuditionSequencer plays one note per beat at the right samples", "[dsp][audition]")
{
    // 200 BPM at 48 kHz is a beat every 14400 samples; the note is held for 80 % of it.
    auto sequencer = makeSequencer(48000.0, 200.0f);
    const auto events = run(sequencer, 14400 * 4 + 100, {512});

    std::vector<long long> ons, offs;
    for (const auto& e : events)
    {
        if (e.allNotesOff)
            continue;
        (e.noteOn ? ons : offs).push_back(e.sample);
        REQUIRE(e.note == AuditionSequencer::kDefaultNote);
    }

    REQUIRE(ons == std::vector<long long>{0, 14400, 28800, 43200, 57600});
    REQUIRE(offs == std::vector<long long>{11520, 25920, 40320, 54720});
}

TEST_CASE("AuditionSequencer timing does not depend on the block size and does not drift", "[dsp][audition]")
{
    // 177 BPM at 44.1 kHz is 14949.152... samples per beat: fractional, so drift would show up.
    constexpr double sampleRate = 44100.0;
    constexpr float bpm = 177.0f;
    const long long ten_minutes = static_cast<long long>(600 * sampleRate);

    auto reference = makeSequencer(sampleRate, bpm);
    auto odd = makeSequencer(sampleRate, bpm);
    const auto a = run(reference, ten_minutes, {441});
    const auto b = run(odd, ten_minutes, {1, 977, 64, 4096, 333});

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i)
    {
        REQUIRE(a[i].sample == b[i].sample);
        REQUIRE(a[i].noteOn == b[i].noteOn);
    }

    // Note-ons are on the beat grid to within a sample, even 10 minutes in.
    const double beat = 60.0 / bpm * sampleRate;
    int beatIndex = 0;
    for (const auto& e : a)
    {
        if (!e.noteOn || e.allNotesOff)
            continue;
        REQUIRE(std::abs(static_cast<double>(e.sample) - beatIndex * beat) <= 1.0);
        ++beatIndex;
    }
    REQUIRE(beatIndex > 1700);
}

TEST_CASE("AuditionSequencer starts with All Notes Off and stops with a note-off", "[dsp][audition]")
{
    auto sequencer = makeSequencer(48000.0, 200.0f, false);
    juce::MidiBuffer midi;

    sequencer.setEnabled(true);
    sequencer.process(256, midi);
    std::vector<Event> first;
    for (const auto metadata : midi)
        first.push_back({metadata.samplePosition, metadata.getMessage().isNoteOn(),
                         metadata.getMessage().isAllNotesOff(), -1});
    REQUIRE(first.size() == 2);
    REQUIRE(first[0].allNotesOff);
    REQUIRE(first[1].noteOn);
    REQUIRE(first[1].sample == 0);

    // Switched off while the note sounds: a note-off at the start of the next block, then nothing.
    sequencer.setEnabled(false);
    midi.clear();
    sequencer.process(256, midi);
    REQUIRE(midi.getNumEvents() == 1);
    REQUIRE((*midi.begin()).getMessage().isNoteOff());

    midi.clear();
    sequencer.process(256, midi);
    REQUIRE(midi.getNumEvents() == 0);
}

TEST_CASE("AuditionSequencer applies a tempo change from the next beat", "[dsp][audition]")
{
    auto sequencer = makeSequencer(48000.0, 200.0f);
    juce::MidiBuffer midi;
    std::vector<long long> ons;
    long long start = 0;

    auto step = [&](int size)
    {
        midi.clear();
        sequencer.process(size, midi);
        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOn())
                ons.push_back(start + metadata.samplePosition);
        start += size;
    };

    step(10000);              // first note at 0
    sequencer.setBpm(100.0f); // beat becomes 28800 samples, but the beat under way keeps its length
    step(50000);

    REQUIRE(ons.size() == 3);
    REQUIRE(ons[0] == 0);
    REQUIRE(ons[1] == 14400); // already scheduled with the old tempo
    REQUIRE(ons[2] == 14400 + 28800);
}

TEST_CASE("AuditionSequencer uses the latest note and clamps the tempo", "[dsp][audition]")
{
    auto sequencer = makeSequencer(48000.0, 200.0f);
    sequencer.setNote(40);
    auto events = run(sequencer, 100, {100});
    REQUIRE(events.back().note == 40);

    sequencer.setNote(45); // takes effect at the next beat
    events = run(sequencer, 14400, {512});
    REQUIRE(events.back().note == 45);

    for (float bpm : {-5.0f, 0.0f, 1.0f, 10000.0f})
    {
        auto clamped = makeSequencer(48000.0, bpm);
        const auto e = run(clamped, 48000 * 10, {512});
        const double beat =
            48000.0 * 60.0 / (bpm < 100.0f ? AuditionSequencer::kMinBpm : AuditionSequencer::kMaxBpm);
        REQUIRE(e.size() > 2);
        REQUIRE(std::abs(static_cast<double>(e[3].sample) - beat) <=
                1.0); // e[0] all notes off, e[1] on, e[2] off, e[3] on
    }
}

TEST_CASE("AuditionSequencer reset starts over", "[dsp][audition]")
{
    auto sequencer = makeSequencer(48000.0, 200.0f);
    const auto first = run(sequencer, 30000, {512});
    sequencer.reset();
    sequencer.setEnabled(true);
    const auto second = run(sequencer, 30000, {512});
    REQUIRE(first.size() == second.size());
    for (size_t i = 0; i < first.size(); ++i)
        REQUIRE(first[i].sample == second[i].sample);
}

TEST_CASE("AuditionSequencer through the engine gives one kick per beat", "[dsp][audition][engine]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int kBlock = 480;

    fcm::dsp::Engine engine{fcm::test::noEffects()};
    fcm::dsp::EngineParameters parameters;
    parameters.outputGain = 0.5f;
    engine.setParameters(parameters);
    engine.prepare({sampleRate, kBlock, 2});

    auto sequencer = makeSequencer(sampleRate, 200.0f);
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    std::vector<float> out;

    for (int block = 0; block < 100 * 3; ++block) // 3 beats of 14400 samples
    {
        midi.clear();
        sequencer.process(kBlock, midi);
        buffer.clear();
        engine.process(buffer, midi);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }

    REQUIRE(fcm::test::allFinite(out));
    // Each beat starts with the pitch drop: loud right after the kick starts, and the previous
    // kick's tail is quiet by then because the note was released at 80 %.
    for (int beat = 0; beat < 3; ++beat)
    {
        const auto from = out.begin() + beat * 14400;
        const std::vector<float> start(from, from + 480);
        REQUIRE(fcm::test::peak(start) > 0.3f);
    }
    // Just before a kick the previous one has been released and faded: silence in the last 10 % of the beat.
    for (int beat = 0; beat < 2; ++beat)
    {
        const auto from = out.begin() + (beat + 1) * 14400 - 1000;
        const std::vector<float> end(from, from + 900);
        REQUIRE(fcm::test::peak(end) < 1.0e-3f);
    }
}
