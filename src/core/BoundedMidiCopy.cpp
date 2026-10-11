// SPDX-License-Identifier: AGPL-3.0-only
#include "core/BoundedMidiCopy.h"

namespace fcm::core
{

bool isSoundEndingMessage(const juce::uint8* data, int numBytes) noexcept
{
    if (numBytes < 2)
        return false;

    const int status = data[0] & 0xF0;
    if (status == 0x80)
        return true;
    if (status == 0x90)
        return numBytes >= 3 && data[2] == 0;
    return status == 0xB0 && (data[1] == 120 || data[1] == 123);
}

MidiCopyResult copyMidiWithinBudget(const juce::MidiBuffer& source, juce::MidiBuffer& destination,
                                    int numSamples, int maxBytes, int priorityMaxBytes) noexcept
{
    MidiCopyResult result;
    int usedBytes = 0;
    bool overflowed = false;

    for (const auto metadata : source)
    {
        if (metadata.samplePosition < 0 || metadata.samplePosition >= numSamples)
            continue;

        const int eventBytes = kMidiEventOverheadBytes + metadata.numBytes;
        const bool fitsNormally = !overflowed && usedBytes + eventBytes <= maxBytes;
        // Once something was dropped, ordinary events stay dropped so a smaller later event does
        // not jump ahead of a dropped one. Messages that end sound still get through: a lost
        // note-off would leave a note held for as long as the envelope sustains.
        const bool fitsAsPriority = !fitsNormally && usedBytes + eventBytes <= priorityMaxBytes &&
                                    isSoundEndingMessage(metadata.data, metadata.numBytes);
        if (!fitsNormally && !fitsAsPriority)
        {
            overflowed = true;
            ++result.dropped;
            continue;
        }

        // Raw bytes: building a juce::MidiMessage would allocate for long messages (sysex).
        destination.addEvent(metadata.data, metadata.numBytes, metadata.samplePosition);
        usedBytes += eventBytes;
        ++result.copied;
    }

    return result;
}

} // namespace fcm::core
