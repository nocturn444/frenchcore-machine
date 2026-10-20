# Spec: Project state (save and load everything)

- Status: Implemented (Phase 6, first part)
- Owner: nocturn4 (product), implemented by agent session
- Related: [state-safe-loading](state-safe-loading.md), [ARCHITECTURE §5, §12](../ARCHITECTURE.md), [PLAN Phase 6](../PLAN.md)

## 1. What it does for the user

Saving a DAW project and opening it again brings back **exactly** the sound you had, including everything that is not a plain knob:

- all the knobs and switches (the parameters);
- the two envelope shapes (and their tempo sync);
- the order of the effects;
- a wavetable you loaded for either oscillator and a Tock sample you loaded, embedded in the project.

New in this part: a saved state now describes **everything**. A control that is missing from a saved state (for example, a project saved by an older version that did not have the Tock yet, or a preset that only mentions some controls) goes back to its default value instead of keeping whatever it had before. Loading a project, a preset or another state therefore always gives the same result, whatever was set before.

Nothing changes for the user otherwise; this is the foundation of the presets of the next parts.

## 2. User interface

None.

## 3. Parameters

None.

## 4. DSP / implementation approach

- `core::ProjectState` (`src/core/ProjectState.h`) gathers the pieces of the plugin's state that are not the audio engine: the parameter tree, the two `EnvelopeState`s, the `FxRackState`, the two `WavetableSelector`s and the `TockSelector`. It has two operations, which are all the processor and the presets need:
  - `capture()` returns the whole state as one `ValueTree` (the parameters plus the nodes `AmpEnvelope`, `PitchEnvelope`, `FxRacks` (`FxChain` in version 1), `EffectCurves` (from state version 3), `UserWavetable1/2` and `UserTockSample` that exist), with `stateVersion` always written;
  - `restore(tree)` loads a tree into everything, as untrusted input ([ARCHITECTURE §12](../ARCHITECTURE.md)): the parameters through `core::loadState`, then each node through its own `restoreFrom`, which falls back to a default or to "nothing" for a missing or damaged node. A rejected tree (not our state) changes nothing.
  `toXml()` and `restoreFromXml()` do the same for the XML form that the host stores.
- `PluginProcessor::getStateInformation` and `setStateInformation` become thin: size limit, XML parse, `ProjectState`.
- **Missing parameters become defaults.** `core::loadState` adds an entry with the default value for every parameter that is absent from the saved tree before the state is applied. (This is why the old behaviour of keeping the current value changes: JUCE's `replaceState` leaves absent controls as they were.)
- Each piece is restored in the order parameters, envelopes, effect order, Tock sample, user wavetables, and then the selectors build their tables, as before.

## 5. Tests

Catch2 (`fcm_core_tests`):

- Round trip: set every kind of state away from its default (a few parameters of every group, both envelope shapes edited with sync on, a new effect order, a user wavetable on each oscillator, a Tock sample), `capture()`, write to XML text, parse it, `restore()` into a fresh set of pieces, `capture()` again: the two trees are equivalent, and so is a second round trip.
- The XML form round-trips too (`toXml()` then `restoreFromXml()`), with `stateVersion` present.
- A state with no `stateVersion`, a newer one and a damaged one load as in the state-safe-loading spec.
- Missing parameters become defaults: load a state that mentions only two parameters into an instance where many others are away from their defaults: those two take the saved values and every other parameter is its default; a project from before the Tock (no `tock.*` entries, no `UserTockSample`) loads with the Tock at its defaults.
- Missing nodes: no `AmpEnvelope` gives the default amp shape, no `FxRacks` gives the default racks (Distortion, EQ and Stereo in Main, the buses empty), no user nodes give no user wavetable or Tock sample, even if a previous state had them.
- A damaged node (an envelope with NaN, an order with unknown IDs, a wavetable text that is not base64) never fails the load of the rest.
- A rejected state (wrong root, empty tree) changes nothing at all.

## 6. Out of scope

- Presets and their files (the next parts of Phase 6).
- Saving the audition (Standalone) settings, the window size or the selected tab.

## 7. Decisions taken

1. A saved state is complete: absent parameters become defaults.
2. The pieces of the state are gathered in one class so the processor, the presets and the tests share one load path.
