# 0012: Effects get note triggers, a drawable curve and interactive decorations

- Status: Accepted
- Date: 2026-10-20
- Extends: [0009](0009-effect-parameters-and-chain-latency.md), [0010](0010-effect-instances-buses-and-mixer.md), [0011](0011-effect-interface-additions.md)

## Context

Version 0.3 adds five effects ([PLAN](../PLAN.md), "Version 0.3.0"). Three of them need things the 0.2 `Effect` interface cannot give:

- The Phaser, the Flanger and the Volume LFO have an LFO that must **start from zero at every note**, so every kick sounds the same (the owner's answer G.1 A of 2026-10-20). An effect sees neither MIDI nor note-ons, and its input is delayed by the latency of the effects before it and, in Main, by the alignment with the buses, so "the note-on" is not at the same sample for every effect.
- The Volume LFO has a **shape drawn with points** (answer 1.2 A), one per copy. It is not a parameter (a list of up to 32 points cannot be automated or described by `ParameterSpec`), so it is state that must be saved, duplicated, reset and published to the audio thread like the envelopes.
- The Peak has an **XY pad** that moves two of its parameters (answer 5.2 A), and the Volume LFO's shape opens in a **larger editor inside the Effects box** (answer 1.4 A). Decorations ([fx-rack](../specs/fx-rack.md), `src/ui/EffectDecorations.cpp`) are pictures today and only get the APVTS, the instance ID and the note.

## Decision

1. **Note triggers.** `Effect` gains `virtual void noteOn(int sampleOffset) noexcept` (default: ignored), called before `process()` once for every note-on whose sound reaches this effect inside the coming buffer, in order, `sampleOffset` counted from the buffer's first sample.
   - The engine records the position of every note-on (a running sample counter of the voice) in a fixed ring of 16 entries; nothing allocates.
   - A rack delivers a note-on to the instance at position `p` when the voice's output of that sample reaches it: the delay is the rack's input delay (0 for Bus A and Bus B; the pool latency for Main, which is delayed to line up with the buses, [ADR 0010](0010-effect-instances-buses-and-mixer.md)) plus the latencies of the members before `p` at the current Quality. The rack keeps, per position, the triggers still in flight; the latencies are constant per Quality ([ADR 0009](0009-effect-parameters-and-chain-latency.md)), so the delay is exact.
   - Every note-on counts, including legato notes with glide and the Standalone's keyboard and auto-play (they are MIDI). Before the first note an LFO runs from its `reset()` state.
   - Instances that are off or asleep get no triggers; one that wakes up is reset, as today.
2. **A shared tempo LFO helper**, `dsp::TempoLfo` in `src/dsp/fx/`: a phase from 0 to 1 that restarts on `noteOn`, advances at `Rate` Hz, or at one cycle per musical figure at the context's tempo when `Sync` is on. The figure list is the envelopes' `kMusicalFigures` (1/64 to 1 bar), copied into a frozen choice list for the effects so a later change of the envelope list cannot change saved values. The tempo is read once per block (`EffectContext::tempoBpm`). Phase restarts, not the DAW's bar position, set where a cycle starts: no host transport is needed.
3. **Effect curves.** An effect may own one drawable curve per copy:
   - `dsp::EffectCurve` (`src/dsp/fx/EffectCurve.h`): an immutable fixed-size struct of 2 to 32 points `{x, y, curve}` with `x` in 0..1 (one cycle), `y` in 0..1 and the segment curvature of the envelopes (`curveProgress`, [envelope-breakpoint](../specs/envelope-breakpoint.md)); the first point is at `x = 0` and the last at `x = 1`.
   - `Effect` gains `virtual const EffectCurve* defaultCurve() const noexcept` (default `nullptr`: no curve), `virtual std::span<const NamedCurve> curvePresets() const noexcept` (static data: the ready-made shapes of the editor's menu) and `virtual void setCurve(const EffectCurve* curve) noexcept`, called once per block before `process()` (`nullptr` means the default).
   - `core::EffectCurveState` keeps one editable curve per instance of every effect that has one, publishes each through a `PublishedPointer` (the envelope mechanism, [ADR 0006](0006-envelope-model-and-publishing.md)) and announces changes to the UI. `EffectPool` holds the sources and acquires each once per block.
   - Saved as a child node `EffectCurves` with one `Curve` child per instance (`instance="volumelfo2"`) holding `Point` children (`x`, `y`, `curve`). Every curve is saved (a saved state describes everything); a missing curve is the default. Loading is untrusted input ([state-safe-loading](../specs/state-safe-loading.md)): an unknown instance is dropped; a curve with fewer than 2 or more than 32 points, an `x` outside 0..1 or out of order, ends not at 0 and 1, or a non-finite number is replaced by the default.
   - `FxRackState`: **add** resets the instance's curve to the default (as it resets the parameters), **duplicate** copies it. Remove leaves it (the next add resets it).
   - **State version 3.** The migration from 2 adds nothing (no curve means the default). A 0.2 build opening a 0.3 project ignores the node, as for any newer state.
4. **Decorations get a context and may be interactive.** The decoration factory receives a `DecorationContext` (the APVTS, the instance ID, the note getter, the `EffectCurveState` and a callback to show a larger editor over the rack). A decoration may change parameters through the APVTS (with begin/end gesture, so the DAW records automation) and curves through `EffectCurveState`. The Effects box shows at most one larger editor at a time over the rack of the current tab, with a **Done** button; Escape and switching tabs close it. Adding an effect with a decoration is still one entry in `EffectDecorations.cpp`; the rack code does not change.

## Consequences

- Effects without these features compile and behave unchanged: the new virtuals default to doing nothing and no curve.
- 0.2 projects load unchanged (migration 2 to 3 is empty); the 0.2.0 reference renders guard it.
- A retriggered LFO makes every kick identical even with effects of different latency before it, at every Quality and in every rack, which is checked by tests (a note-on lands on the sample where the click reaches the effect).
- An LFO cannot follow the DAW's bar position. A free-running or bar-locked mode would need host transport in `EffectContext`; it was not asked for.
- Curves are not automatable. Automation of the shape would need a modulation system, which is planned separately (PLAN, "After 0.3").
