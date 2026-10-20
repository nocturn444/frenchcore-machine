# Spec: Phaser effect

- Status: Approved (product owner's answers of 2026-10-20, questions G.1 A, G.2 A, 2.1 A and 2.2 A); implemented in version 0.3, stage 4. Ranges, defaults and voicing are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md), [fx-flanger](fx-flanger.md)

## 1. What it does for the user

A phaser: it digs a few notches into the sound and sweeps them up and down, which gives a distorted kick tail a moving, metallic, "swirling" colour. With Rate at 0 (or Depth at 0) it stops moving and becomes a fixed colour.

- **Stages**: 4, 8 or 12. More stages, more notches, a stronger effect.
- **Frequency**: where the sweep starts (its lowest point).
- **Rate** and **Sync**: how fast it sweeps. With Sync off, Rate is in Hz (sweeps per second); with Sync on, the knob picks a note value (1/64 to 1 bar, with triplets and dotted values) and follows the tempo, like the envelopes' Sync.
- **Depth**: how far up the sweep goes, up to four octaves above Frequency.
- **Feedback**: makes the notches deeper and the peaks between them sharper, towards a whistle.
- **Key Track**: 0 % keeps Frequency fixed; 100 % moves it with the note you play (one octave per octave, relative to C3), so the colour sounds the same on every note.

**The sweep starts from its lowest point at every note**, so every kick sounds exactly the same. It has the common **On** and **Mix** controls (Mix starts at 100 %; the notches are already part of the effect's own sound, so 100 % is a full phaser and lower values blend in the untouched sound). It adds no delay. Both channels get the same phaser (the kick is mono; width is the Stereo effect's job).

## 2. User interface

A strip in any rack: menu Stages, knobs Frequency, Rate (or the note value when Sync is on), Depth, Feedback, Key Track, switch Sync. The Rate knob and the note-value knob take the same place: only the one that applies is shown (`shownWhen`, [ADR 0011](../decisions/0011-effect-interface-additions.md)).

## 3. Parameters

Effect ID `phaser` (copies `phaser2` to `phaser4`). `defaultMix` = 1.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `phaser.stages` | Stages | 4, 8, 12 | 8 | | choice | crossfades (see §4) | yes |
| `phaser.freq` | Frequency | 20 to 10000, step 1 | 300 | Hz | skewed, midpoint 500 | yes (10 ms) | yes |
| `phaser.sync` | Sync | off, on | off | | toggle | per block | yes |
| `phaser.rate` | Rate | 0 to 20, step 0.01 | 0.5 | Hz | skewed, midpoint 2 | per block | yes; shown when Sync is off |
| `phaser.division` | Rate | 1/64, 1/32, 1/16T, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1 bar | 1/4 | | choice | per block | yes; shown when Sync is on |
| `phaser.depth` | Depth | 0 to 100, step 0.1 | 50 | % | linear | yes (10 ms) | yes |
| `phaser.feedback` | Feedback | 0 to 95, step 0.1 | 40 | % | linear | yes (10 ms) | yes |
| `phaser.keytrack` | Key Track | 0 to 100, step 0.1 | 0 | % | linear | per block | yes |

The `division` list is frozen in the effect (append-only), even if the envelopes' list of figures changes ([ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md) item 2).

## 4. DSP / implementation approach

- `fcm::dsp::PhaserEffect` in `src/dsp/fx/phaser/`, one registration line.
- A chain of first-order all-pass filters (TPT one-pole all-pass, Zavalishin "The Art of VA Filter Design" ch. 3) all tuned to the same frequency `f(t)`, with feedback from the chain's output to its input; the effect's output is `(dry + chain) / 2`, which puts a notch every two stages (Smith, "Physical Audio Signal Processing", phasing with first-order all-pass filters). The level is then scaled by `1 - 0.5 * feedback` so the peaks stay within +6 dB at 95 %.
- `f(t) = Frequency * 2^(4 * depth * l(t)) * 2^(keytrack * note / 12)`, with `l(t) = (1 - cos(2 pi phase)) / 2` (0 at the start of each cycle, so the sweep starts at Frequency), `note` from `EffectContext::noteSemitones`; limited to 0.45 of the sample rate. Coefficients are updated every 8 samples (the LFO is slow; this keeps the cost low) and per sample while Frequency or Depth are smoothing.
- The LFO is `dsp::TempoLfo` ([ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md)): its phase restarts at every `noteOn`, Rate 0 holds it at the start.
- A **Stages** change crossfades over 20 ms between the old and the new chain (both run during the fade), so it never clicks. All twelve stages are allocated in `prepare()`.
- No oversampling; `latencySamples` is 0. Non-finite input becomes 0.

## 5. Tests

- Rate 0, Depth 0, Feedback 0: the response has `stages / 2` notches, the first near Frequency's expected position (within 5 %), each deeper than 30 dB.
- Retrigger: two notes far apart in time give identical output for the same input (bit-identical after the note-on); a note-on delivered at offset `k` restarts the sweep at sample `k`.
- Sync: with Sync on at 120 BPM and 1/4, one cycle lasts 500 ms (within one block of control rate); a tempo change is followed from the next block.
- Key Track 100 %: the notch moves up an octave when the note goes up an octave.
- Feedback 95 % with a full-scale sweep stays bounded and within +6 dB.
- Changing Stages while playing produces no step larger than the signal step.
- Generic tests from `RegisteredEffectsTests.cpp`.

## 6. Out of scope

- Stereo phase offset between channels, other LFO shapes, a free-running or bar-locked LFO (answer G.1 A).

## 7. Open questions

None. Decided while implementing (technical; the user-facing description above holds):
- **Frequency is where the first notch sits at the start of the sweep.** The all-pass stages are tuned to `Frequency / tan(pi / (2 * stages))` (so the chain's phase is -180 degrees at Frequency); tuning them to Frequency itself would put the first notch at 0.41, 0.20 or 0.13 of it. The sweep, Depth and Key Track then move that point as written in section 4.
- **The level scaling is `min(1, 2 / peak)`** with `peak = (1 + 1 / (1 - feedback)) / 2`, the gain of the highest peak between the notches. The first draft (`1 - 0.5 * feedback`) left the peaks near +15 dB at 95 %, which the +6 dB test above forbids. Up to Feedback 67 % nothing is scaled.
- **Feedback sharpens the peaks between the notches; it makes the notches shallower, not deeper** (with feedback the output at the notch is no longer exactly zero: about -17 dB at 40 %). The user guide says so.
