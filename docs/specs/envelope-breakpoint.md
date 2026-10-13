# Spec: Breakpoint envelopes (amp and pitch)

- Status: Implemented (default amp shape and maximum length confirmed by the product owner on 2026-10-11; the other values are first choices listed in section 7)
- Owner: nocturn4 (product), implemented by agent session
- Related: [voice-note-handling](voice-note-handling.md), [voice-punch-tail](voice-punch-tail.md), [ARCHITECTURE §4.4, §5, §6](../ARCHITECTURE.md), [ADR 0006](../decisions/0006-envelope-model-and-publishing.md)

## 1. What it does for the user

Two envelopes shape every kick, and both can be drawn and tuned:

- **Amp envelope**: the volume of the kick over time.
- **Pitch envelope**: how far above the note's pitch the kick starts, and how it falls onto the note. This is the "drop" that gives the kick its punch.

Each envelope is a line made of **points** joined by **segments**. You can drag the points, add new ones, delete the extra ones and bend each segment. Five **knobs** next to each display move the main points of the same line, so you can work with knobs or by drawing, and both always agree.

### Amp envelope

The default shape is **Kick + Bass**: instant attack (1 ms, to avoid a click), a strong punch held for 25 ms, a quick drop to 40 % by 80 ms, then a rise to a 70 % bass body by 140 ms. The body is held while the note is held. When you let go of the key the sound fades out over a short release (30 ms) from wherever it is. So the length of the note in the piano roll sets the length of the tail.

Knobs: **Attack**, **Hold**, **Decay**, **Sustain**, **Release**.

- *Attack*: time to reach full level. *Hold*: how long it stays at full level. *Decay*: time from the end of the hold to the sustain level. *Sustain*: the level held while the key is down (the last point of the line). *Release*: fade-out time after the key is let go.
- A sustain of 0 gives a one-shot kick that ends by itself (the "Short Punch" preset).

### Pitch envelope

The default shape is the current drop: starts at the full **Amount** (+48 semitones), falls in 90 ms with a fast-then-slow curve, and settles exactly on the note's pitch.

Knobs: **Amount**, **Attack**, **Decay**, **Curve**, **End**.

- *Amount*: how many semitones above the note the sweep starts (0 to 96). *Attack*: time to rise from 0 to the full amount (0 means it starts at the top). *Decay*: time to fall to the end value. *Curve*: how the fall bends (negative: fast first, then slow). *End*: where the fall lands, as a fraction of Amount (0 lands on the note; above 0 stays above the note).
- The pitch envelope does not react to note-off: it always finishes its fall.

### Tempo sync

Each envelope has a **Sync** switch next to its shape menu. With Sync on, the envelope's times follow the tempo and the milliseconds are replaced by musical figures:

- The time knobs (amp: Attack, Hold, Decay, Release; pitch: Attack, Decay) become steps through note values: `1/64, 1/32, 1/16T, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1 bar` (T is triplet, D is dotted), preceded by `0` where the knob can be zero and by `Min` (the shortest allowed time, 1 ms) where it cannot. Only figures that fit the knob's range are offered. The other knobs (Sustain, Curve, End, Amount) are unchanged.
- The time grid of the display is labelled with fractions of a bar (`1/16`, `1/8`, `1/4`, ...) instead of milliseconds, and points snap to 1/32 notes in time while you drag them.
- The envelope **keeps its musical lengths when the tempo changes**: a Hold of 1/16 is a sixteenth note at 120 BPM and at 200 BPM. The change applies from the next note.
- The tempo is the host's tempo in a DAW, the **BPM** control of the strip in the Standalone, and 120 BPM when the host gives none.
- Turning Sync on keeps the sound as it is (the times stay what they are at the current tempo; the knobs show the nearest figure). The first time you turn a time knob, that time becomes an exact figure. Turning Sync off keeps the sound too and goes back to milliseconds.
- The Punch / Tail **Split** stays in milliseconds (see section 7).

### Editing the display

- **Drag a point** to move it in time and level (in time it snaps to 1/32 notes when Sync is on). Points cannot cross their neighbours in time. The first point stays at time 0 (and at level 0 on the amp envelope, so a kick never starts with a click).
- **Double-click on empty space** to add a point on the line at that time (up to 32 points; nothing can be added after the last point).
- **Double-click an extra point** to delete it. The main points (the ones the knobs move) cannot be deleted.
- **Drag a segment up or down** to bend it. Dragging up makes the line bulge upward.
- **Knobs move the main points.** Extra points between main points keep their relative position in time (they stretch with the section they sit in) and keep their level.
- A **preset menu** per envelope loads a ready-made shape.
- On the amp display, a dashed vertical line shows the [Punch / Tail split](voice-punch-tail.md).

Shapes are part of the saved state (presets and projects) but are not automatable, so a DAW cannot move individual points; the global controls below are automatable.

## 2. User interface

The editor has two `EnvelopePanel`s (amp left, pitch right) under the oscillators. Each has the preset menu ("Load shape..."), the Sync switch, the display, and one row of knobs: the five shape knobs and a Time Scale knob ([ui-grid-layout](ui-grid-layout.md)).

