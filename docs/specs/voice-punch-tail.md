# Spec: Punch / Tail split and levels

- Status: Implemented. The Balance control of the first version was removed on 2026-10-13 (product owner's decision, see section 7).
- Owner: nocturn4 (product), implemented by agent session
- Related: [envelope-breakpoint](envelope-breakpoint.md), [voice-note-handling](voice-note-handling.md), [ARCHITECTURE §4.5](../ARCHITECTURE.md)

## 1. What it does for the user

A frenchcore kick has two parts: the **punch** (the first moments, the hit) and the **tail** (the bass that rings after). This control lets you set the volume of each part on its own, without redrawing the envelope.

- **Split** is the time (in milliseconds after the note starts) where the punch ends and the tail begins. It does nothing else: it only decides which part of the kick the two levels below apply to. A dashed line on the amp envelope display shows it. Default 80 ms.
- **Punch Level** sets the volume of everything before the split, from -48 dB (silent) to +12 dB, 0 dB by default.
- **Tail Level** sets the volume of everything after the split, with the same range and default.
- With both levels at 0 dB nothing changes, wherever the split is.
- The change between the two parts is smooth (about 4 ms), so there is never a click.

All three controls are automatable.

## 2. User interface

A small "Punch / Tail" panel with the Split control and the two level knobs (the layout is in [UI_LAYOUT.md](../UI_LAYOUT.md)). The split marker is drawn on the amp envelope display, at the split time converted to the envelope's own time (divided by the amp time scale).

## 3. Parameters

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `punchTail.split` | Punch Tail Split | 5 to 500, step 1 | 80 | ms | skew, centre 80 ms | yes (10 ms) | yes |
| `punchTail.punchLevel` | Punch Level | -48 to +12, step 0.1 (-48 is silence) | 0 | dB | linear | yes (10 ms) | yes |
| `punchTail.tailLevel` | Tail Level | -48 to +12, step 0.1 (-48 is silence) | 0 | dB | linear | yes (10 ms) | yes |

## 4. DSP / implementation approach

- Each sounding kick counts the samples since its own note-on. The applied gain is `punchGain + (tailGain - punchGain) * s`, where `punchGain` and `tailGain` are the two levels as linear gains (`10^(dB / 20)`, silent at -48 dB) and `s` goes from 0 to 1 with a smoothstep across a window of `kCrossfadeMs` = 4 ms centred on the split time. The gain multiplies the voice after the amp envelope, so it also applies to the fading copy of a retriggered kick.
- The split time is absolute (milliseconds after note-on), not scaled by the envelope time scale.
- Split and levels are smoothed over 10 ms, so moving a knob while a kick sounds does not click.
- Real-time safe: a few multiplications per sample, no allocation.

## 5. Tests

- Both levels at 0 dB leave the output bit-identical whatever the split.
- Punch Level and Tail Level: -6 dB on the punch halves the punch region and leaves the tail untouched, and the other way round; -48 dB silences the region; +6 dB doubles it; a level change is smoothed and does not click.
- The transition is centred on the split and takes 4 ms: gains at split minus 2 ms and plus 2 ms are the two region gains, and the gain is monotonic in between.
- The split follows the parameter (changing it moves the transition) and is measured from each note's own start, also on retrigger.

## 6. Out of scope

- More than two regions.
- Showing the effect in the display beyond the split marker.
- Following the tempo: the Split stays in milliseconds even when the envelopes use tempo Sync (confirmed by the product owner on 2026-10-11).

## 7. Decisions taken

1. 2026-10-11 (product owner): with tempo Sync on, the Split stays in milliseconds. It marks where the hit ends and the bass begins, which is a matter of sound and not of musical time.
2. 2026-10-13 (product owner): the **Punch/Tail Balance** knob is removed. It changed the character of the sound too much and was hard to understand; the Split's only job is to divide the kick for the two levels. The parameter `punchTail.balance` no longer exists (it was never in a release). A project or preset saved with it simply ignores that entry.
3. The 80 ms default split and the 4 ms crossfade are first choices.
