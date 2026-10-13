# Spec: Multiband stereo effect

- Status: Implemented (Phase 5, fourth part; the ranges, defaults and the decorrelation are first choices by the agent, on the product owner's instruction to decide and keep going, and wait for their ears)
- Owner: nocturn4 (product), implemented by agent session
- Related: [fx-chain](fx-chain.md), [fx-eq](fx-eq.md), [ARCHITECTURE §4.6](../ARCHITECTURE.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md)

## 1. What it does for the user

It splits the kick into three bands (low, mid, high) and lets you set the stereo width of each one separately. A typical use: keep the low end perfectly mono (it hits harder and plays well on every system) and open up the highs.

- **Crossover 1** and **Crossover 2**: where the low band ends and the high band starts (the mid band is between them).
- **Low Width**, **Mid Width**, **High Width**: how wide each band is, from 0 % (mono) through 100 % (exactly as it comes in) up to 200 % (very wide).
- **Mono Below** switch (on by default): forces the low band to mono whatever its width control says. It is the safe setting for a kick.

The kick is generated in mono, so there is nothing to widen in the ordinary sense. To make **Width above 100 %** useful, the effect adds a *decorrelated* copy of each band to the sides: a copy with the same loudness but shifted in time in a frequency-dependent way, so it sounds like a different signal in each ear without changing the tone. At 100 % or below nothing is added: a mono kick stays exactly mono.

It has the common **On** (off by default) and **Mix** controls and adds no delay. It works on stereo output; with a mono output it does nothing.

## 2. User interface

The controls appear in the generic panel as a "Stereo" group until the FX rack arrives.

## 3. Parameters

Effect ID `stereo`. The common controls (`stereo.enabled`, `stereo.mix`) come from the chain.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `stereo.xover1` | Crossover 1 | 40 to 400, step 1 | 120 | Hz | skewed, midpoint 120 | per block | yes |
| `stereo.xover2` | Crossover 2 | 400 to 8000, step 1 | 2500 | Hz | skewed, midpoint 2500 | per block | yes |
| `stereo.lowwidth` | Low Width | 0 to 200, step 1 | 100 | % | linear | per block | yes |
| `stereo.midwidth` | Mid Width | 0 to 200, step 1 | 100 | % | linear | per block | yes |
| `stereo.highwidth` | High Width | 0 to 200, step 1 | 100 | % | linear | per block | yes |
| `stereo.monobelow` | Mono Below | off / on | on | | | per block | yes |

## 4. DSP / implementation approach

- `fcm::dsp::StereoEffect` in `src/dsp/fx/stereo/`, registered with one line in `RegisterEffects.cpp`, after the EQ.
- **Split**: two Linkwitz-Riley crossovers of 4th order (two cascaded Butterworth biquads, Q = 1/sqrt 2), per channel: the input is split at Crossover 1 into low and the rest; the rest is split at Crossover 2 into mid and high; the low band goes through an all-pass built from the Crossover 2 pair (low-pass plus high-pass) so that all three bands stay in phase and sum back to the input's magnitude exactly (Linkwitz and Riley, 1976; Rane note 160). Crossover 2 is kept at least 1.5 times Crossover 1 and both below 0.45 of the sample rate.
- **Width per band** with mid/side: `M = (L + R) / 2`, `S = (L - R) / 2`. With the width `w` (0 to 2) and `a = max(0, w - 1)`:
  `S' = w * S + a * D(M)` and `M' = M`, then both are divided by `sqrt(1 + a^2)` to keep the loudness, and `L = M' + S'`, `R = M' - S'`. `D` is a fixed decorrelator per band and channel pair: a cascade of six first-order all-pass filters with distinct coefficients, which keeps the magnitude of `M` and shifts its phase differently at each frequency. For `w <= 1` (`a = 0`) the effect is plain mid/side width and a mono input stays mono.
- **Mono Below**: when on, the low band uses `w = 0`.
- State is per channel in `double`; coefficients are computed into plain structs (no heap) when the parameters change, once per block. With a width of exactly 100 % in every band and Mono Below off, the effect is the all-pass of the input (magnitude flat, phase shifted around the crossovers).
- A buffer with fewer than two channels is left untouched, except that samples that are not numbers are replaced by 0. Non-finite input is replaced by 0.

## 5. Tests

Catch2, in `tests/dsp/fx/StereoTests.cpp` (the generic tests pick the effect up automatically):

- Flat magnitude: with all widths 100 % and Mono Below off, a sine through the effect keeps its level within 0.3 dB at 25 Hz, 120 Hz, 1 kHz, 2.5 kHz, 10 kHz and 18 kHz, at 44.1, 48 and 96 kHz.
- Width 0 in every band makes a stereo input mono (L equals R), whatever the Mono Below setting.
- Width 100 % keeps the stereo image: for an input with only the left channel, the left and right levels keep their ratio.
- Width above 100 % widens a mono input: for noise, the normalised correlation between L and R falls well below 1 (under 0.85 at 200 % in all bands) and the total level stays within 2 dB; at 100 % or less a mono input stays bit-identical between L and R.
- Per band: Width 0 in the high band mono-ises content at 10 kHz and leaves content at 100 Hz stereo; Mono Below mono-ises content at 50 Hz even with Low Width at 200 % and leaves 5 kHz content alone (the low band is mono-ised about 25 dB down; the rest is the crossover slope).
- The decorrelator keeps the level of what it processes (within 0.5 dB) across the audio band.
- Crossover order and range: Crossover 2 below Crossover 1 is corrected; extremes (frequencies, widths, sample rates) give finite, bounded output for noise and a kick-like burst.
- One-channel buffers are untouched (apart from non-finite samples); zero latency at every factor.
- The layout guard lists the new IDs.

Manual check (Standalone): switch Stereo on, set High Width to 200 % and listen with headphones while the loop runs; check that the low end stays centred.

## 6. Out of scope

- A visual crossover display, a fourth band, per-band mute or solo, widening by delay (Haas).
- True stereo sources inside the plugin (the oscillators are mono).

## 7. Decisions taken

1. Linkwitz-Riley 4th order, three bands, as in the architecture note.
2. Width above 100 % uses a decorrelator, so the control does something on a mono kick; at or below 100 % it never adds anything.
3. Mono Below defaults to on: harmless for a mono kick and the right habit for the low end.
