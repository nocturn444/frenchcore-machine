// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace fcm::dsp
{

inline constexpr int kMaxNoteTriggers = 16;

// Where the engine's recent note-ons are, in the engine's own sample clock (the number of samples
// the engine has processed since it was prepared or reset). A fixed ring of the last 16, so nothing
// allocates (docs/decisions/0012 item 1). The clock position is the sample at which the note's
// sound starts at the voice's output.
class NoteLog
{
  public:
    void clear() noexcept
    {
        count = 0;
        next = 0;
    }

    void push(std::int64_t clock) noexcept
    {
        entries[static_cast<size_t>(next)] = clock;
        next = (next + 1) % kMaxNoteTriggers;
        count = std::min(count + 1, kMaxNoteTriggers);
    }

    int size() const noexcept { return count; }
    std::int64_t at(int index) const noexcept { return entries[static_cast<size_t>(index)]; }

  private:
    std::array<std::int64_t, kMaxNoteTriggers> entries{};
    int count = 0;
    int next = 0;
};

// What a rack needs to hand note-ons to its effects for one block: the log, the engine clock at the
// block's first sample, and how long the note's sound takes to get to the rack's input (0 for the
// buses; the latency of the whole pool for Main, whose input is delayed to line up with the buses,
// docs/decisions/0010).
struct RackTriggers
{
    const NoteLog* log = nullptr;
    std::int64_t clockStart = 0;
    int inputDelay = 0;
};

// The note-ons an effect has been told about for the buffer it is about to process, in order, with a
// helper that cuts the buffer at them. Fixed size; for effects that restart something at a note
// (docs/decisions/0012, docs/ADDING_AN_EFFECT.md).
class PendingNoteOns
{
  public:
    void clear() noexcept { count = 0; }

    // Offsets are in the order they are told (the rack tells them in time order). Offsets beyond the
    // capacity are dropped.
    void add(int sampleOffset) noexcept
    {
        if (count < kMaxNoteTriggers && sampleOffset >= 0)
            offsets[static_cast<size_t>(count++)] = sampleOffset;
    }

    // Calls `segment(begin, end, restartAtBegin)` for consecutive pieces that together cover
    // [0, numSamples): a piece starts at each note-on offset (restartAtBegin true) and at 0. Then it
    // forgets the note-ons.
    template <typename Callback> void run(int numSamples, Callback&& segment) noexcept
    {
        int begin = 0;
        bool restart = false;
        for (int i = 0; i < count; ++i)
        {
            const int at = std::min(offsets[static_cast<size_t>(i)], numSamples);
            if (at > begin)
            {
                segment(begin, at, restart);
                begin = at;
                restart = false;
            }
            restart = true; // several note-ons at one sample count once
        }
        if (begin < numSamples || restart)
            segment(begin, numSamples, restart);
        count = 0;
    }

  private:
    std::array<int, kMaxNoteTriggers> offsets{};
    int count = 0;
};

} // namespace fcm::dsp
