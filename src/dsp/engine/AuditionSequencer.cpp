// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/AuditionSequencer.h"

#include <algorithm>
#include <limits>

namespace fcm::dsp
{

void AuditionSequencer::addEvent(juce::MidiBuffer& out, const juce::MidiMessage& message, double position,
                                 int numSamples) const
{
    out.addEvent(message, std::clamp(static_cast<int>(position), 0, numSamples - 1));
}

void AuditionSequencer::process(int numSamples, juce::MidiBuffer& out) noexcept
{
    if (numSamples <= 0)
        return;

    constexpr int kChannel = 1;

    if (!enabled)
    {
        if (sounding)
            addEvent(out, juce::MidiMessage::noteOff(kChannel, soundingNote), 0.0, numSamples);
        sounding = false;
        wasEnabled = false;
        return;
    }

    if (!wasEnabled)
    {
        // Starting: a key held down before must not stay stuck under the loop, and the first kick
        // lands right away.
        addEvent(out, juce::MidiMessage::allNotesOff(kChannel), 0.0, numSamples);
        sounding = false;
        untilNextOn = 0.0;
        wasEnabled = true;
    }

    const double beatSamples = 60.0 / static_cast<double>(bpm) * sampleRate;
    constexpr double kNever = std::numeric_limits<double>::infinity();

    double position = 0.0; // samples into this block
    for (;;)
    {
        const double toOff = sounding ? untilOff : kNever;
        const double step = std::min(untilNextOn, toOff);

        if (position + step >= numSamples)
        {
            const double rest = numSamples - position;
            untilNextOn -= rest;
            untilOff -= rest;
            break;
        }

        const bool offComesFirst = sounding && toOff <= untilNextOn; // a tie closes the note first

        position += step;
        untilNextOn -= step;
        untilOff -= step;

        if (offComesFirst)
        {
            addEvent(out, juce::MidiMessage::noteOff(kChannel, soundingNote), position, numSamples);
            sounding = false;
        }
        else
        {
            if (sounding)
                addEvent(out, juce::MidiMessage::noteOff(kChannel, soundingNote), position, numSamples);

            soundingNote = note;
            addEvent(out,
                     juce::MidiMessage::noteOn(kChannel, soundingNote, static_cast<juce::uint8>(kVelocity)),
                     position, numSamples);
            sounding = true;
            untilNextOn += beatSamples;
            untilOff = beatSamples * kGateFraction;
        }
    }
}

} // namespace fcm::dsp
