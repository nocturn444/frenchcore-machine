# Spec: Peak effect

- Status: Approved (product owner's answers of 2026-10-20, questions 5.1 A and 5.2 A); not implemented yet (version 0.3, stage 2). Ranges, defaults and voicing are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md), [fx-distortion](fx-distortion.md)

## 1. What it does for the user

A quick "kick distortion" modelled on the idea of the Kick FX section of Nimble Kick (Nimble Tools): it **boosts one zone of frequencies** and then **clips** the result, so that zone is what gets distorted. One pad does most of the work:

- left and right on the pad (**Freq**) chooses the zone, from the sub to the highs;
- down and up on the pad (**Hardness**) goes from a soft, round clip to a hard, square clip;
- **Amount** is how much the zone is boosted and how hard the sound is pushed into the clipper. At 0 the sound passes almost untouched.

It has the common **On** and **Mix** controls (Mix starts at 100 %). Like the Distortion, it is oversampled by the global Quality setting, so it adds a few samples of delay that the DAW compensates.

Our design is our own: Nimble Kick's author describes a gentle (6 dB per octave) peak filter before a clipper with a soft-to-hard pad, and this effect follows that description, not their code or sound.

## 2. User interface

A strip in any rack. Above the knobs, an **XY pad** (a decoration, [ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md)) about 80 points high and as wide as the strip: a dot whose horizontal position is Freq (logarithmic, labelled 50, 100, 200, 500, 1k, 2k, 5k) and vertical position is Hardness (bottom "Soft", top "Hard"). Drag the dot to move both; double-click resets both to their defaults. Under it, the knobs **Freq**, **Amount** and **Hardness** show and set the exact values; moving a knob moves the dot.

## 3. Parameters

Effect ID `peak` (copies `peak2` to `peak4`). `defaultMix` = 1.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `peak.freq` | Freq | 20 to 5000, step 1 | 150 | Hz | skewed, midpoint 300 | yes (10 ms) | yes |
| `peak.amount` | Amount | 0 to 100, step 0.1 | 50 | % | linear | yes (10 ms) | yes |
| `peak.hardness` | Hardness | 0 to 100, step 0.1 | 30 | % | linear | yes (10 ms) | yes |

## 4. DSP / implementation approach

- `fcm::dsp::PeakEffect` in `src/dsp/fx/peak/`, one registration line, one decoration entry (the XY pad, `ui::PeakPad`).
- **Peak**: a first-order band-pass around Freq (a one-pole high-pass followed by a one-pole low-pass, both at Freq; 6 dB per octave on each side, TPT one-poles, Zavalishin "The Art of VA Filter Design" ch. 3) added to the signal: `boosted = x + g * bp(x)`, with `g` taking the band from 0 to +18 dB at Amount 100 % (`g = 10^(18 a / 20) - 1` with `a` = Amount / 100, scaled by the band-pass's peak gain so the boost at Freq is exact).
- **Drive**: `boosted` is multiplied by `10^(24 a / 20)` (0 to +24 dB).
- **Clipper**: `y = (1 - h) * tanh(u) + h * clamp(u, -1, 1)`, `h` = Hardness / 100. Full scale stays at most full scale.
- Freq is limited to 0.45 of the (base) sample rate. Coefficients are recomputed per sample while smoothing, per block otherwise.
- Oversampled by the global Quality with the same `juce::dsp::Oversampling` polyphase IIR set-up as the Distortion ([fx-distortion](fx-distortion.md) §4); `latencySamples(factor)` is that latency (0 at 1x). The plugin's reported latency grows accordingly (four copies, like every effect, [ADR 0010](../decisions/0010-effect-instances-buses-and-mixer.md)); the DAW compensates it.
- Non-finite input becomes 0.

## 5. Tests

- Amount 0, Hardness 0: a -20 dBFS sine passes within 0.5 dB at 50 Hz, 500 Hz and 5 kHz (only the gentle tanh).
- Amount 100: with a -30 dBFS sine sweep before the clipper is reached, the response peaks at Freq (within a third of an octave) at +18 dB plus the drive, for Freq 100 Hz, 1 kHz and 4 kHz.
- Output never exceeds full scale (plus the oversampling filters' overshoot, below +0.5 dB) for a full-scale sweep at Amount 100, Hardness 0 and 100.
- Hardness 100 gives more odd harmonics than Hardness 0 for the same sine (THD measured).
- Latency: a click through the effect arrives at `latencySamples(factor)` within one sample, at every factor.
- XY pad: dragging writes Freq and Hardness through the APVTS (one gesture each); a knob change moves the dot.
- Generic tests from `RegisteredEffectsTests.cpp`.

## 6. Out of scope

- Key Track of Freq, a tone or output knob, more clip types (the Distortion has them).

## 7. Open questions

None.