## 3. Parameters

Automatable (APVTS). The five knobs per envelope and the points are **not** parameters; they edit the shape state.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `ampEnv.timeScale` | Amp Env Time Scale | 0.25 to 4 | 1 | x | skew, centre 1 | no (read at each note-on) | yes |
| `pitchEnv.timeScale` | Pitch Env Time Scale | 0.25 to 4 | 1 | x | skew, centre 1 | no (read at each note-on) | yes |
| `pitchEnv.amount` | Pitch Env Amount | 0 to 96, step 0.1 | 48 | semitones | linear | yes (20 ms) | yes |

Time scale multiplies every time of the shape, including the release (2 makes the envelope twice as long).

Knob ranges (editing the shape; every time is in milliseconds before time scale; the total length of a shape never exceeds 2000 ms, a longer request is clamped):

| Envelope | Knob | Range | Default | Maps to |
|---|---|---|---|---|
| Amp | Attack | 1 to 500 ms | 1 | time of point 1 |
| Amp | Hold | 0 to 1000 ms | 24 | point 2 minus point 1 |
| Amp | Decay | 0 to 2000 ms | 115 | last point minus point 2 |
| Amp | Sustain | 0 to 100 % | 70 | level of the last point |
| Amp | Release | 1 to 2000 ms | 30 | release time (not a point) |
| Pitch | Amount | 0 to 96 st | 48 | parameter `pitchEnv.amount` |
| Pitch | Attack | 0 to 200 ms | 0 | time of point 1 |
| Pitch | Decay | 1 to 1000 ms | 90 | last point minus point 1 |
| Pitch | Curve | -1 to +1 | -1 | curvature of the segment leaving point 1 |
| Pitch | End | 0 to 100 % | 0 | level of the last point |

Sync is part of the envelope state, not a parameter (it travels with the shape, like the points), so a DAW cannot automate it.

## 4. DSP / implementation approach

### 4.1 Model (`fcm::dsp::BreakpointEnvelope`)

A plain value type, edited on the message thread only.

