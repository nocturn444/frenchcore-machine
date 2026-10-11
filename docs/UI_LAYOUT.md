# UI Layout

- Status: Draft v1, from the product owner's sketch (2026-10-11). Red areas are pending discussion.

This document describes the functional layout of the editor. Visual styling (colours, fonts, knob graphics) is a separate, later task and is not specified here.

![UI skeleton v1](images/ui-sketch-v1.png)

In the sketch, **black** areas are mandatory and **red** areas are still to be discussed.

## Principles

- Function first: every MVP feature is reachable from the main window without menus.
- Resizable with a fixed aspect ratio; all positions are relative to the window size.
- All styling goes through a custom `LookAndFeel`, so the visual redesign does not move components.
- Double-click any knob to reset it to default; Ctrl/Cmd-drag for fine adjustment.
- Each area is its own component (`fcm::ui::<Area>Panel`), so areas can be added, removed or moved without touching the others.

## Grid

The editor is a 4-column grid. Rows from top to bottom:

```
┌──────────────────────────────────────────────────────────────────────┐
│ Preset bar: SAVE · SAVE AS · SELECT · INIT · preset name             │
├────────────────┬────────────────┬────────────────────────────────────┤
│ Oscillator     │ (Panel B)      │                                    │
│  voice header  │  red, TBD      │                                    │
│  wavetable     │                │             FX rack                │
│  display+knobs │                │                                    │
├────────────────┴────────────────┤                                    │
│ Amp envelope   │ Pitch envelope │                                    │
├────────────────┼────────────────┼──────────────────┬─────────────────┤
│ (LFO 1/2)      │ Tock sampler   │ (Punch/Tail bal.) │ (Global)        │
│  red, TBD      ├────────────────┴──────────────────┤  red, TBD       │
│                │ (Other) red, TBD                  │                 │
└────────────────┴───────────────────────────────────┴─────────────────┘
```

## Areas

### Mandatory (black)

| Area | Contents | Maps to |
|---|---|---|
| **Preset bar** | `SAVE` (overwrite current user preset), `SAVE AS`, `SELECT` (browser), `INIT` (reset to the default patch), current preset name with prev/next | Presets (ARCHITECTURE §5) |
| **Voice header** | Pitch, Octave, Fine tune; voice settings (key track, glide on/time, velocity) in a compact row or popup | Voice (§4.1) |
| **Wavetable display** | Current frame drawn as a waveform; click to load a wavetable; frame name | Oscillator (§4.2) |
| **Wavetable parameters** | 6 knobs. Proposed: Position, Level, Phase, Unison/Detune (TBD), Wavetable select, plus one spare | Oscillator (§4.2) |
| **Amp envelope (AHDSR)** | Envelope display + 5 knobs (Attack, Hold, Decay, Sustain, Release). Default shape: kick + bass | Envelopes (§4.4) |
| **Pitch envelope** | Envelope display + 5 knobs. Proposed: Amount (st), Attack, Decay, Curve, End pitch | Envelopes (§4.4) |
| **Tock sampler** | Load a tock/click sample (factory list + user files), level, tune, start, decay, filter | Click layer (§4.3), extended to a sampler |
| **FX rack** | Reorderable list of effect slots (drag handle, enable, mix) with the selected effect's panel below | FX chain (§4.5) |

### To discuss (red)

| Area | Sketch shows | Open question |
|---|---|---|
| **Panel B** (next to the oscillator) | Same structure as the oscillator: two header rows, a triangle-shaped display, 6 knobs | A second oscillator, a second wavetable layer, or something else? |
| **LFO 1 / LFO 2** | Display, 3 knobs, 2 toggles, tabs for LFO1/LFO2 | Requires a modulation system, currently out of MVP scope. |
| **Punch / Tail balance** | Single area | Macro that balances the level of the punch (transient) against the tail (bass). |
| **Other** | Wide empty area | Contents to be defined. |
| **Global** | Narrow column | Proposed: output gain, safety limiter, oversampling, glide, velocity. |

## Component tree (initial)

```
FrenchcoreEditor
├── PresetBar
├── OscillatorPanel
│   ├── VoiceHeader
│   ├── WavetableDisplay
│   └── KnobRow (6)
├── EnvelopePanel (amp)
├── EnvelopePanel (pitch)
├── TockSamplerPanel
├── FxRackPanel
│   ├── FxSlotList (drag to reorder)
│   └── FxDetailPanel (selected effect, auto-generated or custom)
└── [pending] PanelB, LfoPanel, PunchTailPanel, OtherPanel, GlobalPanel
```

## Revision history

| Version | Date | Change |
|---|---|---|
| v1 | 2026-10-11 | First layout from the product owner's sketch. |
