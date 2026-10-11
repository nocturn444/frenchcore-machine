# Spec: FX chain (effect interface, registry and reorderable chain)

- Status: Implemented (Phase 5, first part: the machinery, with no effect yet; the effects follow in their own specs)
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADR 0003](../decisions/0003-fx-chain-model.md), [ADR 0009](../decisions/0009-effect-parameters-and-chain-latency.md), [ARCHITECTURE §4.6](../ARCHITECTURE.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [UI_LAYOUT](../UI_LAYOUT.md)

## 1. What it does for the user

The kick goes through a row of effects before the output gain and the safety limiter. This part builds the machinery; the first effects (distortion, EQ, multiband stereo) arrive in the next parts. What the user will be able to do with every effect, whichever it is:

- **Switch it on or off** (each effect starts **off**, so a patch sounds the same as before until you switch something on).
- **Mix** it: a dry/wet control per effect, 0 to 100 %, default 100 %.
- **Reorder** the effects by dragging them in the rack. Every effect exists once. Its controls keep their identity wherever it sits, so automation and presets keep working after a reorder.
- Switching an effect on or off, changing its mix, or reordering never clicks and never changes the timing of the sound.
- The order is saved with the project (and later with presets), but it is not automatable.

The FX chain works on both channels of the output. The oscillators make a mono kick; the chain is the first place where the two channels can differ (the multiband stereo effect).

## 2. User interface

Not in this part (the rack UI is a later part of Phase 5). Until then the generic panel shows an effect's controls like any other group, with the name of the effect as the heading.

## 3. Parameters

The chain adds two parameters for **every registered effect** (the ID prefix is the effect's stable ID, for example `distortion`):

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `<id>.enabled` | `<Name>` On | off / on | off | | | yes (10 ms, a crossfade) | yes |
| `<id>.mix` | `<Name>` Mix | 0 to 100, step 1 | 100 | % | linear | yes (10 ms) | yes |

and one global parameter:

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `global.quality` | Quality | choice: 1x, 2x, 4x, 8x | 2x | oversampling factor | | no | yes |

An effect's own parameters come from its `parameters()` description (section 4). The chain's order is state, not a parameter.

## 4. DSP / implementation approach

- **`fcm::dsp::Effect`** (`src/dsp/fx/Effect.h`): `descriptor()` (stable `id`, display name, `version`), `parameters()` (a span of `ParameterSpec`), `prepare(EffectSpec)` (may allocate; gets sample rate, maximum block size, channels), `reset()`, `setParameters(span<const float>)` (once per block; values in spec order, in the units of the spec), `setOversampling(factor)` (when the Quality factor changes), `process(AudioBuffer&)` (real-time safe, in place), `latencySamples(int oversamplingFactor)`. Effects do not know their position and never read other effects.
- **`ParameterSpec`** is plain data: id suffix, display name, kind (`toggle`, `choice`, `integer`, `number`), minimum, maximum, step, default, skew centre (0 for linear), unit label and, for a choice, the entry names. `fcm_core` builds APVTS parameters from it, so nothing in `fcm_dsp` includes APVTS.
- **`EffectRegistry`** (`src/dsp/fx/EffectRegistry.h`): a list of factories in registration order, which is also the default chain order and the index of the effect everywhere (`effectIndex`). `RegisterEffects.cpp` is the only file that names effects, one line each. At most 16 effects and 24 parameters per effect.
- **`EffectChain`** (`src/dsp/fx/EffectChain.h`) owns one instance of each registered effect, the scratch buffers and a dry delay line per slot. Per block, in the order of the packed order value:
  1. for each effect, `setParameters`, and the slot's smoothed mix target is `enabled ? mix : 0`;
  2. a slot whose mix is 0 and fully faded out is skipped (the dry signal is only delayed by the effect's latency) and its effect is `reset()` when it wakes up again;
  3. otherwise the input is copied to the scratch buffer, the effect processes the copy, and the output is `dry_delayed * (1 - mix) + wet * mix` per sample, with the dry signal delayed by the effect's latency for the current quality (constant timing, [ADR 0009](../decisions/0009-effect-parameters-and-chain-latency.md)).
  Nothing allocates in `process()`.
- **Order**: up to 16 effect indices packed 4 bits each into one `std::atomic<uint64_t>`; the message thread stores it, the audio thread loads it once per block. A packed value that is not a permutation of the registered effects falls back to the registration order. `core::FxOrderState` keeps the order as a list of effect IDs, saves it as the node `FxChain` (property `order`, comma-separated IDs, most-significant first), and restores it: unknown IDs are dropped, repeated IDs ignored, missing effects appended in registration order, so a project from a version with other effects still loads.
- **Latency**: `EffectChain::latencySamples(factor)` is the sum over all registered effects. The processor reports it to the host with `setLatencySamples` whenever Quality changes (message thread).
- **Engine**: the voice renders mono into channel 0, which is copied to the other channels; then the chain runs on the whole block; then the output gain and the safety limiter are applied per sample on every channel. With no effect registered, or none on, the output is bit-identical to before.
- **Parameters in the engine**: `EngineParameters` carries, per effect index, the enabled flag, the mix and the array of the effect's own values, and the oversampling factor. `EngineParameterReader` fills them from the registry and the APVTS raw values; a value that is not finite becomes the effect's default.

## 5. Tests

Catch2 (`fcm_dsp_tests` with small test effects defined in the test files, and `fcm_core_tests`):

- Registry: effects are listed in registration order with unique non-empty IDs; the limits are enforced; every parameter spec is valid (min below max, default inside, positive step for numbers, choices for a choice).
- Chain, with a gain effect and a delay effect (latency 64 samples at 1x, 128 at 2x): all effects off gives exactly the dry signal delayed by the sum of the slot latencies; one effect on at mix 100 % gives the wet signal; mix 50 % gives the average of delayed dry and wet; the order changes the result when effects do not commute (gain before or after a clipper) and a reordering mid-stream never produces a click larger than the step of the signal; an invalid packed order uses the registration order; switching on and off crossfades (no jump larger than the signal step per sample); a skipped slot's effect is `reset()` on wake-up; `setParameters` values reach the effect in spec order; output is finite for silence, full-scale noise and a sine sweep; `process` allocates nothing (a counting global `operator new` armed during the block; JUCE's own containers are not touched).
- Order state (`fcm_core_tests`): round trip; unknown IDs dropped; duplicates ignored; missing effects appended in registration order; damaged text gives the registration order; a huge text is cut; the packed value matches the order.
- Layout: for a test registry, every effect gets `<id>.enabled` and `<id>.mix` and its own parameters with the documented kinds, ranges and defaults; the released-ID guard is unchanged while no real effect is registered; `global.quality` exists with the documented choices and default.
- Engine: with no effect registered the output is bit-identical to the previous engine for a fixed note and parameters; with a test effect on, both channels carry its output; latency reporting equals the sum of the slots for each quality.
- Reader: effect values arrive in spec order in `EngineParameters`; non-finite values fall back to the default.

## 6. Out of scope

- The effects themselves (distortion, EQ, multiband stereo) and the rack UI with drag-and-drop (later parts of Phase 5).
- Duplicate instances of an effect, sidechain, effect presets.
- Automating the order.

## 7. Decisions taken

1. Effects start **off** and mix at 100 %, so nothing changes in an existing patch.
2. Constant latency regardless of what is on (ADR 0009), reported when Quality changes.
3. Packed 4-bit order in one atomic, at most 16 effects.
4. The FX chain runs on the stereo block after the mono voice, before the output gain and the limiter, so the limiter always protects the final output.
