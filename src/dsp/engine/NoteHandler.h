// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <optional>

namespace fcm::dsp
{

// Tracks the held MIDI notes of the monophonic voice with last-note priority
// (docs/specs/voice-note-handling.md §4.1). Fixed capacity, so it never allocates.
class NoteHandler
{
  public:
    static constexpr int kMaxHeldNotes = 16;

    // The note becomes the top of the stack. If it is already held it is moved to the top.
    void noteOn(int note) noexcept;

    // Removes the note. Returns the note that is now on top only when the released note was the top
    // one and an older note is still held (the voice should move its pitch back to it).
    std::optional<int> noteOff(int note) noexcept;

    void allNotesOff() noexcept { count = 0; }

    bool hasHeldNotes() const noexcept { return count > 0; }
    int heldCount() const noexcept { return count; }

  private:
    void removeAt(int index) noexcept;

    std::array<int, kMaxHeldNotes> notes{};
    int count = 0;
};

} // namespace fcm::dsp
