// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace fcm::dsp
{

struct LiveNote
{
    bool on = false;
    int note = 0;
    int velocity = 0;
};

// Hands note events from the message thread to the audio thread without locks
// (docs/specs/standalone-audition.md §4): exactly one producer thread and one consumer thread.
// When the queue is full new events are dropped, which only happens if the audio thread stops.
class NoteQueue
{
  public:
    static constexpr int kCapacity = 64;

    // Producer thread only. Returns false when the event was dropped.
    bool push(const LiveNote& event) noexcept
    {
        const auto write = fifo.write(1);
        if (write.blockSize1 == 0)
            return false;

        slots[static_cast<size_t>(write.startIndex1)] = event;
        return true;
    }

    // Consumer thread only. Returns false when the queue is empty.
    bool pop(LiveNote& event) noexcept
    {
        const auto read = fifo.read(1);
        if (read.blockSize1 == 0)
            return false;

        event = slots[static_cast<size_t>(read.startIndex1)];
        return true;
    }

  private:
    juce::AbstractFifo fifo{kCapacity};
    std::array<LiveNote, kCapacity> slots{};
};

} // namespace fcm::dsp
