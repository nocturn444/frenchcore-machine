# User Guide

> This guide grows with each feature. Sections are filled in by the pull request that implements the feature.

## Installation

_Filled in with the first release._

## Quick start: a kick in one minute

_The full quick start is written when the MVP features land. What works today:_

1. Load Frenchcore Machine as an instrument on a MIDI track, or open the Standalone app and use its on-screen keyboard (see [Standalone](#standalone-playing-without-a-daw)).
2. Play a MIDI note. You hear a kick: a pure sine that starts four octaves higher and falls to the pitch of your note in a fraction of a second, with a punch and a bass body that you can reshape in the envelopes. That fast fall is what gives the kick its punch.
3. Play a lower or higher note to retune the kick, or turn **Key Track** off to make every note play the same pitch (the **Root Note**).
4. Make the note longer or shorter in the piano roll to control how long the tail rings.

The controls are a plain list for now; the real layout comes later.

## Standalone: playing without a DAW

The Standalone app (the version that runs by itself, without a DAW) has a strip at the bottom of the window so you can hear the kick right away. The VST3 and AU versions do not have it, because your DAW already gives you notes and tempo.

- **Keyboard.** Click the keys with the mouse, or play them with your computer keyboard: the letters `A W S E D F T G Y H U J K O L P ;` work like piano keys starting from C1 (the home row `A S D F G H J K L ;` are the white keys, the row above them are the black keys). `Z` moves the letters one octave down and `X` one octave up. The computer keys work as long as the window is active, except while you are typing a number in the BPM box.
- **Auto-play.** Switch it on and the kick repeats by itself, one per beat, so you can change the sound while it loops. **BPM** sets the tempo (60 to 300, default 200); drag it or type a number, and double-click it to go back to 200. Each note is held for 80 % of the beat, so the tail rings almost until the next kick.
- While Auto-play is on, pressing a key does not play a note: it chooses the pitch of the loop, which changes at the next kick. The loop starts on C1 until you choose another note.
- Auto-play and the BPM are not saved: every time you open the Standalone it starts with Auto-play off and 200 BPM.

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

A frenchcore kick has a **punch** (the hit at the start) and a **tail** (the bass that rings after it). This control balances them without redrawing the envelope. It sits under the knobs of the Amp Envelope panel.

- **Punch/Tail** goes from -100 to +100. At 0 nothing changes. Towards -100 the tail gets quieter, down to silence at -100 (the punch is favoured). Towards +100 the punch gets quieter, down to silence at +100 (the tail is favoured).
- **Split** is the time, in milliseconds after the note starts, where the punch ends and the tail begins (default 80 ms). A dashed red line on the Amp Envelope display shows it.
- The change from one part to the other takes about 4 ms, so it never clicks, and the split is counted from the start of each kick, also when you retrigger.

## Envelopes

Two envelopes shape every kick, and you can draw and tune both. Each has a display, five knobs and a menu for ready-made shapes. Everything you change is heard right away, also while a kick is playing (turn on Auto-play in the Standalone to hear it loop).

### Amp envelope (volume)

It is the volume of the kick over time. The default shape is **Kick + Bass**: it starts instantly (in 1 ms, so there is no click), holds a strong punch for 25 ms, drops quickly to 40 %, and rises again to a bass body at 70 % that stays for as long as the key is down. When you let go of the key the sound fades out (30 ms by default), so the length of the note in the piano roll sets the length of the tail.

| Knob | What it does |
|---|---|
| **Attack** | Time to reach full level. |
| **Hold** | How long it stays at full level. |
| **Decay** | Time from the end of the hold to the sustain level. |
| **Sustain** | The level kept while the key is down (the last point of the line). At 0 the kick is a one-shot that ends by itself, whatever the note length. |
| **Release** | Fade-out time after you let go of the key. |

**Time Scale** (small slider under the knobs) makes the whole envelope longer or shorter: 2 doubles every time, 0.5 halves them. It applies from the next note.

### Pitch envelope

It is how far above the note's pitch the kick starts and how it falls onto the note. This fast fall gives the kick its punch.

| Knob | What it does |
|---|---|
| **Amount** | How many semitones above the note the sweep starts (0 to 96, default +48). |
| **Attack** | Time to rise to the full amount. At 0 the kick starts at the top. |
| **Decay** | Time to fall to the end level (default 90 ms). |
| **Curve** | How the fall bends: -1 falls fast first and then slowly, 0 is a straight line. |
| **End** | Where the fall lands, as a share of Amount. At 0 it lands exactly on the note; above 0 it stays above the note. |

The pitch envelope always finishes its fall, even if you let go of the key. It also has a **Time Scale**.

### Drawing on the display

- **Drag a point** to move it in time and level. A point cannot go past its neighbours. The first point stays at time 0 (and at level 0 on the amp envelope).
- **Double-click on empty space** to add a point on the line, between the first and the last point (up to 32 points).
- **Double-click a small white point** to delete it. The big gold points are the ones the knobs move and cannot be deleted.
- **Drag a segment up or down** to bend it: dragging up makes the line bulge upward.
- The knobs and the points are the same line: turning a knob moves the gold points, and the white points you added stretch along with the section they are in.
- **Load shape...** opens a menu of ready-made shapes. For the amp: Kick + Bass, Short Punch and Long Tail. For the pitch: Classic Drop, Short Punch and Long Tail. Loading one replaces the current shape.

The shapes are saved with your project, so the kick comes back exactly as you left it. The two envelopes cannot be automated point by point, but Time Scale and Amount can.

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
| **Velocity Sensitivity** | How much MIDI velocity changes the loudness: at 0 % (the default) every note is equally loud, at 100 % a soft note is much quieter than a hard one. |

How notes behave:

- The plugin plays one note at a time. A new note restarts the kick, and it does so without clicks even if the previous kick is still sounding.
- The length of the note sets the length of the tail. When you let go of the last key the kick fades out over the amp envelope's Release (30 ms by default), so a long note in the piano roll gives a long tail and a short note a short one. A shape with a Sustain of 0 (like Short Punch) is a one-shot that ends by itself.
- If you hold one note, press another and release the second, the pitch goes back to the first note without restarting the kick.
