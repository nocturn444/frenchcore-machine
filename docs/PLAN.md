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

## Phase 0: Foundation (documentation and repository)

- [x] Initial brief and product decisions
- [x] Core documentation: plan, architecture, MVP scope, README (EN/ES), contributing, license, changelog
- [x] Project system prompt for agents (`CLAUDE.md`)
- [x] GitHub templates (PR, issues) and cloud session setup
- [~] UI layout spec from the product owner's sketch (`docs/UI_LAYOUT.md`)

## Phase 1: Skeleton that builds everywhere

Goal: an empty plugin that loads in a DAW on all three platforms.

- [ ] CMake project with CPM, pinned JUCE 9 and Catch2
- [ ] Libraries `fcm_dsp`, `fcm_core`, `fcm_ui` and the plugin target (VST3, AU, Standalone)
- [ ] Empty processor (silence) and empty resizable editor
- [ ] CI: build + unit tests + pluginval on Windows, macOS, Linux; artifacts uploaded
- [ ] clang-format check in CI
- [ ] `docs/BUILDING.md` verified against a clean checkout

## Phase 2: First sound

Goal: a MIDI note plays a pitched kick from a built-in sine table.

- [ ] Spec: voice and note handling (mono, retrigger, key track, glide, velocity)
- [ ] Spec: wavetable oscillator (format, mipmaps, interpolation)
- [ ] `WavetableData` + mipmap builder + oscillator, with tests
- [ ] Voice + note handler, with tests
- [ ] Fixed amp and pitch envelopes (hard-coded default shapes)
- [ ] Output gain + safety limiter
- [ ] Parameters wired through APVTS; basic generic UI

## Phase 3: Envelopes

- [ ] Spec: breakpoint envelope model and editor interactions
- [ ] `BreakpointEnvelope` model + real-time renderer, with tests
- [ ] Lock-free publishing of envelope shapes to the audio thread
- [ ] Envelope editor component (add/move/delete points, curvature)
- [ ] Shape presets including the default "Kick + Bass"

## Phase 4: Wavetables

- [ ] Factory wavetable set designed with the product owner
- [ ] User wavetable loading (background thread, validation, error messages)
- [ ] Wavetable embedded in plugin state
- [ ] Wavetable display with position indicator
- [ ] Tock sampler (factory and user samples)

## Phase 5: FX system

- [ ] Spec + ADR: effect interface, registry, chain model (ADR 0003 already drafted)
- [ ] `Effect` interface, `EffectRegistry`, `EffectChain` with lock-free reorder, with tests
- [ ] Distortion (5 types, oversampling)
- [ ] 3-band EQ
- [ ] Multiband stereo
- [ ] FX rack UI with drag-and-drop reordering, enable and mix per slot
- [ ] `docs/ADDING_AN_EFFECT.md` validated by following it for one of the effects

## Phase 6: State and presets

- [ ] Full state save/load including non-parameter state
- [ ] State versioning and migration framework
- [ ] Preset format, factory presets, user preset save/load, browser
- [ ] Round-trip tests for state and presets

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
- Modulation of wavetable position and FX parameters by envelopes.
- Signed and notarized installers.

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Aliasing from very high pitch-envelope sweeps and heavy distortion | Harsh, unmusical sound | Mipmapped wavetables, oversampled distortion, tests that measure aliasing. |
| Real-time bugs (clicks, dropouts) | Unusable in a session | Strict threading rules, allocation guard, pluginval. |
| AU/macOS-specific build issues without a Mac at hand | Broken macOS builds | macOS runner in CI on every PR. |
| Scope creep in the FX section | MVP slips | New effects only after v0.1.0; the registry keeps adding them cheap later. |
