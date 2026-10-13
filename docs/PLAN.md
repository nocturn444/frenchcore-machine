# Development Plan

This is the roadmap for Frenchcore Machine from an empty repository to the MVP (v0.1.0) and beyond. Each phase ends with something that can be built, tested and, from Phase 2 on, heard in a DAW.

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done. Keep this file updated in the same PR that changes a status.

## How work flows (documentation-driven development)

Every feature follows the same order:

1. **Spec first.** Write or update a spec in `docs/specs/` using [the template](specs/_TEMPLATE.md): what it does for the user, parameters with ranges and defaults, DSP approach, tests that will prove it works. The product owner reviews user-facing behaviour.
2. **Architecture check.** If the feature changes module boundaries, threading or state format, update [ARCHITECTURE.md](ARCHITECTURE.md) and add an ADR in `docs/decisions/`.
3. **Tests, then code.** Write the unit tests described in the spec, then implement until they pass.
4. **Docs closing.** Update the user guide, `CHANGELOG.md` (Unreleased section) and this plan's status.
5. **Pull request.** One feature per PR, CI green, checklist in the PR template completed.

A pull request that adds behaviour without updating the relevant docs is not mergeable.

### Starting a phase in a new session

Each phase is self-contained, so it can be picked up by a fresh agent session (for example a Claude Code session on claude.ai/code) with no prior context: read `CLAUDE.md`, then this plan, then `docs/ARCHITECTURE.md`, `docs/MVP_SCOPE.md` and `docs/UI_LAYOUT.md`, and work through the first unchecked phase. Its "Done when" list is the acceptance criteria. Sound and behaviour questions go to the product owner before implementation.

## Phase 0: Foundation (documentation and repository)

- [x] Initial brief and product decisions
- [x] Core documentation: plan, architecture, MVP scope, README (EN/ES), contributing, license, changelog
- [x] Project system prompt for agents (`CLAUDE.md`)
- [x] GitHub templates (PR, issues) and cloud session setup
- [x] UI layout spec from the product owner's sketch (`docs/UI_LAYOUT.md`)

## Phase 1: Skeleton that builds everywhere

Goal: an empty plugin that loads in a DAW on all three platforms.

- [x] CMake project with CPM, pinned JUCE 9 and Catch2
- [x] Libraries `fcm_dsp`, `fcm_core`, `fcm_ui` and the plugin target (VST3, AU, Standalone)
- [x] Empty processor (silence) and resizable editor showing the agreed layout grid
- [x] CI: build + unit tests + pluginval on Windows, macOS, Linux; artifacts uploaded
- [x] clang-format check in CI
- [x] `docs/BUILDING.md` verified against a clean checkout

## Phase 2: First sound

Goal: a MIDI note plays a pitched kick from a built-in sine table.

- [x] Spec: voice and note handling (mono, retrigger, key track, glide, velocity)
- [x] Spec: wavetable oscillator (format, mipmaps, interpolation)
- [x] `WavetableData` + mipmap builder + oscillator, with tests (two instances: osc1, osc2)
- [x] Voice + note handler, with tests
- [x] Fixed amp and pitch envelopes (hard-coded default shapes)
- [x] Output gain + safety limiter
- [x] Parameters wired through APVTS; basic generic UI
- [x] Standalone-only audition strip: on-screen keyboard and auto-play loop (requested by the product owner during Phase 2 review; [spec](specs/standalone-audition.md))

Done when:

- A MIDI note in the Standalone and in a DAW plays a kick: sine oscillator, fast pitch drop from about +48 semitones down to the note's pitch, amplitude decay. No clicks on retrigger.
- With key track on, the tail follows the MIDI note; with it off, it stays at a fixed root.
- Glide, when enabled, slides the tail pitch between consecutive notes.
- Output never exceeds 0 dBFS with the safety limiter on.
- New DSP has Catch2 tests (pitch accuracy within 1 cent at the end of the pitch drop, finite output, deterministic reset); CI is green on all three platforms; pluginval passes.
- Both specs are in `docs/specs/`, and `CHANGELOG.md`, `docs/USER_GUIDE.md` and this plan are updated.

## Phase 3: Envelopes

- [x] Spec: breakpoint envelope model and editor interactions
- [x] `BreakpointEnvelope` model + real-time renderer, with tests
- [x] Lock-free publishing of envelope shapes to the audio thread
- [x] Envelope editor component (add/move/delete points, curvature)
- [x] Shape presets including the default "Kick + Bass"
- [x] AHDSR / pitch knobs bound to the main envelope points
- [x] Punch/Tail split and levels, with tests (the Balance knob was removed on 2026-10-13)

## Phase 4: Wavetables

- [x] Factory wavetable set (nine generated tables and the Wavetable choice per oscillator, [spec](specs/wavetable-factory.md); the list was chosen by the agent and waits for the product owner's ears)
- [x] User wavetable loading (background thread, validation, error messages; [spec](specs/wavetable-user.md))
- [x] Wavetable embedded in plugin state
- [x] Wavetable display with position indicator ([spec](specs/wavetable-display.md))
- [x] Tock: synth mode and sample mode (factory and user samples, [spec](specs/tock.md))

## Phase 5: FX system

- [x] Spec + ADR: effect interface, registry, chain model ([spec](specs/fx-chain.md), ADR 0003 and 0009)
- [x] `Effect` interface, `EffectRegistry`, `EffectChain` with lock-free reorder, with tests
- [x] Distortion (5 types, oversampling; [spec](specs/fx-distortion.md))
- [x] Parametric EQ, 4 bands ([spec](specs/fx-eq.md))
- [x] Multiband stereo ([spec](specs/fx-stereo.md))
- [x] FX rack UI with drag-and-drop reordering, enable and mix per slot ([spec](specs/fx-rack.md))
- [x] `docs/ADDING_AN_EFFECT.md` validated by following it for the three effects

## Phase 6: State and presets

- [x] Full state save/load including non-parameter state ([spec](specs/project-state.md))
- [x] State versioning and migration framework (hook, safe loading and untrusted-input rules: [spec](specs/state-safe-loading.md); migrations are added when the state format changes)
- [x] Preset format, factory presets, user preset save/load, browser ([presets](specs/presets.md), [preset-browser](specs/preset-browser.md)); the final list of factory presets is for nocturn4 to decide by ear
- [x] Round-trip tests for state and presets

## Phase 7: MVP hardening and release

- [ ] UI layout matches `UI_LAYOUT.md`
- [ ] Manual test pass in at least: Ableton Live, FL Studio, Reaper (see [TESTING.md](TESTING.md))
- [ ] CPU profiling; no allocation on the audio thread (debug guard clean)
- [ ] User guide complete
- [ ] Tag `v0.1.0`, GitHub Release with binaries for all platforms

## After the MVP (not scheduled)

- Visual design pass (custom LookAndFeel, possibly WebView UI).
- More effects (compressor/OTT, filter, transient shaper, reverb).
- CLAP format.
- Two LFOs and a modulation system (wavetable position, FX parameters).
- Signed and notarized installers.

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Aliasing from very high pitch-envelope sweeps and heavy distortion | Harsh, unmusical sound | Mipmapped wavetables, oversampled distortion, tests that measure aliasing. |
| Real-time bugs (clicks, dropouts) | Unusable in a session | Strict threading rules, allocation guard, pluginval. |
| AU/macOS-specific build issues without a Mac at hand | Broken macOS builds | macOS runner in CI on every PR. |
| Scope creep in the FX section | MVP slips | New effects only after v0.1.0; the registry keeps adding them cheap later. |
