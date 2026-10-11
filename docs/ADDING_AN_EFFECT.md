# Adding an Effect

The FX section is designed so that a new effect touches only its own folder, one registration line, its tests and its docs. If you find yourself editing the chain, the UI rack or the parameter layout code to add an effect, stop: that is an architecture problem and should be raised as an issue.

This guide describes the intended workflow. It will be validated and updated when the first effect is implemented (Phase 5 in [PLAN.md](PLAN.md)).

## Steps

1. **Write the spec.** Copy `docs/specs/_TEMPLATE.md` to `docs/specs/fx-<id>.md`. Define:
   - a stable effect ID (lowercase, no spaces, e.g. `compressor`). It can never change after release;
   - every parameter: ID suffix, display name, range, default, unit, skew, whether it is smoothed;
   - the DSP approach and latency;
   - the tests that prove it works.
2. **Create the folder** `src/dsp/fx/<id>/` with:
   - `<Name>Effect.h/.cpp`: a class deriving from `Effect` (see [ARCHITECTURE.md §4.6](ARCHITECTURE.md#46-fx-chain-main-extension-point));
   - a `createParameters()` function returning the effect's parameter group. Parameter IDs are `"<id>.<suffix>"`;
   - optional `<Name>Panel.h/.cpp` under `src/ui/fx/` if the effect needs a custom panel. Without one, the rack shows an automatic panel built from the parameters.
3. **Register it** with one line in `src/dsp/fx/RegisterEffects.cpp`. Append to the end of the list; order in this file is the default chain order for new instances and must not be reshuffled.
4. **Write tests** in `tests/dsp/fx/<id>Tests.cpp`. Every effect must at least pass:
   - finite output (no NaN/Inf) for silence, full-scale noise and a full-scale sine sweep;
   - no allocation in `process()`;
   - `reset()` makes output deterministic;
   - correct `latencySamples()`.
   The generic chain tests pick up the new effect automatically.
5. **Update docs:** the user guide section for effects, the MVP/feature table if relevant, and `CHANGELOG.md` under `Unreleased`.
6. **Open a PR** using the template.

## Rules

- Never rename or remove a released parameter ID. Deprecate it and add a state migration instead.
- Bump the effect's descriptor `version` when its parameters change, and add a migration.
- All allocation happens in `prepare()`. `process()` and `reset()` are real-time safe.
- Effects do not read other effects' state and do not know their position in the chain.
