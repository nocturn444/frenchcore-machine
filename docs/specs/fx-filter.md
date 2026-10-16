# Spec: Filter effect

- Status: Approved (product owner's answers of 2026-10-16, questions 1.1 and 1.2 A); not implemented. Version 0.2, stage 4. Ranges and defaults are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [mixer-and-buses](mixer-and-buses.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0011](../decisions/0011-effect-interface-additions.md)

## 1. What it does for the user

A classic synth filter to carve the kick: take the top off a distorted tail, thin out the low end of a layer, or isolate a band.

- **Type**: **Low Pass** (keeps the lows), **High Pass** (keeps the highs), **Band Pass** (keeps a band around the cutoff), **Notch** (removes a band around the cutoff).
- **Slope**: **12 dB** (gentle) or **24 dB** per octave (steep).
- **Cutoff**: where the filter acts, 20 Hz to 20 kHz.
- **Resonance**: a peak at the cutoff, from none to a strong whistle (it never self-oscillates into a runaway).
- **Drive**: pushes the sound into a soft saturation inside the filter, which thickens it and tames the resonance peak; 0 % is clean.

It has the common **On** and **Mix** controls. It adds no delay.

## 2. User interface

A strip in any rack ([mixer-and-buses](mixer-and-buses.md)): menus Type and Slope, knobs Cutoff, Resonance, Drive.

## 3. Parameters

Effect ID `filter` (copies `filter2` to `filter4`).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `filter.type` | Type | Low Pass, High Pass, Band Pass, Notch | Low Pass | | choice | per block | yes |
| `filter.slope` | Slope | 12 dB, 24 dB | 12 dB | | choice | per block | yes |
| `filter.cutoff` | Cutoff | 20 to 20000, step 1 | 20000 | Hz | skewed, midpoint 1000 | yes (10 ms, per sample) | yes |
| `filter.resonance` | Resonance | 0 to 100, step 0.1 | 0 | % | linear | yes (10 ms) | yes |
| `filter.drive` | Drive | 0 to 100, step 0.1 | 0 | % | linear | yes (10 ms) | yes |

Defaults: a Low Pass fully open with no resonance and no drive, so switching it on changes almost nothing until Cutoff is moved.

## 4. DSP / implementation approach

- `fcm::dsp::FilterEffect` in `src/dsp/fx/filter/`, one registration line.
- Topology-preserving-transform state variable filter (Vadim Zavalishin, "The Art of VA Filter Design", chapter on the 2-pole SVF), which stays stable and smooth under fast cutoff changes. Low Pass, High Pass, Band Pass (constant peak gain) and Notch are outputs of the same filter. 24 dB = two SVFs in series with the same cutoff, the resonance applied to the second.
- Resonance maps to the SVF damping `k = 2 - 1.95 * r` (`r` = resonance / 100), so `k` never reaches 0 (no self-oscillation).
- Drive: gain `1 + 15 * d^2` (`d` = drive / 100, so up to about +24 dB) into `tanh` at the filter input, divided by the same gain after, so turning Drive adds saturation without a big jump in level. Not oversampled (mild saturation; heavy saturation belongs to Distortion), so `latencySamples` is 0 at every factor.
- Cutoff and resonance are smoothed per sample, and the coefficients recomputed per sample (one `tan` per sample per filter, cheap enough; a table if profiling says otherwise). The cutoff is limited to 0.45 of the sample rate.
- Non-finite input becomes 0.

## 5. Tests

- Response with sines at 48 kHz: Low Pass at 12 and 24 dB is -3 dB (resonance 0, within 1 dB) near the cutoff and falls about 12 or 24 dB per octave above it; High Pass the mirror; Band Pass peaks at the cutoff; Notch removes the cutoff by more than 30 dB.
- Resonance raises the peak at the cutoff; at 100 % the output stays bounded for noise and a full-scale sweep.
- Drive 0 is clean (tiny THD); Drive 100 adds harmonics and keeps the level within 6 dB of Drive 0 for a full-scale sine.
- A cutoff sweep of 20 Hz to 20 kHz in one block, per-sample automation, produces no non-finite value and no step larger than the signal step.
- Generic effect tests (finite output, no allocation, reset determinism, zero latency) come from `RegisteredEffectsTests.cpp`.

## 6. Out of scope

- Comb, formant or ladder models, filter key tracking, a filter envelope (a future modulation system).

## 7. Open questions

None.
