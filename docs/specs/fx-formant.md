# Spec: Formant filter

- Status: Approved (product owner's answers of 2026-10-20, questions 4.1 A, 4.2 A and 4.3 A); implemented in version 0.3, stage 3. Formant table, ranges and defaults are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [fx-filter](fx-filter.md)

## 1. What it does for the user

A filter that makes the sound "talk", as if it said a vowel. On a distorted kick tail it gives the tail a voice-like colour; automating Morph makes it move between vowels.

- **Vowel**: A, E, I, O or U.
- **Morph**: glides from the chosen vowel to the next one in the list (A to E, E to I, I to O, O to U, U back to A). 0 % is the chosen vowel, 100 % the next one. Automate it to make the sound "speak".
- **Shift**: moves all the vowel's resonances lower or higher, up to an octave each way, like a deeper or a higher voice. It does not follow the played note: the vowels stay where they are, like a real voice.
- **Resonance**: how sharp and "vocal" the vowel sounds; low values are softer and wider.
- **Drive**: a soft saturation before the filter, like the Filter's; 0 % is clean.

It has the common **On** and **Mix** controls (Mix starts at 100 %). It adds no delay.

Tip for the user guide: a vowel keeps the middle frequencies and removes most of the sub, so on a whole kick use it on a bus with Apply To Tail, or lower its Mix.

## 2. User interface

A strip in any rack: menu Vowel, knobs Morph, Shift, Resonance, Drive. It is its own effect in the "+ Add effect" menu, not a type of the Filter.

## 3. Parameters

Effect ID `formant` (copies `formant2` to `formant4`). `defaultMix` = 1.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `formant.vowel` | Vowel | A, E, I, O, U | A | | choice | crossfades (see §4) | yes |
| `formant.morph` | Morph | 0 to 100, step 0.1 | 0 | % | linear | yes (10 ms) | yes |
| `formant.shift` | Shift | -12 to +12, step 0.01 | 0 | st | linear | yes (10 ms) | yes |
| `formant.resonance` | Resonance | 0 to 100, step 0.1 | 50 | % | linear | yes (10 ms) | yes |
| `formant.drive` | Drive | 0 to 100, step 0.1 | 0 | % | linear | yes (10 ms) | yes |

## 4. DSP / implementation approach

- `fcm::dsp::FormantEffect` in `src/dsp/fx/formant/`, one registration line.
- Three band-pass filters in parallel, one per formant F1, F2, F3 (TPT state variable filters, as in the Filter, [fx-filter](fx-filter.md) §4), each with its own gain, summed.
- Formant table (an adult male voice, from the average formant frequencies of Peterson and Barney, "Control methods used in a study of the vowels", JASA 1952; frequencies in Hz, gains in dB relative to F1):

  | Vowel | F1 | F2 | F3 | Gains |
  |---|---|---|---|---|
  | A (as in "father") | 730 | 1090 | 2440 | 0, -5, -15 |
  | E (as in "bet") | 530 | 1840 | 2480 | 0, -8, -14 |
  | I (as in "beet") | 270 | 2290 | 3010 | 0, -12, -16 |
  | O (as in "bought") | 570 | 840 | 2410 | 0, -2, -18 |
  | U (as in "boot") | 300 | 870 | 2240 | 0, -10, -22 |

  The gains are the agent's first voicing, to be adjusted by ear. Bandwidths at Resonance 50 %: 80, 100 and 120 Hz; Resonance scales them by `2^(1 - 2 r)` (`r` = Resonance / 100: twice as wide at 0 %, half as wide at 100 %).
- Morph interpolates between the chosen vowel and the next: frequencies in the logarithmic domain, gains in dB. Shift multiplies all three frequencies by `2^(shift / 12)`; each frequency is limited to 0.45 of the sample rate.
- A **Vowel** change crossfades over 20 ms between the old and the new settings (the filters move to the new target), so switching it never clicks.
- Level: a fixed make-up gain per vowel, computed when the table is built, so full-scale pink noise comes out at the same level for every vowel (within 1 dB) at Resonance 50 %.
- Drive: the Filter's `tanh(a x) / tanh(a)` with `a = 15 d^2`, before the bank. Not oversampled; `latencySamples` is 0.
- Non-finite input becomes 0.

## 5. Tests

- Each vowel at Morph 0, Shift 0: with white noise, the measured spectrum has its three highest peaks within 10 % of F1, F2, F3 of the table.
- Morph 50 % sits between the two vowels (each formant's peak lies between the two table values); Shift +12 doubles the peak frequencies (within 5 %).
- Pink noise level per vowel within 1 dB of the others at Resonance 50 %; Resonance 100 % narrows the peaks (the -3 dB width of F1 shrinks).
- Switching Vowel during a sine produces no step larger than the signal step (the 20 ms crossfade).
- Drive 0 is clean; drive bounded for a full-scale sweep.
- Generic tests from `RegisteredEffectsTests.cpp`.

## 6. Out of scope

- Key Track of Shift (answer 4.3 A), more vowels or a female/child table, a "talking" LFO (automate Morph, or a future modulation system).

## 7. Open questions

None. Decided while implementing (technical, no change of what the user hears or controls):
- The make-up gain brings the pink noise of every vowel to 6 dB below its input at Resonance 50 % (it comes out between +6 and +9 dB at the formant peaks), computed from the table with the analog prototype of the three band-passes.
- Morph and Shift are smoothed over 20 ms together with Vowel (not 10 ms), so every move of the formants glides the same way; Resonance and Drive are smoothed over 10 ms.
- The widest band is limited to a damping of 2 (Q of 0.5) so a very wide band stays a band.
- Drive is the Filter's saturation, repeated in the effect's own folder so effects do not depend on each other.
