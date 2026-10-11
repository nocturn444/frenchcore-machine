# Spec: 3-band EQ effect

- Status: Implemented (Phase 5, third part; the ranges and defaults are first choices by the agent, on the product owner's instruction to decide and keep going, and wait for their ears)
- Owner: nocturn4 (product), implemented by agent session
- Related: [fx-chain](fx-chain.md), [fx-distortion](fx-distortion.md), [ARCHITECTURE §4.6](../ARCHITECTURE.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md)

## 1. What it does for the user

A simple equaliser with three bands to shape the kick after (or before) the distortion:

- **Low**: a *low shelf*: boosts or cuts everything below its frequency. Use it to add or tame the sub weight.
- **Mid**: a *peak* (bell): boosts or cuts a region around its frequency, as narrow or wide as the Q says. Use it to dig out mud or add punch.
- **High**: a *high shelf*: boosts or cuts everything above its frequency. Use it for click and air.

Each band has **Freq** (where it acts), **Gain** (how much, -18 to +18 dB; 0 does nothing) and **Q** (the width of the Mid bell; the steepness of the shelves). With every Gain at 0 the EQ changes nothing. It has the common **On** (off by default) and **Mix** controls, and it adds no delay.

## 2. User interface

The controls appear in the generic panel as an "Eq" group until the FX rack arrives.

## 3. Parameters

Effect ID `eq`. The common controls (`eq.enabled`, `eq.mix`) come from the chain. Parameter ID suffixes contain a dot after the band name, as in `eq.low.gain`.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `eq.low.freq` | Low Freq | 20 to 500, step 1 | 100 | Hz | skewed, midpoint 100 | per block | yes |
| `eq.low.gain` | Low Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.low.q` | Low Q | 0.3 to 2, step 0.01 | 0.71 | | linear | per block | yes |
| `eq.mid.freq` | Mid Freq | 60 to 8000, step 1 | 500 | Hz | skewed, midpoint 500 | per block | yes |
| `eq.mid.gain` | Mid Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.mid.q` | Mid Q | 0.2 to 10, step 0.01 | 1.0 | | skewed, midpoint 1 | per block | yes |
| `eq.high.freq` | High Freq | 1000 to 20000, step 10 | 8000 | Hz | skewed, midpoint 6000 | per block | yes |
| `eq.high.gain` | High Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.high.q` | High Q | 0.3 to 2, step 0.01 | 0.71 | | linear | per block | yes |

## 4. DSP / implementation approach

- `fcm::dsp::EqEffect` in `src/dsp/fx/eq/`, registered with one line in `RegisterEffects.cpp`, after Distortion.
- Three biquad filters per channel from the Audio EQ Cookbook (Robert Bristow-Johnson): low shelf, peaking EQ and high shelf, with `A = 10^(gain / 40)`. The coefficients are computed by plain functions into a fixed struct (no heap, so it is safe in `setParameters`), and the filters run in transposed direct form II in `float` state with `double` coefficients.
- The frequency is limited to 0.45 of the sample rate and the Q to its range, so the filters stay stable at any setting. A band whose gain is below 0.01 dB in magnitude is skipped entirely, so a flat EQ is bit-exact.
- The coefficients are updated once per block when the parameters change; the filter state continues, so a change is smooth for normal moves. Very large jumps in one block may produce a small step; that is accepted.
- No latency (0 at every factor); the effect ignores the oversampling factor.
- Non-finite input is replaced by 0; non-finite output never leaves the effect.

## 5. Tests

Catch2, in `tests/dsp/fx/EqTests.cpp` (the generic tests of `RegisteredEffectsTests.cpp` pick the EQ up automatically):

- Flat: all gains at 0 give bit-identical output for any signal.
- Frequency response, measured with sines through the effect at 48 kHz (within 0.5 dB): Low +12 dB at 100 Hz boosts 20 Hz by about 12 dB and leaves 2 kHz at 0 dB, a -12 dB cut mirrors it; Mid +12 dB at 1 kHz with Q 1 gives about +12 dB at 1 kHz and about 0 dB at 100 Hz and at 10 kHz; a narrower Q (8) is much less wide (at 1.5 kHz the boost is far smaller than with Q 0.5); High +12 dB at 8 kHz boosts 16 kHz by about 12 dB and leaves 500 Hz at 0 dB.
- The gain at the band frequency is as set: the shelf is +6 dB (half its gain) at its corner and the peak is the full gain at its centre.
- Combined bands add up (in dB) away from each other's influence.
- Stability: extreme settings (Q at both ends, frequencies at both ends, gains at both ends, at 44.1, 48 and 96 kHz, and a frequency above Nyquist) give finite, bounded output for noise and for a loud kick-like sine burst.
- Changing parameters block by block does not blow up (random settings each block) and stays bounded.
- Mono (one channel) buffers work.
- Layout: the new IDs are in the released-ID guard with the documented ranges and defaults.

Manual check (Standalone): switch EQ on, move each band while the auto-play loop runs.

## 6. Out of scope

- More bands, a spectrum or response display, linear-phase mode, per-band enable.
- Smoothing the coefficients sample by sample.

## 7. Decisions taken

1. Three bands with the ranges above; the Mid range covers the kick's body up to its click.
2. Cookbook biquads, no oversampling, no latency.
3. Flat settings are bit-exact because skipped bands are not run at all.
