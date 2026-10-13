# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-10-13

First release. VST3, AU and Standalone for Windows, macOS and Linux (the plugins are not signed yet; see the installation steps in the user guide).

### Added

- **Kick voice**: monophonic, a MIDI note plays a pitched kick whose tail follows the note, with Key Track and a fixed Root Note, optional Glide, Velocity Sensitivity (0 % by default), a safety limiter on the output and retrigger and release without clicks.
- **Two wavetable oscillators** (On, Wavetable, Level, Pitch, Octave, Fine, Position, Phase) with nine factory wavetables (Sine, Soft Triangle, Warm Drive, Hard Clip, Wave Fold, Odd Stack, Saw Rise, Hardcore Punch, Rumble Core), a wavetable display, and loading your own wavetable from a WAV (Serum and Vital format), saved inside the project. See `docs/specs/oscillator-wavetable.md`, `wavetable-factory.md`, `wavetable-user.md`, `wavetable-display.md`.
- **Editable amp and pitch envelopes** with a default Kick + Bass shape: drag points, add and delete extra points, bend segments, five knobs per envelope, ready-made shapes, a Time Scale knob and tempo Sync (host tempo, or the BPM control in the Standalone). See `docs/specs/envelope-breakpoint.md`.
- **Punch / Tail**: a Split time that divides the kick in two and a Punch Level and a Tail Level (-48 to +12 dB) for each part. See `docs/specs/voice-punch-tail.md`.
- **Tock**, an optional click layer at the start of every kick: Synth mode or Sample mode with four built-in sounds (Click, Knock, Snap, Tick) or your own WAV (up to 2 seconds), with Level, Tune, Start, Decay and Tone. See `docs/specs/tock.md`.
- **Effects** you can drag into any order, each with On (off by default) and Mix, never clicking and never shifting the kick's timing: **Distortion** (Soft Clip, Hard Clip, Overdrive, Foldback, Bitcrush, oversampled by the Quality setting), a 4-band **parametric EQ** (each band a Bell, Low or High Shelf, Low or High Cut, with a response display and draggable dots) and a multiband **Stereo** effect with a Mono Below switch. A global **Quality** setting (1x to 8x, default 2x) sets the oversampling. See `docs/specs/fx-*.md`.
- **Presets**: five factory presets (Init, Classic Punch, Rumble Kick, Hardcore Crunch, Gabber Fold), your own saved as `.fcmpreset` files, previous and next buttons, and a name field with Save. A project or preset stores everything, including your own wavetables and Tock sample. See `docs/specs/presets.md` and `project-state.md`.
- **One-screen editor** that follows the agreed layout and scales with the window without rearranging. See `docs/specs/ui-grid-layout.md`.
- **Standalone only**: an on-screen keyboard (also playable with the computer keyboard) and an Auto-play loop with a BPM control to hear the kick without a DAW.
- Saved projects and presets are treated as untrusted input: unusable values are replaced by defaults, a state from a newer version loads as far as it is understood, and damaged files are refused with a message. See `docs/specs/state-safe-loading.md`.
- Continuous integration on Linux, macOS and Windows (build, unit tests, pluginval at strictness 10, formatting), pinned and verified dependencies, and a release workflow that builds the three systems when a version tag is pushed.

[Unreleased]: https://github.com/nocturn444/frenchcore-machine/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/nocturn444/frenchcore-machine/releases/tag/v0.1.0
