# 0009: Effects describe their parameters as plain data, and the chain keeps a constant latency

- Status: Accepted
- Date: 2026-10-11

## Context

[ADR 0003](0003-fx-chain-model.md) fixed the chain model (one reorderable slot per effect type). Building it raises two questions the ADR left open.

1. **Where do an effect's parameters live?** The rule is that `fcm_dsp` never includes APVTS or plugin headers, and its test executable links only `juce_dsp`, which enforces it. So an effect inside `fcm_dsp` cannot create `juce::AudioParameterFloat` objects, yet "adding an effect must not require edits to the layout code".
2. **What does the chain report as latency?** Distortion runs oversampled, and oversampling filters delay the signal. Hosts want a latency number, and changing it while playing is disruptive. A dry/wet mix or a bypass that does not delay the dry signal by the same amount would comb-filter against the wet one.

## Decision

1. Each effect returns its parameters as **plain data** (`ParameterSpec`: id suffix, name, kind, range, step, default, skew centre, unit, choices). `fcm_core` turns the specs of every registered effect into APVTS parameters when it builds the layout, and a reader turns the parameter values into a flat array of floats that the chain hands to the effect once per block (`Effect::setParameters`). No effect includes anything outside `fcm_dsp`.
2. The common controls of a slot (`<id>.enabled`, `<id>.mix`) are added by the chain's layout code for every effect, so they are never repeated in an effect.
3. Every slot has a **fixed latency** per quality setting. The dry signal of every slot is always delayed by that latency, whether the slot is on, bypassed or mixed, so switching or mixing never changes timing. The latency reported to the host is the sum over all registered effects for the current quality (the global oversampling choice), so it only changes when the user changes Quality, not when an effect is toggled or reordered.
4. The chain owns every effect once (the registry builds them at `prepare()`), the order is a packed value in a single atomic that the message thread writes and the audio thread reads once per block, and an invalid order falls back to the registration order.

## Consequences

- Adding an effect is one folder, one registration line, tests and docs; the layout, the chain and the rack UI do not change.
- A bypassed effect still costs a small delay line, and the chain adds a little latency even with everything off whenever an oversampled effect is registered. That is the price of never changing the host latency or the timing on toggle.
- The effect's parameters can be tested without a host, as plain arrays.
- If a future effect needs a parameter kind that `ParameterSpec` cannot describe, the spec grows; effects that already exist do not change.
