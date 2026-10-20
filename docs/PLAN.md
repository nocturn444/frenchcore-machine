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

- [x] UI layout matches `UI_LAYOUT.md` ([spec](specs/ui-grid-layout.md); pending the product owner's eyes)
- [x] Manual test pass: done in FL Studio only (owner's decision, 2026-10-13: no other DAW will be tried). Not tried: parameter automation, other sample rates, the Quality setting. See [TESTING.md](TESTING.md) and the status of the checks
- [x] CPU profiling (about 3 % of one core at 8x with all effects on, 1.6 % at the default 2x: [TESTING](TESTING.md)); no allocation on the audio thread: checked by unit tests for every effect and for the effect chain; the oscillators, envelopes and voice follow the real-time rules by design but have no allocation test of their own yet
- [x] User guide complete (installation added; every feature has its section)
- [x] Tag `v0.1.0`, GitHub Release with binaries for all platforms (published by the owner on 2026-10-14; the workflow and steps are in [RELEASING](RELEASING.md))

## 0.1.1: security and finishing fixes

Findings of the security audit and the review of the 0.1.0 release (2026-10-14) that did not need the owner.

- [x] JUCE 9.0.4 (JUCE's security release for malformed UTF-8 text)
- [x] Host state checked before parsing (UTF-8, no DTD or entities, nesting limit); embedded audio decoded only with an honest size ([state-safe-loading](specs/state-safe-loading.md))
- [x] Release workflow: read-only builds, `SHA256SUMS.txt` and provenance attestations, releases only from `main`; zips with `LICENSE.txt` and `README.txt`, no Windows build leftovers, stripped Linux binaries; release notes with the known limitations ([RELEASING](RELEASING.md))
- [x] AU validated with `auval` in CI
- [x] Licence notice in the window ([legal-notice](specs/legal-notice.md))
- [ ] Merged and released by the owner (`v0.1.1`)

## Version 0.2.0: mixer, more effects, noise, new Tock, extreme EQ

Requested by the product owner on 2026-10-16 (five items: the missing effects, a Serum 2 style mixer with effect copies and per-part/per-source effects, a noise oscillator like Serum's, a rethought Tock, and extreme EQ peaks for microwave and hardtekk kicks). One version, built in stages; each stage is one pull request (stage 4 is one pull request per effect) that the owner reviews and merges. Version 0.1.1 (security fixes) is merged and is the base.

### Decisions of the product owner (2026-10-16)

| Topic | Decision | Spec |
|---|---|---|
| Release | One version, 0.2.0, in stages | this plan |
| Compatibility | Every 0.1 project and preset (factory and user) opens and sounds the same | [mixer-and-buses](specs/mixer-and-buses.md), ADR 0010 |
| New effects | Filter (with Drive), Compressor with a Multiband switch (off: a normal compressor; on: OTT-style, with OTT's controls), Transient shaper, Reverb (one simple room) | [fx-filter](specs/fx-filter.md), [fx-compressor](specs/fx-compressor.md), [fx-transient](specs/fx-transient.md), [fx-reverb](specs/fx-reverb.md) |
| Mixer | Sources Osc 1, Osc 2, Noise, Tock, each to Main, Bus A or Bus B, with Level, Mute, Solo | [mixer-and-buses](specs/mixer-and-buses.md) |
| Buses | Two buses plus Main; a bus applies to Whole, Punch or Tail using the one existing Split; all the sound enters the bus and outside the chosen part it comes out clean | [mixer-and-buses](specs/mixer-and-buses.md) |
| Effect copies | Up to 4 of each effect; add, remove, duplicate, reorder | [mixer-and-buses](specs/mixer-and-buses.md), ADR 0010 |
| Window | Must still fit the owner's screen: the mixer may go in a tab (agent's choice: tabs in the Effects box); the window does not grow | [ui-grid-layout](specs/ui-grid-layout.md) |
| Noise | Eight generated noises plus your own WAV; follows the amp envelope (own envelopes per source may come with a future modulation system); fixed pitch with a Key Track switch, not the pitch envelope; in the free LFO box | [noise-oscillator](specs/noise-oscillator.md) |
| Tock | Keep Synth mode and the four sounds; add trimming of any sample with handles and a Detect button, a Pogo knob (bipolar pitch bend over the played part), samples up to 10 s; it no longer goes through the amp envelope (0.1 states keep the old behaviour) | [tock](specs/tock.md) section 8 |
| EQ | An Extreme switch per band (±36 dB, Q up to 50) and a Key Track per band that follows the MIDI note like Serum's filters; loud peaks are left to the safety limiter | [fx-eq](specs/fx-eq.md) section 8 |

The owner confirmed questions 3.4 (noise in the LFO box) and 4.5 (Tock samples up to 10 seconds) on 2026-10-16.

### Stages

- [x] **0. Documentation**: the specs above, [ADR 0010](decisions/0010-effect-instances-buses-and-mixer.md), [ADR 0011](decisions/0011-effect-interface-additions.md), architecture, layout and this plan.
- [x] **1. Effect copies and racks (Main only).** First commit: reference renders of 0.1.1 (every factory preset and a test project with the Tock on, one note each) as test data, so every later stage can prove nothing changed. Then the interface additions of ADR 0011 (`EffectContext`, `shownWhen`, `defaultMix`, 48 parameters; Bits and Rate of the Distortion shown only for Bitcrush), the instance pool (4 per type, IDs per ADR 0010), `core::FxRackState` with the three racks, state version 2 and the migration from 1, and the Main rack UI with "+ Add effect", duplicate, remove, drag and scrolling.
- [x] **2. Mixer and buses.** Voice stems, the mixer parameters, Bus A and Bus B with Level and Apply To, the latency model of ADR 0010, and the Effects box tabs Mixer / Bus A / Bus B / Main.
- [x] **3. EQ: Extreme and Key Track** ([fx-eq](specs/fx-eq.md) section 8).
- [x] **4. New effects**, one pull request each, in this order: Filter (done), Transient shaper (done), Compressor (single band and Multiband) (done), Reverb (done). Each follows [ADDING_AN_EFFECT](ADDING_AN_EFFECT.md) and needs no change outside its folder, its registration line, its tests and docs (if it does, that is a defect of stages 1 and 2 to fix first).
- [x] **5. Noise oscillator** ([noise-oscillator](specs/noise-oscillator.md)), in the LFO box, with its mixer channel.
- [x] **6. Tock rework** ([tock](specs/tock.md) section 8): first measure and record why it is hard to hear; then Amp Env, Trim with the waveform display, Detect, Pogo, 10-second samples; the Hardcore Crunch factory preset gets `tock.ampEnv` on so it sounds as in 0.1.
- [x] **7. Wrap-up.** Every 0.1 factory preset and the test project still render the same as the references of stage 1 (the tolerance of [mixer-and-buses](specs/mixer-and-buses.md) section 5); CPU measured again with every rack full ([TESTING](TESTING.md)); user guide, README (EN and ES) features, CHANGELOG; version 0.2.0 in `CMakeLists.txt`; release notes draft in `.github/release-notes.md`. The owner tags and publishes.

Done when (for the whole version):

- Every spec above is implemented and its tests pass; CI is green on Windows, macOS and Linux; pluginval at strictness 10 and auval pass.
- A 0.1.0 or 0.1.1 project, every 0.1 factory preset and a 0.1 user preset file open and render the same audio as 0.1.1 (aligned by the reported latency, every sample within 1e-6 of the reference, which is -120 dBFS: the stems are summed in a different order, so exact bit equality is not required), checked by tests.
- No allocation on the audio thread in any new DSP (counting `operator new` tests, as for the 0.1 effects), and the voice stems and racks have allocation tests too.
- The window keeps its 0.1 size and shape.
- `docs/USER_GUIDE.md`, `CHANGELOG.md`, `docs/STATUS.md` and this plan are updated in each stage's pull request.

## Version 0.3.0: Peak, Formant, Phaser, Flanger and Volume LFO

Requested by the product owner on 2026-10-20: five new effects, a volume LFO like LFOTool, a phaser, a flanger, a formant filter and a "peak" like Nimble Kick's. Version 0.2.0 is released and is the base. One version, built in stages; each stage is one pull request that the owner reviews and merges. The questions and answers are summarised below; the specs hold the details.

### Decisions of the product owner (2026-10-20)

| Topic | Decision | Spec |
|---|---|---|
| Release | A new version, 0.3.0, one pull request per effect (the agent's recommendation; the owner left the version and the order to the agent) | this plan |
| Compatibility | Every 0.1 and 0.2 project and preset opens and sounds the same (project rule) | [ADR 0012](decisions/0012-effect-note-triggers-curves-and-decorations.md) |
| LFOs of the Phaser, Flanger and Volume LFO | Restart at every note, so every kick sounds the same (G.1 A); a Rate knob in Hz with a Sync switch for note values (G.2 A) | [ADR 0012](decisions/0012-effect-note-triggers-curves-and-decorations.md) |
| Volume LFO | Mainly pumping like a sidechain, also creative gating: "after a lot of distortion and reverb the kick becomes a uniform mass and loses its syncopation" (1.1); a shape drawn with points plus ready-made shapes (1.2 A); Depth, Smooth, Mix (1.3 A); the shape opens larger inside the Effects box (1.4 A) | [fx-volumelfo](specs/fx-volumelfo.md) |
| Phaser | Stages, Frequency, Rate, Depth, Feedback, Mix (2.1 A) and Key Track (2.2 A) | [fx-phaser](specs/fx-phaser.md) |
| Flanger | Delay, Rate, Depth, Feedback (positive or negative), Mix (3.1 A) and Key Track (3.2 A) | [fx-flanger](specs/fx-flanger.md) |
| Formant filter | Its own effect (4.1 A): Vowel, Morph, Shift, Resonance, Drive, Mix (4.2 A); Shift does not follow the note (4.3 A) | [fx-formant](specs/fx-formant.md) |
| Peak | Like Nimble Kick's Kick FX: a boosted zone into a soft-to-hard clipper (5.1 A), with an XY pad plus knobs (5.2 A) | [fx-peak](specs/fx-peak.md) |
| Default Mix | 100 % for all five (6.1 B) | each spec |
| Factory presets | No new ones for now; the owner makes presets after trying the effects (6.2 A) | |

### Stages

- [x] **0. Documentation**: the five specs, [ADR 0012](decisions/0012-effect-note-triggers-curves-and-decorations.md), architecture, the effect guide and this plan.
- [x] **1. Groundwork ([ADR 0012](decisions/0012-effect-note-triggers-curves-and-decorations.md)), no change in sound.** First commit: reference renders of 0.2.0 as test data (every factory preset, plus a test project that uses Bus A with Apply To Tail, Bus B and every 0.2 effect), checked like the 0.1.1 ones ([mixer-and-buses](specs/mixer-and-buses.md) §5). Then note triggers through the racks (with timing tests at every Quality and rack position), `dsp::TempoLfo`, effect curves (`EffectCurve`, `core::EffectCurveState`, the `EffectCurves` node, state version 3 with an empty migration from 2, add/duplicate rules, hostile-input tests) and the decoration context with the larger editor over the rack. Tested with test-only effects; no user-visible change.
- [x] **2. Peak** ([fx-peak](specs/fx-peak.md)), with the XY pad.
- [x] **3. Formant filter** ([fx-formant](specs/fx-formant.md)).
- [x] **4. Phaser** ([fx-phaser](specs/fx-phaser.md)).
- [x] **5. Flanger** ([fx-flanger](specs/fx-flanger.md)).
- [x] **6. Volume LFO** ([fx-volumelfo](specs/fx-volumelfo.md)), with the shape picture and the shape editor.
- [x] **7. Wrap-up.** The 0.1.1 and 0.2.0 references still pass; CPU measured again with every rack full ([TESTING](TESTING.md)); user guide, README (EN and ES) features, CHANGELOG; version 0.3.0 in `CMakeLists.txt`; release notes draft in `.github/release-notes.md`. The owner tags and publishes.

Stages 2 to 6 each follow [ADDING_AN_EFFECT](ADDING_AN_EFFECT.md) and need no change outside the effect's folder, its registration line, its decoration entry (Peak and Volume LFO), its tests and docs; if one does, that is a defect of stage 1 to fix there first.

Done when (for the whole version):

- Every spec above is implemented and its tests pass; CI is green on Windows, macOS and Linux; pluginval at strictness 10 and auval pass.
- Every 0.1.1 and 0.2.0 reference render still matches (same tolerance as in 0.2), so old projects and presets sound the same.
- No allocation on the audio thread in any new DSP or in the note-trigger and curve paths (counting `operator new` tests).
- The window keeps its size and shape.
- `docs/USER_GUIDE.md`, `CHANGELOG.md`, `docs/STATUS.md` and this plan are updated in each stage's pull request.

## After 0.3 (not scheduled)

- Visual design pass (custom LookAndFeel, possibly WebView UI).
- CLAP format.
- Two LFOs and a modulation system (wavetable position, FX parameters), including separate envelopes per oscillator or source like Serum (owner's note, 2026-10-16). The LFO box now holds the Noise panel, so the LFOs need a new place in the window.
- Signed and notarized installers.

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Aliasing from very high pitch-envelope sweeps and heavy distortion | Harsh, unmusical sound | Mipmapped wavetables, oversampled distortion, tests that measure aliasing. |
| Real-time bugs (clicks, dropouts) | Unusable in a session | Strict threading rules, allocation guard, pluginval. |
| AU/macOS-specific build issues without a Mac at hand | Broken macOS builds | macOS runner in CI on every PR. |
| Scope creep in the FX section | MVP slips | New effects only after v0.1.0; the registry keeps adding them cheap later. |
