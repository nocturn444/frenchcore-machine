// SPDX-License-Identifier: AGPL-3.0-only
#include "core/BoundedMidiCopy.h"

namespace fcm::core
{

MidiCopyResult copyMidiWithinBudget(const juce::MidiBuffer& source, juce::MidiBuffer& destination,
                                    int numSamples, int maxBytes) noexcept
{
    MidiCopyResult result;
    int usedBytes = 0;

    for (const auto metadata : source)
    {
        if (metadata.samplePosition < 0 || metadata.samplePosition >= numSamples)
            continue;

        const int eventBytes = kMidiEventOverheadBytes + metadata.numBytes;
        if (usedBytes + eventBytes > maxBytes)
        {
            // Keep counting so the caller can tell how much was lost, but stop copying: a smaller
            // later event must not jump the queue ahead of a dropped one and reorder the stream.
            ++result.dropped;
            usedBytes = maxBytes;
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
