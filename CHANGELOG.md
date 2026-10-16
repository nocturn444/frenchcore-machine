# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

To be released as 0.1.1: security fixes and finishing touches after the 0.1.0 review and security audit. No change to the sound, the controls or the project format; projects and presets from 0.1.0 open as before.

### Security

- JUCE updated from 9.0.3 to 9.0.4, JUCE's security release that fixes a memory overrun when building text from malformed UTF-8 (reachable from a crafted DAW project).
- The state a DAW hands to the plugin is checked before it is parsed: it must be valid UTF-8, must not declare a DTD or entities, and may nest at most 64 levels. A crafted project can no longer crash the DAW through the text decoder or by running the XML parser out of stack. Preset files get the same DTD and nesting checks.
- Audio embedded in a project or preset (your own wavetables and Tock sample) is decoded only when its stored size is honest; a crafted size could make the plugin try to allocate gigabytes and crash the DAW. Such audio is now dropped and the rest of the project loads.
- Release workflow: the build jobs only get read access; a separate job attaches the zips with a `SHA256SUMS.txt` and a signed build provenance attestation; releases are refused for commits that are not on `main`; checkouts no longer keep the token.

### Added

- The window shows the version, the licence (AGPL-3.0, no warranty) and where the source code is, in the free "Other" box. See `docs/specs/legal-notice.md`.
- Every release zip carries `LICENSE.txt` and a short `README.txt`.
- CI validates the AU with Apple's `auval` on macOS, and checks the release packaging on every pull request.

### Fixed

- The Windows zip no longer contains the build leftovers `Frenchcore Machine.lib` and `.exp`; the Linux binaries are stripped of their symbol tables.
- The release notes now list the known limitations; the user guide says the Linux build needs glibc 2.38 or newer and that shared projects and presets include your own wavetables and Tock sample.


### Version 0.2.0 (in progress)

Stage 1 of the 0.2 plan (`docs/PLAN.md`): effect copies and racks. Projects and presets from 0.1 open and sound the same (checked by tests against renders made with 0.1.1).

#### Added

- **Effect copies**: every effect has four copies, called Distortion, Distortion 2 and so on (in the window and in the DAW's automation list). The first copy keeps its 0.1 parameter IDs. See `docs/specs/mixer-and-buses.md`.
- **Add, duplicate, remove** effects in the Effects box (**+ Add effect**, **++**, **x**), up to eight effects in a row and four copies of each effect; the row scrolls when it does not fit.
- Controls that only matter for some settings are hidden otherwise (Distortion's Bits and Rate show only for Bitcrush). `docs/decisions/0011-effect-interface-additions.md`.
- Test data: renders of every factory preset and two Tock projects made with 0.1.1 (`tests/data/reference-0.1.1`) that later versions must reproduce.

#### Changed

- The project state is version 2: the effect order is saved as three racks (`FxRacks`; only Main is used until the mixer arrives). Projects from version 1 are migrated when they load. A project saved with 0.2 does not open correctly in 0.1.
- The plugin reports 16 samples of latency at the default Quality (4 in 0.1), because it keeps room for four copies of every effect.

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
