# Spec: Standalone audition (on-screen keyboard and auto-play)

- Status: Implemented (requested by the product owner on 2026-10-11; the values in section 3 are the ones they asked for or, where marked, first choices)
- Owner: nocturn4 (product), implemented by agent session
- Related: [voice-note-handling](voice-note-handling.md), [ARCHITECTURE §6, §7](../ARCHITECTURE.md)

## 1. What it does for the user

When you open Frenchcore Machine as the **Standalone** app (no DAW), you can hear the kick without any MIDI hardware:

- **On-screen keyboard.** Click a key with the mouse to play it, or use the computer keyboard like a small piano. The keys `A W S E D F T G Y H U J K O L P ;` play the notes from C1 upward (the lower row of letters are the white keys, the row above are the black keys). `Z` and `X` move the whole set down or up one octave.
- **Auto-play.** A toggle that repeats the kick by itself at a tempo you set, so you can tweak the sound while it loops. Default tempo 200 BPM. One kick per beat; each note is held for 80 % of the beat, so the tail rings almost until the next kick. The pitch is the last key you chose (C1 until you choose one).
- While auto-play is on, pressing a key does not play a note on its own: it **chooses the pitch** of the loop, which changes at the next kick.

None of this exists in the **VST3 and AU** versions: a DAW already provides notes and tempo, so the panel does not appear there and the plugin does no audition processing.

Auto-play and the keyboard are tools for listening, not part of the sound: they are not saved in presets or projects and they are not parameters (so the DAW never sees them). They start off, at 200 BPM, every time the Standalone opens.

## 2. User interface

A strip at the bottom of the Standalone window (the window is taller than the plugin versions by the height of the strip):

```
[ Auto-play ]  BPM [ 200 ]   |  on-screen keyboard (MIDI notes 24 to 83)
```

- **Auto-play**: toggle button.
- **BPM**: slider with a number box; type a value or drag. Range 60 to 300, whole numbers.
- **Keyboard**: horizontal, five octaves. Mouse clicks play velocity 100. The computer keys also play velocity 100.
- The computer keys work whenever the window has focus (not only after clicking the keyboard), except while typing in the BPM box.

## 3. Parameters

None. These are not automatable parameters and are not part of the saved state. They live in `fcm_core::AuditionState` (plain atomics), owned by the processor and used only when the plugin runs as Standalone.

| Setting | Range | Default | Notes |
|---|---|---|---|
| Auto-play | off / on | off | |
| BPM | 60 to 300 | 200 | Default asked for by the product owner. |
| Gate length | fixed | 80 % of a beat | First choice. |
| Velocity | fixed | 100 | Same for the keyboard, the computer keys and auto-play. |
| Auto-play note | last key chosen | MIDI note 36 (C1) | |

## 4. DSP / implementation approach

Threading follows [ARCHITECTURE §6](../ARCHITECTURE.md): the audio thread takes no locks and allocates nothing.

- **UI to audio.** The keyboard component is fed by a `juce::MidiKeyboardState`, which takes a lock, so the audio thread never touches it. A listener on the message thread turns each note event into a `LiveNote` and pushes it into `fcm::dsp::NoteQueue`, a fixed-size single-producer single-consumer queue (`juce::AbstractFifo`, 64 entries; events are dropped if it is full). The audio thread pops everything at the start of each block and puts the events at sample 0 of the block.
- **Auto-play** is generated on the audio thread by `fcm::dsp::AuditionSequencer`, which writes note-on/note-off events at sample-accurate positions into the block's MIDI buffer. The beat length is `60 / BPM` seconds as a fractional sample count, carried across blocks so there is no drift. Turning it on fires a kick at once (preceded by an All Notes Off so a key held down before does not stay stuck). Turning it off sends the note-off of a sounding note. A BPM change applies from the next beat.
- **MIDI buffer.** The host's MIDI buffer is copied into a buffer owned by the processor, with its capacity reserved in `prepareToPlay`, and the audition events are added there, so `processBlock` allocates nothing. The copy has a byte budget (`core::copyMidiWithinBudget`, 6144 of the 8192 reserved bytes, the rest is headroom for the audition events): a host burst bigger than that is cut off, later events are dropped, never reordered, and the buffer never has to grow. Only the Standalone does this; the other formats pass the host buffer straight through.
- **Standalone detection.** `juce::AudioProcessor::wrapperType` equals `wrapperType_Standalone`. The editor shows the strip only then.
- **Keyboard focus.** The panel registers as a key listener on the editor, so the computer keys work wherever the focus is inside the window.

## 5. Tests

Catch2, in `tests/dsp/` (headless):

- `NoteQueue`: events come out in order; a full queue drops new events and keeps the old ones; emptied queue can be reused; a producer thread and a consumer thread exchange 100000 events without loss or reordering.
- `AuditionSequencer`: disabled produces nothing; enabled fires the first note-on at sample 0 and then one per beat at the right sample (200 BPM at 48 kHz is every 14400 samples), also across block boundaries and with odd block sizes, without drift over 10 minutes of audio; note-off after 80 % of the beat; turning it off sends the note-off; turning it on sends All Notes Off first; a BPM change applies from the next beat; the note can be changed between beats; out-of-range BPM is clamped; reset returns it to the start.
- `copyMidiWithinBudget` (fcm_core_tests): a small block is copied completely; events outside the block are skipped; a burst of 100000 events is capped to the budget and counted as dropped; room is left for the audition events; an oversized sysex is dropped; order is kept and a dropped event is never overtaken by a later one.
- Engine integration: the sequencer's events through the engine give one retriggered kick per beat and silence the rest of the time after the release.
- `AuditionState` (fcm_core_tests): defaults (off, 200 BPM, note 36); BPM is clamped; `sendNote` reaches the queue.

Manual check (Standalone): the strip is there; mouse and computer keys play; `Z`/`X` shift the octave; auto-play loops at the chosen BPM, changing the BPM while looping works, and the VST3/AU editors do not show the strip.

## 6. Out of scope

- Remembering auto-play and BPM between launches.
- Sync to a host tempo (a DAW provides notes already).
- Patterns, swing, accents or note sequences: auto-play repeats one note.
- Showing the strip in VST3/AU.
- Choosing the gate length or velocity.

## 7. Open questions

1. Gate length 80 % of the beat is a first choice: the tail rings nearly until the next kick. Say if you would rather have a different feel.