- `EnvelopePoint { timeMs, value (0..1), curve (-1..+1) }`; `curve` belongs to the segment that leaves the point towards the next one. 2 to 32 points, sorted by time; equal times are allowed (a vertical jump).
- Times are clamped to 0..2000 ms and levels to 0..1; the first point is at time 0.
- Amp envelopes have at least 4 points and pitch envelopes at least 3. The **main points** are index 1, index 2 and the last point (amp), index 1 and the last point (pitch); the first point is fixed. All others are **extra points**.
- Operations: `addPoint`, `movePoint` (clamped between the neighbours' times), `removePoint` (extras only), `setCurve`, `setReleaseMs`, and the knob accessors. A knob change recomputes the main points' times and moves each extra point to the same fraction of the segment between its surrounding main points.
- `toShape()` builds the immutable audio-side copy.

### 4.1b Tempo sync (see ADR 0007)

- The model stores `syncRefBpm` (0 = off, otherwise 20 to 400): the tempo its times were written at. `enableSync(bpm)` records the tempo and changes no time. `rebaseTo(bpm)` multiplies every time and the release by `reference / bpm` (clamped to the 2000 ms limits) and makes `bpm` the reference. `disableSync(bpm)` rebases and clears the reference.
- `EnvelopeShape` carries `syncRefBpm`. At each note-on the voice scales the envelope by `syncRefBpm / bpm` (times the time scale); the renderer is unchanged.
- `MusicalTime` holds the table of figures (name and length in quarter-note beats) and builds the list of choices for a knob: label and milliseconds at a tempo, within the knob's range; `nearestChoice` finds the one closest to a given time.
- The editor in sync mode labels the grid in fractions of a bar and snaps dragged points to 1/32 notes (computed from the reference tempo, which equals the current tempo after the first edit).

### 4.2 Shape and renderer

- `EnvelopeShape`: a fixed-size POD (32 points, release time, count). Nothing in it allocates.
- Segment curve: with `x` the progress (0..1) through the segment and `c` its curvature, `f(x) = x` for `c = 0` and `f(x) = (e^(5 c x) - 1) / (e^(5 c) - 1)` otherwise; the level is `v0 + (v1 - v0) * f(x)`. At `c = -1` on a falling segment this is the fast-then-slow fall of the Phase 2 pitch envelope, so the default pitch shape sounds the same as before.
- `valueAt(timeMs)`: the last point whose time is not after `timeMs`; if it is the last point, its level; otherwise the curve between it and the next. Equal times therefore jump to the later point.
- `EnvelopeRenderer` (audio thread): one per envelope per voice. Per sample it advances by `1000 / (sampleRate * timeScale)` ms (time scale latched at note-on) and keeps a segment index so the lookup is O(1) amortised.
  - **Amp use:** after the last point the level is held while the key is down; `release()` fades linearly from the current level to 0 in `releaseMs` and then the renderer is idle. If the held level is 0 the renderer goes idle at once. Release works at any moment, including before the end of the shape.
  - **Pitch use:** `release()` is never called; after the last point the last level is held forever.
- A shape can be swapped while a kick sounds: the renderer keeps its time position and continues on the new shape.

### 4.3 Publishing (see ADR 0006)

- `fcm::dsp::PublishedPointer<T>`: the message thread publishes a new `const T` with an atomic pointer exchange; the audio thread calls `acquire()` once per block, which also records the pointer in use (a hazard pointer). The message thread frees a retired object only when it is no longer the one in use. The audio thread never allocates, frees or locks.
- `fcm::core::EnvelopeState` owns the editable model and the publisher for one envelope, publishes after every edit, notifies listeners (the UI), and converts to and from a `juce::ValueTree`.

### 4.4 State

The processor stores the two envelopes as child nodes `AmpEnvelope` and `PitchEnvelope` of the saved state, each with its `releaseMs`, an optional `syncBpm` (absent means not synced) and a list of `Point` children (`time`, `value`, `curve`, `main`). A missing node means the default shape; an invalid node (too few or too many points, times out of order, non-finite numbers) is ignored with the default shape used instead. `stateVersion` stays 1: nothing has been released, and the nodes are optional.

### 4.5 Presets (data-driven)

`EnvelopePresets` is a table of named shapes per envelope kind; adding a preset is one table entry. Amp: **Kick + Bass** (default), **Short Punch**, **Long Tail**. Pitch: **Classic Drop** (default), **Short Punch**, **Long Tail**. The values are in the code table and the tests pin the defaults.

### 4.6 Tail length

The plugin reports to the host a tail of the amp release time times the amp time scale plus 50 ms.

## 5. Tests

Catch2, headless:

- Model: add keeps order and respects the 32-point limit; move is clamped between neighbours, the first point stays at 0 (and at level 0 for the amp), levels stay in 0..1; remove only removes extras and respects the minimum count; knob getters return what the setters set, extras keep their relative position, total length is clamped to 2000 ms; equal-time points are accepted.
- Shape: `valueAt` hits every point exactly, interpolates linearly with curve 0, is monotonic for a falling curve, matches the Phase 2 pitch formula for the default pitch shape, and handles equal times (jump) and times beyond the end.
- Renderer: attack/hold/decay/sustain levels at the right times at 44.1, 48 and 96 kHz (within 1 sample); sustain held until `release()`; release is linear from the current level and takes `releaseMs`; release during the attack; idle when the held level is 0; time scale stretches the times; pitch use holds the end level; deterministic after reset; a shape swap mid-note continues without a jump larger than the shapes differ by.
- `PublishedPointer`: acquire returns the latest; a retired object in use is not freed; old objects are freed on later publishes; no leaks at destruction; two-thread stress (100000 acquires against 20000 publishes) never sees a torn or freed object.
- State: round trip of every preset through `ValueTree` and through XML text; missing node gives the default; invalid nodes are rejected.
- Sync: `MusicalTime` figures and choices (range filtering, nearest choice, names); enabling sync changes no time; rebasing preserves the musical length (a 1/8 stays a 1/8 and its milliseconds scale with the tempo) and clamps; disabling keeps the sound; invalid reference tempos are rejected on load; the voice plays a synced shape written at 120 BPM twice as fast at 240 BPM (attack, hold, decay, release) and an unsynced one unchanged; the sync scale multiplies the time scale; the sync reference survives the saved form.
- Voice and engine: default Kick + Bass reaches the body level and holds it while the key is down, releases in 30 ms on note-off, and the pitch tail is still within 1 cent of the note; the default pitch drop is identical to the Phase 2 one; Short Punch ends by itself; amount and time scale parameters have the expected effect; editing the shape while sounding does not click.
- Core: the parameter IDs are pinned in the existing ID test.

Manual check: draw and bend a shape in the Standalone with Auto-play on and hear it change at once; knobs and points stay in sync; presets load; save and reopen the Standalone project/DAW project and the shapes come back.

## 6. Out of scope

- Looping envelopes, multiple sustain points, an envelope on anything but amp and pitch (modulation system, after the MVP).
- Host automation of individual points or the knobs.
- Undo/redo in the editor, snapping to a grid, copy and paste of shapes between envelopes.
- Zooming the display: it fits the shape up to the 2000 ms maximum.
- User-saved shape presets (they come with the preset system of Phase 6; the full state, including shapes, is already saved).

## 7. Open questions

0. Sync applies to the envelope times only. The Punch / Tail Split stays in milliseconds, because it is an automatable parameter and cannot follow the tempo the way a stored shape can. It can get the same treatment if wanted.

First choices of the agent for the product owner to confirm or change:

1. Short Punch and Long Tail shapes (amp and pitch) are invented; only the names come from the scope. Values: amp Short Punch is a 15 ms hold and a 105 ms curved fall to 0; amp Long Tail drops to 45 % by 300 ms and curves down to 0 at 900 ms; pitch Short Punch drops in 40 ms and Long Tail in 250 ms.
2. Release is a straight-line fade. A curved release is not offered.
3. Segment bending range is limited to the same exponential family at the extremes (curve -1 to +1).
