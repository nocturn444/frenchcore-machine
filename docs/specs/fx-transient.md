# Spec: Transient shaper effect

- Status: Approved (product owner's answer of 2026-10-16, question 1.1); not implemented. Version 0.2, stage 4. Ranges and defaults are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [mixer-and-buses](mixer-and-buses.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md)

## 1. What it does for the user

A transient shaper turns the **attack** (the first instant of every hit) and the **sustain** (what rings after it) up or down, whatever sound goes in. Place it after a distortion to bring back the punch the distortion flattened, or to shorten a tail.

- **Attack**: -100 % softens the hit, +100 % makes it snap harder. Default 0.
- **Sustain**: -100 % shortens what rings after the hit, +100 % makes it longer and louder. Default 0.
- **Speed**: how short a "hit" is: lower values react to the very first milliseconds only, higher values to a longer attack. Default 50 %.
- **Output**: volume after the effect, -24 to +12 dB.

With Attack and Sustain at 0 the effect changes nothing. It has the common On and Mix and adds no delay.

## 2. User interface

A strip in any rack: knobs Attack, Sustain, Speed, Output.

## 3. Parameters

Effect ID `transient` (copies `transient2` to `transient4`).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `transient.attack` | Attack | -100 to +100, step 0.1 | 0 | % | linear | yes (10 ms) | yes |
| `transient.sustain` | Sustain | -100 to +100, step 0.1 | 0 | % | linear | yes (10 ms) | yes |
| `transient.speed` | Speed | 0 to 100, step 0.1 | 50 | % | linear | per block | yes |
| `transient.output` | Output | -24 to +12, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |

## 4. DSP / implementation approach

- `fcm::dsp::TransientEffect` in `src/dsp/fx/transient/`, one registration line.
- Envelope-difference method of the SPL Transient Designer (level-independent): a fast and a slow peak follower on the linked stereo level; the attack signal is `max(0, dB(fast) - dB(slow))`, the sustain signal is `max(0, dB(slow) - dB(fast))` with a slow release. Gain in dB = `attack% * 0.24 * attackSignal + sustain% * 0.24 * sustainSignal`, clamped to -24..+24 dB, smoothed by 1 ms so it does not distort low frequencies. Speed sets the fast follower's attack/release (0.1 to 5 ms) and the slow follower's attack (5 to 50 ms).
- Level-independent by design: the same kick at -20 dB is shaped like at 0 dB.
- No look-ahead, no latency. With Attack and Sustain both at 0 the gain is exactly 1 (bit-exact, the process is skipped).
- Non-finite input becomes 0.

## 5. Tests

- Neutral settings are bit-exact.
- A synthetic kick (fast attack, exponential decay): Attack +100 raises the peak of the first 5 ms relative to the tail; Attack -100 lowers it; Sustain +100 raises the energy after 50 ms; Sustain -100 lowers it.
- The same input at 0 dB and at -20 dB gives the same shaping (gain curves within 1 dB).
- Bounded, finite output at extremes; generic effect tests.

## 6. Out of scope

- Frequency-dependent (multiband) shaping, look-ahead, a clip stage.

## 7. Open questions

None.
