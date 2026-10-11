// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/NoteHandler.h"
#include "dsp/engine/PitchGlide.h"
#include "dsp/engine/SafetyLimiter.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_core/juce_core.h>

using fcm::dsp::NoteHandler;
using fcm::dsp::PitchGlide;
using fcm::dsp::SafetyLimiter;

TEST_CASE("NoteHandler gives the last played note priority", "[dsp][notes]")
{
    NoteHandler notes;
    REQUIRE_FALSE(notes.hasHeldNotes());

    notes.noteOn(36);
    notes.noteOn(40);
    REQUIRE(notes.heldCount() == 2);

    // Releasing the top note moves back to the older held note.
    REQUIRE(notes.noteOff(40) == 36);
    // Releasing the last held note leaves nothing to go back to.
    REQUIRE_FALSE(notes.noteOff(36).has_value());
    REQUIRE_FALSE(notes.hasHeldNotes());
}

TEST_CASE("NoteHandler ignores releases that do not change the top note", "[dsp][notes]")
{
    NoteHandler notes;
    notes.noteOn(36);
    notes.noteOn(40);

    REQUIRE_FALSE(notes.noteOff(36).has_value()); // older note: the sounding pitch stays
    REQUIRE(notes.heldCount() == 1);
    REQUIRE_FALSE(notes.noteOff(99).has_value()); // never held
    REQUIRE(notes.heldCount() == 1);
}

TEST_CASE("NoteHandler moves a repeated note to the top instead of duplicating it", "[dsp][notes]")
{
    NoteHandler notes;
    notes.noteOn(36);
    notes.noteOn(40);
    notes.noteOn(36);

    REQUIRE(notes.heldCount() == 2);
    REQUIRE(notes.noteOff(36) == 40);
}

TEST_CASE("NoteHandler drops the oldest note when full", "[dsp][notes]")
{
    NoteHandler notes;
    for (int note = 0; note < NoteHandler::kMaxHeldNotes + 1; ++note)
        notes.noteOn(note);

    REQUIRE(notes.heldCount() == NoteHandler::kMaxHeldNotes);

    // Releasing from the top walks back down; note 0 (the oldest) was dropped.
    for (int note = NoteHandler::kMaxHeldNotes; note > 1; --note)
        REQUIRE(notes.noteOff(note) == note - 1);
    REQUIRE_FALSE(notes.noteOff(1).has_value());
    REQUIRE_FALSE(notes.hasHeldNotes());
}

TEST_CASE("NoteHandler allNotesOff clears the stack", "[dsp][notes]")
{
    NoteHandler notes;
    notes.noteOn(36);
    notes.noteOn(40);
    notes.allNotesOff();
    REQUIRE_FALSE(notes.hasHeldNotes());
    REQUIRE_FALSE(notes.noteOff(40).has_value());
}

TEST_CASE("PitchGlide jumps before the first note and with glide off", "[dsp][glide]")
{
    PitchGlide glide;
    glide.prepare(48000.0);
    REQUIRE_FALSE(glide.hasPitch());

    glide.setTarget(36.0f, true, 100.0f); // first note: nothing to slide from
    REQUIRE(juce::exactlyEqual(glide.next(), 36.0f));

    glide.setTarget(60.0f, false, 100.0f);
    REQUIRE(juce::exactlyEqual(glide.next(), 60.0f));

    glide.setTarget(48.0f, true, 0.0f); // zero time is a jump too
    REQUIRE(juce::exactlyEqual(glide.next(), 48.0f));
}

