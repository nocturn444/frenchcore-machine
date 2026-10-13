# MVP Scope (v0.1.0)

The MVP is the smallest version of Frenchcore Machine that a frenchcore producer can load in a DAW and use to make a finished kick faster than with a standard synth. Anything not listed under "In scope" waits for a later version.

## Definition of done for the MVP

The MVP is done when all of the following are true:

1. VST3, AU (macOS) and Standalone builds are produced by CI for Windows, macOS and Linux.
2. pluginval passes at strictness 10 on all three platforms.
3. A user can, from the default patch, get a usable frenchcore kick in under a minute by choosing a wavetable, a preset envelope shape and a distortion type.
4. Every feature in the table below works, is covered by unit tests where it has DSP, and is documented in the user guide.
5. Saving and reopening a DAW project restores the exact sound, including a user-loaded wavetable and the FX order.

## In scope

| Area | Feature | Notes |
|---|---|---|
| Voice | Monophonic, retrigger on each note | Last-note priority. |
| Voice | MIDI note sets the bass pitch, key-track switch for fixed pitch | |
| Voice | Optional glide with time control | |
| Voice | Velocity sensitivity | |
| Oscillators | Two identical wavetable oscillators: on/off, level, pitch, octave, fine tune, position, phase | Band-limited mipmaps. |
| Oscillator | 6 to 10 factory wavetables designed for kicks | Sine, triangle-ish, saturated sines, "hardcore" shapes. |
| Oscillator | Load user wavetables (Serum/Vital-style WAV, 2048-sample frames) | Also accepts a single-cycle WAV. Embedded in the project state. |
| Tock | Optional transient layer, Synth mode (generated click) or Sample mode (factory tocks + user samples) | Level, tune, start, decay, tone. |
| Envelopes | Amp envelope, breakpoint editor + 5 AHDSR knobs on the same curve, default "kick + bass" shape | Add/move/delete points, segment curvature. |
| Envelopes | Pitch envelope, breakpoint editor + 5 knobs, amount in semitones | |
| Punch/Tail | A level for the punch and one for the tail, with a split-time slider that divides the kick | |
| Envelopes | Shape presets for both envelopes | e.g. "Kick + Bass", "Short Punch", "Long Tail". |
| FX | Distortion: soft clip, hard clip, overdrive, foldback, bitcrush | Oversampled per the global quality setting. |
| FX | 3-band parametric EQ | |
| FX | Multiband stereo width (3 bands) | Mono-below option. |
| FX | User-reorderable chain (drag and drop), per-slot enable and mix | Serum-style, one slot per effect type. |
| Global | Output gain, safety limiter, quality (oversampling), glide, velocity, key track | |
| Presets | Factory presets, save/load user presets, prev/next browsing | |
| UI | Functional, resizable editor following [UI_LAYOUT.md](UI_LAYOUT.md) | Placeholder styling. |
| UI | Standalone-only strip with an on-screen keyboard and an auto-play loop at a set BPM | Added at the product owner's request; not in VST3/AU. See [spec](specs/standalone-audition.md). |
| Docs | User guide, build guide, architecture, contribution guide | |

## Out of scope for the MVP (candidates for later)

- Visual design / final skin (planned right after the MVP).
- CLAP format.
- More effects: compressor/OTT, filter, reverb, transient shaper, "kick tuner" analyzer.
- Multiple instances of the same effect in the chain.
- LFOs (two, per the UI sketch) and modulation of wavetable position or FX parameters (modulation system).
- Wavetable editor or drawing tool.
- Sample layer beyond the click (full sample-based kick layer).
- Preset tagging, search, online sharing.
- macOS code signing and notarization, installers.
- MIDI learn.

## Open items to confirm with the product owner

- Contents of the reserved "Other" area in [UI_LAYOUT.md](UI_LAYOUT.md).
- The exact list and sound of the factory wavetables and presets.
