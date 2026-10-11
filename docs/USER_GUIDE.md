# User Guide

> This guide grows with each feature. Sections are filled in by the pull request that implements the feature.

## Installation

_Filled in with the first release._

## Quick start: a kick in one minute

_The full quick start is written when the MVP features land. What works today:_

1. Load Frenchcore Machine as an instrument on a MIDI track (or open the Standalone; it needs a MIDI keyboard or a virtual MIDI source, there is no on-screen keyboard yet).
2. Play a MIDI note. You hear a kick: a pure sine that starts four octaves higher and falls to the pitch of your note in a fraction of a second, then fades out. That fast fall is what gives the kick its punch.
3. Play a lower or higher note to retune the kick, or turn **Key Track** off to make every note play the same pitch (the **Root Note**).

The controls are a plain list for now; the real layout comes later.

## Oscillators and wavetables

There are two identical oscillators, **Osc 1** and **Osc 2**. Osc 1 is on and Osc 2 is off when you start.

| Control | What it does |
|---|---|
| **On** | Switches the oscillator on or off, without clicks. |
| **Level** | How loud this oscillator is. |
| **Pitch**, **Octave**, **Fine** | Tune this oscillator against the kick's pitch, in semitones, octaves and cents (hundredths of a semitone). For example, set Osc 2 to Octave +1 to add a layer one octave up. |
| **Position** | Chooses the waveform inside the wavetable. The built-in sine has only one, so it does nothing yet. |
| **Phase** | Where in its cycle the oscillator starts at every note. The same value every time means every kick starts exactly the same way. |

The only wavetable for now is a pure sine. Factory wavetables and loading your own come in a later version.

## Tock

## Punch / Tail balance

## Envelopes

## Effects

## Presets

## Global: output, limiter, quality, glide

| Control | What it does |
|---|---|
| **Output Gain** | Overall volume, from -48 dB (silence) to +12 dB. |
| **Safety Limiter** | On by default. It keeps the sound from ever going above 0 dBFS (the loudest level your DAW can carry), bending loud peaks softly instead of clipping them hard. Switch it off only if you want the raw signal. |
| **Key Track** | On: the MIDI note sets the pitch the kick settles on. Off: every note settles on the **Root Note**. |
| **Root Note** | The pitch used when Key Track is off. Default C1 (about 65 Hz). |
| **Glide** and **Glide Time** | With Glide on, the pitch the kick settles on slides from the previous note to the new one over Glide Time instead of jumping. The fast pitch drop at the start of each kick is not affected. |
| **Velocity Sensitivity** | How much MIDI velocity changes the loudness: at 0 % every note is equally loud, at 100 % a soft note is much quieter than a hard one. |

How notes behave:

- The plugin plays one note at a time. A new note restarts the kick, and it does so without clicks even if the previous kick is still sounding.
- Releasing a key does not cut the kick; it always plays out. A short MIDI note still gives you the whole kick.
- If you hold one note, press another and release the second, the pitch goes back to the first note without restarting the kick.