TEST_CASE("PitchGlide ramp takes exactly the glide time, whatever the interval", "[dsp][glide]")
{
    for (float interval : {1.0f, 12.0f, 48.0f})
    {
        PitchGlide glide;
        glide.prepare(48000.0);
        glide.setTarget(36.0f, true, 10.0f);
        glide.setTarget(36.0f + interval, true, 10.0f); // 10 ms = 480 samples

        float value = 0.0f;
        for (int i = 1; i < 480; ++i)
        {
            const float previous = value;
            value = glide.next();
            REQUIRE(value > previous);
            REQUIRE(value < 36.0f + interval);
        }
        // The 480th sample lands exactly on the target.
        REQUIRE(juce::exactlyEqual(glide.next(), 36.0f + interval));
        REQUIRE(juce::exactlyEqual(glide.next(), 36.0f + interval));
    }
}

TEST_CASE("PitchGlide is linear in semitones", "[dsp][glide]")
{
    PitchGlide glide;
    glide.prepare(48000.0);
    glide.setTarget(36.0f, true, 10.0f);
    glide.setTarget(48.0f, true, 10.0f);

    float value = 0.0f;
    for (int i = 0; i < 240; ++i) // halfway
        value = glide.next();
    REQUIRE_THAT(value, Catch::Matchers::WithinAbs(42.0, 1.0e-3));
}

TEST_CASE("PitchGlide setting the same target does not restart the ramp", "[dsp][glide]")
{
    PitchGlide glide;
    glide.prepare(48000.0);
    glide.setTarget(36.0f, true, 10.0f);
    glide.setTarget(48.0f, true, 10.0f);
    for (int i = 0; i < 100; ++i)
        glide.next();
    const float before = glide.next();

    glide.setTarget(48.0f, true, 10.0f); // called every block by the engine
    REQUIRE(glide.next() > before);

    for (int i = 0; i < 400; ++i)
        glide.next();
    REQUIRE(juce::exactlyEqual(glide.next(), 48.0f)); // it still finished on time
}

TEST_CASE("PitchGlide retargeting mid-slide starts a new ramp from where it is", "[dsp][glide]")
{
    PitchGlide glide;
    glide.prepare(48000.0);
    glide.setTarget(36.0f, true, 10.0f);
    glide.setTarget(48.0f, true, 10.0f);

    float value = 0.0f;
    for (int i = 0; i < 240; ++i)
        value = glide.next(); // about 42

    glide.setTarget(30.0f, true, 10.0f);
    const float first = glide.next();
    REQUIRE(first < value);
    REQUIRE(first > 30.0f);
    for (int i = 0; i < 479; ++i)
        glide.next();
    REQUIRE(juce::exactlyEqual(glide.next(), 30.0f));
}

TEST_CASE("SafetyLimiter never exceeds 0 dBFS and is transparent below the knee", "[dsp][limiter]")
{
    for (float x = -0.8f; x <= 0.8f; x += 0.01f)
        REQUIRE(juce::exactlyEqual(SafetyLimiter::process(x), x));

    for (float x = 0.0f; x < 1.0e6f; x = x * 1.1f + 0.01f)
    {
        REQUIRE(std::abs(SafetyLimiter::process(x)) <= 1.0f);
        REQUIRE(std::abs(SafetyLimiter::process(-x)) <= 1.0f);
    }
}

TEST_CASE("SafetyLimiter is monotonic and continuous at the knee", "[dsp][limiter]")
{
    float previous = SafetyLimiter::process(0.0f);
    for (float x = 0.001f; x < 20.0f; x += 0.001f)
    {
        const float y = SafetyLimiter::process(x);
        REQUIRE(y >= previous);
        REQUIRE(y - previous < 0.0011f); // slope never above 1 (plus rounding)
        previous = y;
    }
}

TEST_CASE("SafetyLimiter replaces non-finite input with silence", "[dsp][limiter]")
{
    REQUIRE(juce::exactlyEqual(SafetyLimiter::process(std::nanf("")), 0.0f));
    REQUIRE(juce::exactlyEqual(SafetyLimiter::process(INFINITY), 0.0f));
    REQUIRE(juce::exactlyEqual(SafetyLimiter::process(-INFINITY), 0.0f));
}
