// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/NoteHandler.h"

namespace fcm::dsp
{

void NoteHandler::removeAt(int index) noexcept
{
    for (int i = index; i < count - 1; ++i)
        notes[static_cast<size_t>(i)] = notes[static_cast<size_t>(i + 1)];
    --count;
}

void NoteHandler::noteOn(int note) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        if (notes[static_cast<size_t>(i)] == note)
        {
            removeAt(i);
            break;
        }
    }

    if (count == kMaxHeldNotes)
        removeAt(0); // drop the oldest

    notes[static_cast<size_t>(count++)] = note;
}

std::optional<int> NoteHandler::noteOff(int note) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        if (notes[static_cast<size_t>(i)] != note)
            continue;

        const bool wasTop = (i == count - 1);
        removeAt(i);
        if (wasTop && count > 0)
            return notes[static_cast<size_t>(count - 1)];
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace fcm::dsp
