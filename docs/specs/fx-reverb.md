# Spec: Reverb effect

- Status: Approved (product owner's answers of 2026-10-16, questions 1.1 and 1.4 A: one simple room); not implemented. Version 0.2, stage 4. Ranges, defaults and voicing are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [mixer-and-buses](mixer-and-buses.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0011](../decisions/0011-effect-interface-additions.md)

## 1. What it does for the user

A simple room reverb. In hardcore kick design it is often put **before** the distortion: the distortion then turns the reverb's tail into a fuller, longer, noisier kick tail. After the distortion it adds space.

- **Size**: from a small room to a big hall.
- **Decay**: how long the reverb rings, 0.1 to 10 seconds.
- **Damping**: how dark the tail becomes as it fades (high frequencies die first).
- **Pre-delay**: a short gap before the reverb starts, 0 to 200 ms, which keeps the hit dry and clear.
- **Low Cut**: removes the lows from the reverb only, so it does not muddy the sub. Default 200 Hz.

Its **Mix** starts at **30 %** (not 100 % like the other effects), because a reverb that is only wet is rarely what you want. The reverb is stereo even though the kick is mono. It adds no delay to the dry sound.

## 2. User interface

A strip in any rack: knobs Size, Decay, Damping, Pre-delay, Low Cut.

## 3. Parameters

Effect ID `reverb` (copies `reverb2` to `reverb4`). `defaultMix` = 0.3 ([ADR 0011](../decisions/0011-effect-interface-additions.md)), so `reverb.mix` defaults to 30.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `reverb.size` | Size | 0 to 100, step 0.1 | 40 | % | linear | per block (delay lengths change smoothly) | yes |
| `reverb.decay` | Decay | 0.1 to 10, step 0.01 | 1.2 | s | skewed, midpoint 1.5 | per block | yes |
| `reverb.damping` | Damping | 0 to 100, step 0.1 | 50 | % | linear | per block | yes |
| `reverb.predelay` | Pre-delay | 0 to 200, step 0.1 | 0 | ms | linear | per block | yes |
| `reverb.lowcut` | Low Cut | 20 to 1000, step 1 | 200 | Hz | skewed, midpoint 200 | per block | yes |

## 4. DSP / implementation approach

- `fcm::dsp::ReverbEffect` in `src/dsp/fx/reverb/`, one registration line.
- An 8-line feedback delay network with a Householder feedback matrix (Jot and Chaigne, 1991; Julius O. Smith, "Physical Audio Signal Processing", FDN reverberation). Mutually prime delay lengths between about 10 and 80 ms scaled by Size (0.3x to 1.5x), a few short allpass diffusers before the network, per-line gain set from Decay so the tail falls 60 dB in Decay seconds (`g = 10^(-3 * delay / (decay * sr))`), a one-pole low-pass per line for Damping, slight modulation of two lines to avoid metallic ringing. Two decorrelated output taps for left and right.
- Pre-delay: a delay line before the network. Low Cut: a 2nd-order high-pass on the reverb input.
- All delay lines are allocated in `prepare()` for the longest size at the current sample rate. Size changes crossfade the read positions so they never click.
- The wet output is the reverb only (the chain's Mix adds the dry). No latency.
- Non-finite input becomes 0. `reset()` clears every line (a reverb that wakes up never plays an old tail).

## 5. Tests

- An impulse produces a tail that falls 60 dB within 20 % of Decay (measured on the energy decay curve) for 0.5, 1.2 and 5 s.
- Damping 100 makes the late tail darker than Damping 0 (energy above 4 kHz in the last half).
- Low Cut 500 Hz leaves the reverb's energy below 100 Hz at least 20 dB lower than Low Cut 20 Hz.
- Pre-delay delays the first reflection by that time (within 1 ms).
- Left and right outputs differ (correlation below 0.9) for a mono input.
- The default Mix of every reverb copy is 30 %.
- Extreme settings stay bounded (Decay 10 s, Size 100 %, a full-scale square wave for 10 s); generic effect tests.

## 6. Out of scope

- Several reverb models (room, hall, plate; question 1.4 A), freeze, ducking, shimmer.

## 7. Open questions

None.
