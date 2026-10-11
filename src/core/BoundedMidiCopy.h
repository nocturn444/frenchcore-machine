// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace fcm::core
{

// Standalone only: the processor copies the host's MIDI into a buffer whose memory was reserved in
// prepareToPlay. juce::MidiBuffer grows (allocates) when it runs out of room, which the audio
// thread must never do, so the copy has a byte budget and drops what does not fit.

// Reserved size of the buffer, and the part of it the host's events may use. The rest is headroom
// for the audition events of one block (at most a few dozen note events).
constexpr int kAuditionMidiBytes = 8192;
constexpr int kHostMidiBudgetBytes = 6144;
// Messages that end sound (note-off, note-on with velocity 0, All Notes Off, All Sound Off) may keep
// using the reserved room up to this total after the normal budget is spent, so a burst never
// leaves a note stuck on. What remains (1024 bytes) is still enough for the audition events.
constexpr int kHostMidiPriorityBudgetBytes = 7168;

// What juce::MidiBuffer stores per event on top of its data bytes: a 4-byte sample position and a
// 2-byte size (juce_MidiBuffer.cpp). Rounded up to 8 on purpose so the budget stays below the real
// capacity.
constexpr int kMidiEventOverheadBytes = 8;

struct MidiCopyResult
{
    int copied = 0;
    int dropped = 0;
};

// True for messages that end sound: note-off, note-on with velocity 0, CC 120 and CC 123.
bool isSoundEndingMessage(const juce::uint8* data, int numBytes) noexcept;

// Copies the events of `source` whose position is in [0, numSamples) into `destination`, which must
// be empty and have its memory reserved, until `maxBytes` of storage are used. After the first event
// that does not fit, other events are dropped (so the stream is never reordered), except that
// sound-ending messages keep being copied until `priorityMaxBytes` are used. Does not allocate if
// `priorityMaxBytes` is below the reserved size of `destination`.
MidiCopyResult copyMidiWithinBudget(const juce::MidiBuffer& source, juce::MidiBuffer& destination,
                                    int numSamples, int maxBytes, int priorityMaxBytes) noexcept;

} // namespace fcm::core
