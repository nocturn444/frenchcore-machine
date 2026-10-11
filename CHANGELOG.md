# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Initial project documentation: plan, architecture, MVP scope, ADRs, spec template, contributing guide, bilingual README.
- AGPL-3.0 license.
- Agent instructions (`CLAUDE.md`), GitHub issue and PR templates, and a setup hook for Claude Code cloud sessions.
- Build system (CMake + CPM, JUCE 9.0.3, Catch2) producing VST3, AU and Standalone targets.
- Plugin skeleton: silent engine, output gain parameter, resizable editor showing the agreed layout grid.
- CI on Linux, macOS and Windows: build, unit tests, pluginval at strictness 10, clang-format check, downloadable builds.
- First sound: a MIDI note plays a monophonic kick. It starts with a fast pitch drop (+48 semitones in 90 ms) and settles on the note's pitch, with an exponential amplitude decay. Letting go of the last key fades the kick out in 30 ms, so the note length sets the tail length. Retriggering and releasing do not click.
- Voice controls: Key Track with a fixed Root Note, optional Glide with Glide Time, Velocity Sensitivity (0 % by default, so every kick is equally loud).
- Two wavetable oscillators (On, Level, Pitch, Octave, Fine, Position, Phase) using band-limited mipmapped wavetables; the only wavetable for now is a built-in sine.
- Safety Limiter (soft clipper, on by default) after Output Gain; the output never goes above 0 dBFS while it is on.
- Generic parameter panel in the editor (one control per parameter) until the real panels land.
- Standalone only: an on-screen keyboard (also playable with the computer keyboard, `Z`/`X` shift the octave) and an Auto-play loop with a BPM control (default 200) to hear the kick without a DAW. Not shown in VST3/AU. See `docs/specs/standalone-audition.md`.
- Specs for voice and note handling and for the wavetable oscillator, and ADR 0005 (engine parameter hand-off).
- `fcm_core_tests`: tests for the parameter layout (including a guard on released parameter IDs) and the parameter-to-engine mapping.

- Editable envelopes for volume (amp) and pitch: a display where you drag points, double-click to add or delete extra points and drag a segment to bend it, five knobs per envelope that move the main points of the same line, and a shape menu (Kick + Bass, Short Punch, Long Tail for the amp; Classic Drop, Short Punch, Long Tail for the pitch). The default amp shape is now Kick + Bass: a punch, a dip and a bass body held while the key is down.
- New automatable parameters: Amp Env Time Scale, Pitch Env Time Scale, Pitch Env Amount (default +48 semitones), Punch Tail Balance and Punch Tail Split (default 80 ms).
- Punch / Tail balance: favours the hit or the bass tail with one control, with a marker for the split time on the amp display.
- Envelope shapes are saved with the project and restored with it; a damaged shape falls back to the default.
- ADR 0006 (envelope model and lock-free publishing), specs for breakpoint envelopes and for Punch / Tail.

### Changed

- The editor shows the generic parameter panel instead of the layout placeholder grid; the layout itself stays documented in `docs/UI_LAYOUT.md`.
- The plugin reports the kick's tail length to the host (the amp release time).
- The kick now holds its body while the key is down (the default Kick + Bass shape has a sustain level) and fades out on release; the fixed Phase 2 shapes are gone.

[Unreleased]: https://github.com/nocturn444/frenchcore-machine/commits/main
