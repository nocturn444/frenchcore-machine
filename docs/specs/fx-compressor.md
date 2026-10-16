# Spec: Compressor effect (with a Multiband / OTT mode)

- Status: Approved (product owner's answers of 2026-10-16, questions 1.1 and 1.3 A: one Compressor with a Multiband switch; off is a normal compressor, on is an OTT-style multiband compressor); implemented (stage 4, third of the four effects). Ranges, defaults and the exact curves are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [mixer-and-buses](mixer-and-buses.md), [fx-stereo](fx-stereo.md) (same crossovers), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0011](../decisions/0011-effect-interface-additions.md)

## 1. What it does for the user

Compression evens out the level of a sound. On a kick it makes the tail denser and louder next to the punch, or (with a fast attack) squashes the punch.

**Multiband off: a normal compressor.**

- **Threshold**: above this level the sound is turned down.
- **Ratio**: how much: 4:1 means 4 dB over the threshold come out as 1 dB.
- **Attack**: how fast it reacts. A slower attack lets the hit of the kick through before it clamps down.
- **Release**: how fast it lets go.
- **Knee**: soft (gradual) or hard (sudden) start of the compression.
- **Makeup**: gain after the compression to bring the level back up.

**Multiband on: an OTT-style compressor.** The sound is split in three bands (lows, mids, highs). In each band, loud parts are turned down (**downward** compression) and quiet parts are turned up (**upward** compression), which makes everything dense and "in your face", the classic OTT sound.

- **Depth**: how much of the whole effect (0 % nothing, 100 % full; like OTT's Depth).
- **Time**: how fast all the bands react (lower is faster).
- **Upward** and **Downward**: how much of each kind of compression.
- **In Gain** and **Out Gain**: level into and out of the effect.
- **Low**, **Mid**, **High**: the level of each band after compression.

The **Mix** of every effect works as usual. It adds no delay.

## 2. User interface

A strip in any rack: the **Multiband** switch, then only the controls of the current mode (`shownWhen`, [ADR 0011](../decisions/0011-effect-interface-additions.md)). Off: Threshold, Ratio, Attack, Release, Knee, Makeup. On: Depth, Time, Upward, Downward, In Gain, Out Gain, Low, Mid, High. A small gain-reduction meter is not part of this version.

## 3. Parameters

Effect ID `compressor` (copies `compressor2` to `compressor4`).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `compressor.multiband` | Multiband | off / on | off | | | crossfade 10 ms | yes |
| `compressor.threshold` | Threshold | -60 to 0, step 0.1 | -18 | dB | linear | yes (10 ms) | yes |
| `compressor.ratio` | Ratio | 1 to 20, step 0.1 | 4 | :1 | skewed, midpoint 4 | yes (10 ms) | yes |
| `compressor.attack` | Attack | 0.1 to 100, step 0.1 | 5 | ms | skewed, midpoint 10 | per block | yes |
| `compressor.release` | Release | 10 to 1000, step 1 | 100 | ms | skewed, midpoint 150 | per block | yes |
| `compressor.knee` | Knee | 0 to 12, step 0.1 | 6 | dB | linear | per block | yes |
| `compressor.makeup` | Makeup | -12 to +24, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |
| `compressor.depth` | Depth | 0 to 100, step 0.1 | 100 | % | linear | yes (10 ms) | yes |
| `compressor.time` | Time | 0 to 100, step 0.1 | 50 | % | linear | per block | yes |
| `compressor.upward` | Upward | 0 to 100, step 0.1 | 100 | % | linear | yes (10 ms) | yes |
| `compressor.downward` | Downward | 0 to 100, step 0.1 | 100 | % | linear | yes (10 ms) | yes |
| `compressor.ingain` | In Gain | -24 to +24, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |
| `compressor.outgain` | Out Gain | -24 to +24, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |
| `compressor.low` | Low | -24 to +24, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |
| `compressor.mid` | Mid | -24 to +24, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |
| `compressor.high` | High | -24 to +24, step 0.1 | 0 | dB | linear | yes (10 ms) | yes |

`shownWhen`: Threshold to Makeup with Multiband off; Depth to High with Multiband on.

## 4. DSP / implementation approach

- `fcm::dsp::CompressorEffect` in `src/dsp/fx/compressor/`, one registration line.
- **Single band**: feed-forward compressor with a log-domain gain computer and a smooth branching peak detector (Giannoulis, Massberg and Reiss, "Digital Dynamic Range Compressor Design: A Tutorial and Analysis", JAES 2012), soft knee as in that paper, detector on the maximum of the two channels (linked, so the stereo image does not move).
- **Multiband**: three bands with 4th-order Linkwitz-Riley crossovers at 88 Hz and 2500 Hz (the split points OTT is known for), the same crossover code as Stereo, so the bands add back to the input when nothing is compressed (allpass phase, no latency). In each band, a gain computer with two parts: downward above a threshold (-14 dB below full scale for the lows, -20 for mids and highs; ratio 66:1 times Downward) and upward below a threshold (-40 dB; ratio 4:1 times Upward, gain limited to +24 dB so silence is not raised into noise); attack and release per band (lows 30/300 ms, mids 10/150 ms, highs 3/80 ms) scaled by `0.1 + 1.9 * time / 100`. Depth crossfades each band's gain toward unity (0 dB) in the dB domain. These numbers are a starting point to be voiced by ear against OTT; they live in one table in the effect.
- **Choices made in the implementation (technical, by the agent):** the single-band detector holds the peak of the louder channel and lets go over 20 ms (fixed, not a control) before the gain computer, so a steady tone is seen at its peak and the attack and release act on the gain reduction in dB (a one-pole smoothing: 63 % of the change at the set time). In multiband, Downward and Upward scale the ratio between 1:1 and the full one (66:1 and 4:1), and the gain of each band is smoothed in dB with the band's own attack and release (attack when the gain must fall, release when it rises). A path that is not heard is not run and starts from rest when needed.
- The strip: more than seven controls in a single row make two rows (the multiband mode has nine plus the switch); knobs in the rack scroll the list under the wheel instead of changing (drag to change).
- Switching Multiband crossfades the two outputs over 10 ms; both paths run during the crossfade only.
- No oversampling, no latency (no look-ahead).
- Non-finite input becomes 0; gains are clamped to -60..+24 dB.

## 5. Tests

- Single band, static curve with a steady sine: below threshold gain 0 dB (Makeup 0); 12 dB above threshold at 4:1 comes out 3 dB above (within 0.5 dB, hard knee); the knee is continuous; attack and release reach 63 % of the change within 20 % of the set times.
- Multiband with Depth 0: output equals input (within 0.1 dB across the spectrum, the crossovers add back to flat magnitude). With Depth 100: a quiet sine in one band is raised, a loud sine is lowered, each band independently.
- Band levels move only their band.
- Switching Multiband mid-signal produces no step larger than the signal step.
- Extreme settings give bounded, finite output; generic effect tests.

## 6. Out of scope

- Sidechain input, look-ahead, a gain-reduction meter, user crossover points, per-band thresholds.

## 7. Open questions

None (the curve numbers are tuning, done by ear after implementation).
