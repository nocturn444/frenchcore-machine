# Spec: Safe state loading

- Status: Implemented (security hardening, see the audit of `main` at 17a8a0a)
- Owner: nocturn4 (product), implemented by agent session
- Related: [ARCHITECTURE §5 and §12](../ARCHITECTURE.md), [oscillator-wavetable](oscillator-wavetable.md)

## 1. What it does for the user

A saved project or preset can never crash the plugin or the DAW, even if the file is damaged or was made on purpose to cause trouble. When something in a saved state does not make sense, the plugin keeps what it can use and falls back to the default for the rest. Nobody sees an error: a bad value just becomes the control's default value.

- A control whose saved value is not a number (`nan`, `inf`, text) goes back to its default.
- A saved value outside the control's range is moved to the nearest allowed value.
- Controls the plugin does not know (for example from a newer version) are ignored.
- A project saved by a newer version of the plugin still opens: the controls this version knows are loaded and the rest is ignored. Saving it again writes the current format, so anything only the newer version understood is lost.
- A file that is not a Frenchcore Machine state, or is absurdly large, is ignored and the plugin keeps its current settings.

## 2. User interface

None.

## 3. Parameters

None. The rules below apply to every parameter without listing them: new parameters are covered automatically.

## 4. DSP / implementation approach

Two layers, so a mistake in one does not matter.

**On load (`core::loadState`, message thread):**

1. Reject anything that is not the plugin's state node, and anything over `kMaxStateBytes` (32 MiB) before it is parsed.
2. Read `stateVersion` (`readStateVersion`): a missing, non-numeric or smaller-than-1 value counts as version 1. A version newer than the plugin's `kStateVersion` is accepted as described above.
3. Run the migrations from the stored version up to `kStateVersion` (`applyMigrations` over the table in `StateMigrations.cpp`, which is empty while `kStateVersion` is 1). Each migration rewrites the state tree from version N to N + 1.
4. Sanitize the parameter values in the tree before JUCE sees them: for each saved `PARAM` entry, drop it when its ID is unknown; read its value strictly (only digits, sign, `.` and `e`; the result must be finite); a bad value becomes the parameter's default; a good one is clamped into the parameter's range and snapped to its step.
5. Replace the state, set `stateVersion` to the current version, and run a second pass over the live parameters: any whose normalised value is not finite or not within 0..1 is reset to its default.

`getStateInformation` always writes `stateVersion`.

**In the engine:**

- `EngineParameterReader::read` replaces any non-finite value with the parameter's default before converting it (integers included, so no conversion of NaN to an integer happens).
- `WavetableOscillator` never indexes outside its table whatever it is given: non-finite or negative frequency becomes 0, the frequency is capped below Nyquist, the mip level is clamped to `0 .. kNumMipLevels - 1`, a non-finite or out-of-range position becomes 0 or is clamped, the phase is forced into `[0, 1)` (also when it comes from a tiny negative value that rounds to exactly 1), and the table index is clamped. Allocation-free.

### 4.1 Envelopes, tempo and tail length (Phase 3 data)

- The seven envelope and Punch/Tail parameters (`ampEnv.timeScale`, `pitchEnv.timeScale`, `pitchEnv.amount`, `punchTail.balance`, `punchTail.split`, `punchTail.punchLevel`, `punchTail.tailLevel`) are ordinary parameters, so `loadState` sanitizes them like the rest, and `EngineParameterReader` falls back to the engine defaults if one still holds a non-finite value.
- The `AmpEnvelope` and `PitchEnvelope` child nodes are restored by `EnvelopeState::restoreFrom` after `loadState` accepted the state (not when it rejected it). A damaged node gives the default shape.
- The tempo that synced envelopes follow is not trusted either: the plugin keeps the last good tempo when the host gives 0, a negative number or a NaN, and `Voice` ignores a tempo or time scale that is not finite. `EnvelopeRenderer::start` treats a non-finite time scale as 1, because `std::max` and `std::clamp` pass a NaN through.
- `getTailLengthSeconds` goes through `core::tailLengthSeconds`, which never returns a non-finite or negative time to the host.

## 5. Tests

Catch2:

- Oscillator (`fcm_dsp_tests`): reset, position and frequency with NaN, +inf, -inf, huge and tiny values (including -1e-20) never read outside the table and give finite output within [-1, 1]; the mip level stays in range for NaN, infinite and negative frequencies. The test suite is also run once under AddressSanitizer and UndefinedBehaviorSanitizer when the change is made.
- State loading (`fcm_core_tests`): for every float parameter, a state with `nan`, `inf`, `-inf`, text, out-of-range and empty values loads, gives parameters that are finite and inside their range (the default for non-numbers, the nearest bound for out-of-range values), and a note played through the reader and the engine gives only finite samples. At least `osc1.phase`, `osc1.fine`, `osc1.position` and `global.glideTime` are named explicitly.
- A state without `stateVersion` loads as version 1; a future `stateVersion` loads the known controls and ignores the rest; a non-numeric or negative `stateVersion` counts as 1; `stateVersion` is written back after loading and on saving.
- Unknown parameter IDs are ignored; a wrong root node, an empty or oversized state is rejected and leaves the current settings alone.
- Migrations: `applyMigrations` runs the steps in order from the stored version to the target (tested with injected steps), does nothing when the version is already current, and reports a missing step.

- Envelope robustness (`fcm_dsp_tests`): an engine with a tempo-synced envelope and a host tempo of NaN, infinity, 0, a negative number or an absurd value, and with a NaN or infinite time scale, still plays a finite kick and the voice ends after the release; the renderer treats a NaN time scale as 1.
- `tailLengthSeconds` (`fcm_core_tests`): the usual value, and always finite and non-negative for NaN, infinite and negative inputs.

## 6. Out of scope

- User wavetable and preset file loading (later phases); the requirements they must meet are written in ARCHITECTURE §12.
- Non-parameter state that does not exist yet (the envelope shapes of Phase 3 validate their own data when they land).

## 7. Open questions

None.
