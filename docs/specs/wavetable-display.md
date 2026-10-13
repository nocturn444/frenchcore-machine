# Spec: Wavetable display

- Status: Implemented (Phase 4, third part; layout and colours are first choices)
- Owner: nocturn4 (product), implemented by agent session
- Related: [wavetable-factory](wavetable-factory.md), [wavetable-user](wavetable-user.md), [UI_LAYOUT](../UI_LAYOUT.md)

## 1. What it does for the user

Between each oscillator's Pitch / Octave / Fine strip and its knobs there is a display that shows what the oscillator plays:

- the **name** of the wavetable (a factory name, or "User: <file name>"; "User (empty, plays the Sine)" when User is chosen with nothing loaded);
- a faint **stack of its waveforms** (up to 12, evenly spread across the table), so you see how the wave changes from the first to the last;
- in bright blue, the **waveform at the current Position**, blended between the two neighbouring waveforms exactly like the oscillator does. Moving the Position knob moves the bright line live.

It is a display only: it has no controls.

## 2. User interface

The strip under the controls now holds, for each oscillator, the display above its Load button, and one line of messages along the bottom. The window is about 150 pixels taller than in the first Phase 3 version (the strip is 150 pixels high at the default size); it scales like the rest.

## 3. Parameters

None. It reads `osc1.position` / `osc2.position` and the wavetable of the oscillator.

## 4. DSP / implementation approach

- No DSP. `core::WavetableSelector` keeps a copy of the raw frames it last published (`displayFrames()`), its `displayName()`, and announces every change through a `ChangeBroadcaster`. The display repaints on that and on Position changes (a `ParameterAttachment`).
- `core::interpolatedFrame(frames, frameSize, position)` computes the bright line: a linear blend between the two neighbouring frames, position clamped to 0..1 and a non-finite position taken as 0. It is a pure function so it is tested without a window.
- Memory: the copy of the frames is at most 2 MB per oscillator.

## 5. Tests

- `interpolatedFrame`: position 0 gives the first frame, 1 the last, 0.5 between two frames the exact average, a single-frame table always the same frame, a non-finite or out-of-range position is safe, an unusable input (empty, wrong length) gives an empty result.
- `WavetableSelector`: `displayFrames()` equals the factory frames of the selected entry and `displayName()` its name; after loading a user wavetable the frames and "User: name" are shown, and "User (empty, plays the Sine)" with none; every change is announced.

Manual check (Standalone): choose each wavetable and move Position; load a WAV.

## 6. Out of scope

- Editing or drawing the waveform; a 3D view; a spectrum view.
- Showing the Tock or the envelopes here.
