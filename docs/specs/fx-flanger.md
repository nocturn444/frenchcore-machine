# Spec: Flanger effect

- Status: Approved (product owner's answers of 2026-10-20, questions G.1 A, G.2 A, 3.1 A and 3.2 A); not implemented yet (version 0.3, stage 5). Ranges, defaults and voicing are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md), [fx-phaser](fx-phaser.md)

## 1. What it does for the user

A flanger: it mixes the sound with a copy delayed by a few milliseconds and moves that delay, which gives the "jet" sweep. In hardcore kicks it is also used **still** (Rate 0): a tuned comb that gives the tail a metallic tone. With Key Track at 100 % that tone follows the note you play, so it stays in tune with the kick.

- **Delay**: where the effect sits (0.1 to 10 ms). Shorter delays sound higher and thinner, longer ones lower and more like a comb.
- **Rate** and **Sync**: how fast it sweeps, in Hz or as a note value that follows the tempo (same as the Phaser).
- **Depth**: how far the sweep goes, up to two octaves up from Delay's tone.
- **Feedback**: from -95 % to +95 %. Positive makes it ring more, metallic and whistling; negative gives a hollower, more nasal ring.
- **Key Track**: 0 % keeps Delay fixed; 100 % retunes it with the note (one octave per octave, relative to C3).

**The sweep starts from Delay at every note**, so every kick sounds the same. It has the common **On** and **Mix** controls (Mix starts at 100 %, which is already a full flanger: the delayed copy is part of the effect's own sound). It adds no delay to the timing of the kick. Both channels get the same flanger.

## 2. User interface

A strip in any rack: knobs Delay, Rate (or the note value when Sync is on), Depth, Feedback, Key Track, switch Sync.

## 3. Parameters

Effect ID `flanger` (copies `flanger2` to `flanger4`). `defaultMix` = 1.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `flanger.delay` | Delay | 0.1 to 10, step 0.01 | 2 | ms | skewed, midpoint 2 | yes (20 ms) | yes |
| `flanger.sync` | Sync | off, on | off | | toggle | per block | yes |
| `flanger.rate` | Rate | 0 to 20, step 0.01 | 0.3 | Hz | skewed, midpoint 2 | per block | yes; shown when Sync is off |
| `flanger.division` | Rate | same list as `phaser.division` | 1/4 | | choice | per block | yes; shown when Sync is on |
| `flanger.depth` | Depth | 0 to 100, step 0.1 | 50 | % | linear | yes (10 ms) | yes |
| `flanger.feedback` | Feedback | -95 to 95, step 0.1 | 50 | % | linear | yes (10 ms) | yes |
| `flanger.keytrack` | Key Track | 0 to 100, step 0.1 | 0 | % | linear | per block | yes |

## 4. DSP / implementation approach

- `fcm::dsp::FlangerEffect` in `src/dsp/fx/flanger/`, one registration line.
- A feedback comb: `w = x + fb * d(t)`, `y = (x + d(t)) / 2` scaled by `1 - 0.5 * |fb|`, where `d(t)` is `w` read `D(t)` samples ago from a delay line (Smith, "Physical Audio Signal Processing", flanging). Peaks stay within +6 dB at 95 %.
- `D(t) = Delay * 2^(-2 * depth * l(t)) * 2^(-keytrack * note / 12)`, with `l(t) = (1 - cos(2 pi phase)) / 2` (0 at the start, so the sweep starts at Delay and goes up in tone), `note` from `EffectContext::noteSemitones`; limited to 0.05 to 40 ms.
- Fractional delay read with 4-point Hermite (cubic) interpolation, so a slow sweep does not zipper. The delay line holds 50 ms at the sample rate, allocated in `prepare()`.
- The LFO is `dsp::TempoLfo` ([ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md)), restarted at every `noteOn`.
- No oversampling; `latencySamples` is 0 (the delayed copy is the effect, not a latency). Non-finite input becomes 0, and the delay line is cleared if a non-finite value ever reaches it.

## 5. Tests

- Rate 0, Depth 0, Feedback 0, Delay 2 ms: notches at 250 Hz, 750 Hz, 1250 Hz (odd multiples of `1 / (2 D)`), within 2 %.
- Key Track 100 %: one octave up in the note halves the delay (the first notch doubles).
- Feedback +95 % and -95 % with a full-scale sweep stay bounded and within +6 dB; the peaks fall on the expected frequencies for each sign.
- Retrigger: identical output for two notes far apart; the sweep restarts at the note-on offset.
- Sync: 1/4 at 120 BPM gives a 500 ms cycle.
- A sweep from 0.1 to 10 ms produces no step larger than the signal step.
- Generic tests from `RegisteredEffectsTests.cpp`.

## 6. Out of scope

- Stereo spread between channels, other LFO shapes, a through-zero mode.

## 7. Open questions

None.
