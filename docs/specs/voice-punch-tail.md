# Spec: Punch / Tail balance

- Status: Implemented (values in section 7 are first choices for the product owner to confirm)
- Owner: nocturn4 (product), implemented by agent session
- Related: [envelope-breakpoint](envelope-breakpoint.md), [voice-note-handling](voice-note-handling.md), [ARCHITECTURE §4.5](../ARCHITECTURE.md)

## 1. What it does for the user

A frenchcore kick has two parts: the **punch** (the first moments, the hit) and the **tail** (the bass that rings after). **Punch / Tail** lets you balance them with one knob, without redrawing the envelope.

- **Balance** goes from -100 to +100. At 0 nothing changes. Moving it **towards -100 favours the punch**: the tail gets quieter, down to silent at -100. Moving it **towards +100 favours the tail**: the punch gets quieter, down to silent at +100.
- **Split** is the time (in milliseconds after the note starts) where the punch ends and the tail begins. A dashed line on the amp envelope display shows it. Default 80 ms.
- **Punch Level** and **Tail Level** are two knobs under the Split slider that set the volume of each part on its own, from -48 dB (silent) to +12 dB, at 0 dB by default. They work together with Balance: the level is applied on top of whatever Balance does, so you can, for example, lift the tail by 3 dB and still use Balance to trade between the two.
- The change between the two regions is smooth (about 4 ms), so there is never a click.

Both controls are automatable.

## 2. User interface

Two sliders (Balance, Split) and, under them, two knobs (Punch Level, Tail Level) in the amp envelope panel (Phase 3), later in the Punch / Tail panel of [UI_LAYOUT.md](../UI_LAYOUT.md). The split marker is drawn on the amp envelope display, at the split time converted to the envelope's own time (divided by the amp time scale).

## 3. Parameters

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `punchTail.balance` | Punch Tail Balance | -100 to +100, step 1 | 0 | % | linear | yes (10 ms) | yes |
| `punchTail.split` | Punch Tail Split | 5 to 500, step 1 | 80 | ms | skew, centre 80 ms | yes (10 ms) | yes |
| `punchTail.punchLevel` | Punch Level | -48 to +12, step 0.1 (-48 is silence) | 0 | dB | linear | yes (10 ms) | yes |
| `punchTail.tailLevel` | Tail Level | -48 to +12, step 0.1 (-48 is silence) | 0 | dB | linear | yes (10 ms) | yes |

## 4. DSP / implementation approach

- Gains: with `b = balance / 100`, the balance gains are `b > 0 ? 1 - b : 1` for the punch and `b < 0 ? 1 + b : 1` for the tail. Each is multiplied by its level (`10^(dB / 20)`, silent at -48 dB): `punchGain = punchBalanceGain * punchLevel` and `tailGain = tailBalanceGain * tailLevel`.
- Each sounding kick counts the samples since its own note-on. The applied gain is `punchGain + (tailGain - punchGain) * s`, where `s` goes from 0 to 1 with a smoothstep across a window of `kCrossfadeMs` = 4 ms centred on the split time. The gain multiplies the voice after the amp envelope, so it also applies to the fading copy of a retriggered kick.
- The split time is absolute (milliseconds after note-on), not scaled by the envelope time scale.
- Balance and split are smoothed over 10 ms, so moving the knob while a kick sounds does not click.
- Real-time safe: a few multiplications per sample, no allocation.

## 5. Tests

- Balance 0 with both levels at 0 dB leaves the output bit-identical to a voice without the stage.
- Punch Level and Tail Level: -6 dB on the punch halves the punch region and leaves the tail untouched, and the other way round; levels multiply with Balance (balance -50 % and tail -6 dB gives a quarter of the tail); -48 dB silences the region; +6 dB doubles it; the level change is smoothed and does not click.
- Balance -100 silences the tail and leaves the punch untouched; +100 silences the punch and leaves the tail; ±50 gives half level in the quieter region.
- The transition is centred on the split and takes 4 ms: gains at split minus 2 ms and plus 2 ms are the two region gains, and the gain is monotonic in between.
- The split follows the parameter (changing it moves the transition) and is measured from each note's own start, also on retrigger.
- No click: the largest sample step with balance changes while sounding stays within the signal's own slope.
- Retrigger and chords keep their existing no-click tests with the stage on.

## 6. Out of scope

- A balance curve other than linear gain, or a separate level for each region in dB.
- More than two regions.
- Showing the effect in the display beyond the split marker.

## 7. Open questions

1. At -100 the tail is fully silent (and at +100 the punch). That is the pan-like behaviour of the architecture note; say if you would rather limit it to a gentler maximum cut.
2. The 80 ms default split and the 4 ms crossfade are first choices.
