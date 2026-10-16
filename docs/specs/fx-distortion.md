# Spec: Distortion effect

- Status: Implemented (Phase 5, second part; every sound decision below was taken by the agent on the product owner's instruction to decide and keep going, and waits for their ears). Version 0.2: Bits and Rate are shown only when Type is Bitcrush (`shownWhen`, [ADR 0011](../decisions/0011-effect-interface-additions.md)); no sound change.
- Owner: nocturn4 (product), implemented by agent session
- Related: [fx-chain](fx-chain.md), [ADR 0009](../decisions/0009-effect-parameters-and-chain-latency.md), [ARCHITECTURE §4.6](../ARCHITECTURE.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [MVP_SCOPE](../MVP_SCOPE.md)

## 1. What it does for the user

Distortion is the heart of a frenchcore kick: it takes the clean sine and makes it hard, loud and aggressive. This effect has five kinds of distortion:

- **Soft Clip**: smooth saturation (a hyperbolic tangent). Warm, rounds the wave without a harsh edge.
- **Hard Clip**: slices the wave flat at a fixed level. Bright and aggressive, close to a square wave when pushed.
- **Overdrive**: a lopsided saturation that bends the top of the wave more than the bottom. It adds even harmonics, so it sounds thicker and more "tube-like".
- **Foldback**: when the wave goes past the limit it folds back instead of being flattened, which makes metallic, buzzing overtones.
- **Bitcrush**: reduces the number of levels the sound can have (bit depth) and the number of times per second it is measured (rate reduction). Gritty and digital.

Controls:

- **Type**: one of the five above. Changing it while a kick rings can click; set it before you play.
- **Drive**: how hard the sound is pushed into the distortion, 0 to 100 % (0 to 48 dB of gain before the distortion). Default 25 % (12 dB).
- **Pre-gain**: extra gain before the distortion, -24 to +24 dB, default 0. Use it to push a quiet kick into the distortion without changing the Drive curve.
- **Post-gain**: the volume after the distortion, -48 to +12 dB, default -6 dB. Distortion makes a sound louder, so this brings it back down.
- **Tone**: how bright the result is, 0 to 100 %. At 100 % (default) nothing is filtered; lower values take the high end off (down to a low-pass at 1 kHz).
- **Bits** (Bitcrush only): the bit depth, 2 to 16, default 8.
- **Rate** (Bitcrush only): rate reduction, from 1x (none) to 32x, default 4x.

The effect has the common **On** (off by default) and **Mix** controls of every effect.

The aggressive types make harmonics that can fold back into harsh, unmusical frequencies (aliasing). To avoid that, Soft Clip, Hard Clip, Overdrive and Foldback run **oversampled** at the global **Quality** setting (1x, 2x, 4x, 8x; default 2x). Bitcrush is deliberately not oversampled, because its roughness is the effect. Oversampling adds a small delay, which the plugin reports to your DAW and compensates inside, so the timing of the kick never changes.

## 2. User interface

Phase 5 shows the controls in the generic panel as a "Distortion" group until the FX rack arrives.

## 3. Parameters

Effect ID `distortion`. The common controls (`distortion.enabled`, `distortion.mix`) come from the chain ([fx-chain](fx-chain.md)).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `distortion.type` | Type | choice: Soft Clip, Hard Clip, Overdrive, Foldback, Bitcrush | Soft Clip | | | no | yes |
| `distortion.drive` | Drive | 0 to 100, step 1 | 25 | % | linear (0 to 48 dB of gain) | yes (20 ms) | yes |
| `distortion.pregain` | Pre-gain | -24 to +24, step 0.1 | 0 | dB | linear | yes (20 ms) | yes |
| `distortion.postgain` | Post-gain | -48 to +12, step 0.1 | -6 | dB | linear | yes (20 ms) | yes |
| `distortion.tone` | Tone | 0 to 100, step 1 | 100 | % | linear | no (per block) | yes |
| `distortion.bits` | Bits | 2 to 16 (integer) | 8 | bits | | no | yes |
| `distortion.rate` | Rate | 1 to 32, step 0.1 | 4 | x | skewed, midpoint 4 | no | yes |

The Type list is append-only (the saved value is the entry number).

## 4. DSP / implementation approach

- `fcm::dsp::DistortionEffect` in `src/dsp/fx/distortion/`, registered with one line in `RegisterEffects.cpp` (after the chain's test seams: the first real effect).
- **Signal path per channel**: input, times the smoothed gain `10^((pregain + 0.48 * drive) / 20)`, then the shaper (oversampled for the four non-bitcrush types), then, for Overdrive only, a DC blocker (one-pole high-pass at about 5 Hz, so its lopsided shape adds no DC; the other types are symmetric and stay phase-transparent at low pitches), then the smoothed post-gain, then the Tone low-pass.
- **Shapers** (applied to the pre-gained sample `x`): Soft Clip `tanh(x)`; Hard Clip `clamp(x, -1, 1)`; Overdrive (asymmetric, see below); Foldback a triangle fold with period 4 (`1 - |((x + 1) mod 4) - 2|`, which equals `x` for `|x| <= 1` and is correct for any size of `x`); Bitcrush: `x` clamped to -1..1, rounded to `2^(bits - 1)` levels per side, and held for `rate` samples (a phase accumulator, so non-integer rates work).
- **Overdrive** is `y = tanh(x)` for `x >= 0` and `y = 0.6 * tanh(x / 0.6)` for `x < 0`: the negative half clips earlier, which makes the asymmetry. Peaks differ (about 1 and 0.6) at high drive.
- **Oversampling**: `juce::dsp::Oversampling` with the polyphase IIR half-band filters (low latency), one instance per factor 2, 4 and 8, all created in `prepare()`. `setOversampling` picks the instance (and resets it, so no stale state is read). Factor 1 bypasses the oversampler. `latencySamples(factor)` is the instance's latency rounded up (0 at 1x), the same for every type so the latency never depends on the type.
- **Tone**: a one-pole low-pass per channel with the cutoff `1 kHz * 20^(tone / 100)` (1 kHz to 20 kHz, capped at 0.45 of the sample rate); at 100 % it is skipped entirely.
- Non-finite input is replaced by 0 before the shaper, and non-finite output never leaves the effect.
- Everything is allocated in `prepare()`; `process()` takes no locks and allocates nothing.

## 5. Tests

Catch2, in `tests/dsp/fx/DistortionTests.cpp` (the generic tests that every registered effect must pass are in `tests/dsp/fx/RegisteredEffectsTests.cpp`):

- Generic, for every registered effect (picks distortion up automatically): finite output for silence, full-scale noise and a full-scale sine sweep at 44.1, 48 and 96 kHz and at every oversampling factor; `process()` allocates nothing; `reset()` makes two runs bit-identical; `latencySamples` is constant for a given factor and never negative; every `ParameterSpec` valid.
- Shapers: Soft Clip is linear for tiny signals (within 1 %), saturates below 1 for large ones and is odd-symmetric; Hard Clip never exceeds 1 and equals the input below 1; Overdrive has different positive and negative peaks at high drive and its output has no DC after settling; Foldback stays within -1..1 for any input up to 1000 and equals the input below 1 (fold continuity); Bitcrush at 4 bits has at most 17 distinct levels for a full-scale ramp, and Rate 8 holds each value for 8 samples.
- Drive and gains: Drive 0 and Pre-gain 0 and Post-gain 0 dB on Soft Clip leaves a quiet sine within 1 %; Post-gain scales the output by its dB value; Pre-gain +6 dB equals Drive of 12.5 %.
- Tone: Tone 0 has far less energy above 4 kHz than Tone 100 for a hard-clipped sine; Tone 100 is bit-identical to no filter.
- Aliasing: a 4 kHz sine at 44.1 kHz driven hard through Hard Clip has less non-harmonic energy at 8x than at 1x by at least 12 dB, and monotonically less at 2x, 4x and 8x (the control that proves the measurement can fail is the 1x case); Bitcrush is not oversampled (same output at every factor).
- Latency: the measured delay of a 120 Hz sine through the effect equals `latencySamples(factor)` within 1 sample for 2x, 4x and 8x (and 0 at 1x); in the chain, switching the effect off or on, changing the mix or the type does not change the timing.
- Parameters: `fcm_core_tests` guard the new IDs and their defaults and ranges; extreme and non-finite values in the layout state load as defaults.

Manual check (Standalone): switch Distortion on, try each type with Drive and Tone while the auto-play loop runs; switch Quality and listen for aliasing.

## 6. Out of scope

- Per-band distortion, a waveshaper editor, a "dry kick transient" bypass, parameter smoothing of Type, Tone automation smoothing beyond a block.
- A visual transfer-curve display.
- Anti-aliasing the Bitcrush.

## 7. Decisions taken

1. Five types and their shapes as described; Overdrive's 0.6 asymmetry and the 5 Hz DC blocker are first choices.
2. Drive 0 to 100 % maps to 0 to 48 dB; the default 25 % (12 dB) with Post-gain -6 dB is a moderate starting point; frenchcore users will push Drive much higher.
3. IIR oversampling (low latency) rather than linear-phase FIR; the latency is small and compensated, so timing is unaffected.
4. Bitcrush runs at the base rate on purpose.
5. Type is not smoothed: changing it live may click.
