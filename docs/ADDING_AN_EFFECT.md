# Adding an Effect

The FX section is designed so that a new effect touches only its own folder, one registration line, its tests and its docs. If you find yourself editing the chain, the rack code or the parameter layout code to add an effect, stop: that is an architecture problem and should be raised as an issue.

This guide was validated by adding Distortion, the EQ and Stereo with it ([fx-distortion](specs/fx-distortion.md), [fx-eq](specs/fx-eq.md), [fx-stereo](specs/fx-stereo.md)); none of them needed a change in the chain, the layout code, the reader or the rack. Background: [fx-chain](specs/fx-chain.md), [ADR 0003](decisions/0003-fx-chain-model.md), [ADR 0009](decisions/0009-effect-parameters-and-chain-latency.md).

## Steps

1. **Write the spec.** Copy `docs/specs/_TEMPLATE.md` to `docs/specs/fx-<id>.md`. Define:
   - a stable effect ID (lowercase letters and digits, no spaces, e.g. `compressor`). It can never change after release;
   - every parameter: ID suffix, display name, range, default, unit, skew, whether it is smoothed;
   - the DSP approach and the latency at each oversampling factor;
   - the tests that prove it works.
   The product owner approves what the user hears and sees before implementation.
2. **Create the folder** `src/dsp/fx/<id>/` with `<Name>Effect.h/.cpp`: a class deriving from `fcm::dsp::Effect` ([`src/dsp/fx/Effect.h`](../src/dsp/fx/Effect.h)):
   - `descriptor()` returns the ID, the display name and a `version` (static data);
   - `parameters()` returns a **static** array of `ParameterSpec` (plain data: ID suffix, name, kind `toggle`/`choice`/`integer`/`number`, range, step, default, skew centre, unit, choices). The full ID is `"<id>.<suffix>"`, and a suffix may contain a dot (`low.gain`). Do not declare `enabled` or `mix`: the chain adds them to every effect, off and 100 % by default;
   - `prepare(spec)` allocates everything (buffers, oversamplers); `reset()`, `setParameters(values)`, `setOversampling(factor)` and `process(buffer)` never allocate, lock or wait;
   - `setParameters` gets the values once per block in the order of `parameters()`, in the units of the spec. It never needs the APVTS;
   - `latencySamples(factor)` is constant for a given factor and does not depend on the parameters or on whether the effect is on. The chain compensates the dry signal by it;
   - replace non-finite samples by 0 and never let one leave the effect.
3. **Register it** with one line in `src/dsp/fx/RegisterEffects.cpp` (the include and `registry.add<YourEffect>();`). Append to the end of the list: its order is the default chain order and the effect's index, and must not be reshuffled. Add the `.cpp` file to `src/dsp/CMakeLists.txt` (the sources are listed there).
4. **Write tests** in `tests/dsp/fx/<Name>Tests.cpp` and add the file to `tests/CMakeLists.txt`. The generic tests in `tests/dsp/fx/RegisteredEffectsTests.cpp` pick the new effect up automatically and check, for every sample rate and oversampling factor and with default, minimum and maximum parameter values:
   - valid IDs and parameter specs;
   - finite output for silence, full-scale noise and a full-scale sine sweep, with one channel, and with damaged (non-finite) input;
   - no allocation in `process()`, `reset()` and `setParameters()`;
   - `reset()` makes the output deterministic;
   - constant, non-negative latency, zero at 1x.
   Your own tests cover what the effect does (responses, shapes, aliasing, latency matching `latencySamples`). Also add the effect's parameter IDs (`<id>.enabled`, `<id>.mix` and its own) to the released-ID list in `tests/core/ParameterTests.cpp`.
5. **Update docs:** the effect's section in `docs/USER_GUIDE.md`, `CHANGELOG.md` under `Unreleased`, the status in `docs/PLAN.md`, and the spec's status.
6. **Optional: grouping and a picture.** The rack ([fx-rack](specs/fx-rack.md)) lays out your controls by itself. To get one column per group (like the EQ's bands) give the suffixes a dot ("b1.freq") and name the parameters "<Group> <Control>" ("Band 1 Freq"). To show a picture above the controls, add one entry for your effect ID in `src/ui/EffectDecorations.cpp`; the rack code does not change.
7. **Open a PR** using the template. The effect appears in the Effects box, saves and restores with the project, and takes part in the chain order without any further work.

## Rules

- Never rename or remove a released parameter ID. Deprecate it and add a state migration instead.
- Bump the effect's descriptor `version` when its parameters change, and add a migration.
- Choice lists are append-only (the saved value is the entry number).
- All allocation happens in `prepare()`. `process()`, `reset()`, `setParameters()` and `setOversampling()` are real-time safe.
- Effects do not read other effects' state and do not know their position in the chain.
- An effect that makes harmonics should oversample with `setOversampling` and report the latency of its filters through `latencySamples`.
