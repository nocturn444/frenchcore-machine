# UI Layout

- Status: v3. Layout agreed with the product owner (2026-10-11); implemented as a fixed design grid on 2026-10-13 ([spec](specs/ui-grid-layout.md)). "Other" is reserved, contents to be defined.

This document describes the functional layout of the editor. Visual styling (colours, fonts, knob graphics) is a separate, later task and is not specified here.

![UI skeleton v1](images/ui-sketch-v1.png)

In the original sketch, black areas were mandatory and red areas were to be discussed. The decisions on the red areas are recorded below.

## Principles

- Function first: every MVP feature is reachable from the main window without menus.
- Resizable with a fixed aspect ratio. The grid is drawn at one design size (1500 points wide) and the whole of it is scaled to the window, so nothing rearranges or squeezes at any size. It is wider than tall so it fits a screen.
- All styling goes through a custom `LookAndFeel`, so the visual redesign does not move components.
- Double-click any knob to reset it to default; Ctrl/Cmd-drag for fine adjustment.
- Each area is its own component (`fcm::ui::<Area>Panel`), so areas can be added, removed or moved without touching the others.

## Grid

```
┌──────────────────────────────────────────────────────────────────────┐
│ Preset bar: SAVE · SAVE AS · SELECT · INIT · preset name             │
├────────────────┬────────────────┬────────────────────────────────────┤
│ Oscillator 1   │ Oscillator 2   │                                    │
│  header        │  header        │                                    │
│  wavetable     │  wavetable     │             FX rack                │
│  display+knobs │  display+knobs │                                    │
├────────────────┼────────────────┤                                    │
│ Amp envelope   │ Pitch envelope │                                    │
├────────────────┼────────────────┼───────────────────┬────────────────┤
│ LFO 1/2        │ Tock           │ Punch / Tail     │ Global         │
│ (after MVP,    ├────────────────┴───────────────────┤                │
│  space kept)   │ Other (reserved)                   │                │
└────────────────┴────────────────────────────────────┴────────────────┘
```

## Areas

| Area | Contents | MVP | Maps to |
|---|---|---|---|
| **Preset bar** | Preset menu (factory, then yours) with the current name, prev/next, a name field with `SAVE` (asks before replacing). `INIT` is the first factory preset. See [preset-browser](specs/preset-browser.md) | Yes | Presets (ARCHITECTURE §5) |
| **Oscillator 1 and 2** | Two identical panels. Header: on/off, Pitch, Octave, Fine tune. Wavetable display (click to load, frame name). 6 knobs: Position, Level, Phase, plus three reserved for later oscillator features | Yes | Oscillators (§4.2) |
| **Amp envelope** | Editable display (drag, add, delete points) + 5 knobs: Attack, Hold, Decay, Sustain, Release. Knobs move the main points of the same curve. Shows the punch/tail split marker. Default shape: kick + bass | Yes | Envelopes (§4.4) |
| **Pitch envelope** | Editable display + 5 knobs: Amount (st), Attack, Decay, Curve, End | Yes | Envelopes (§4.4) |
| **Tock** | Mode switch (Synth / Sample), sample selector (factory list + load file), Level, Tune, Start, Decay, Tone | Yes | Tock (§4.3) |
| **Punch / Tail** | A Split control for the time where the punch ends and the tail starts, and a Punch Level and a Tail Level knob. There is no balance knob (removed 2026-10-13) | Yes | Punch/Tail (§4.5) |
| **Global** | Output gain, safety limiter on/off, Quality (oversampling 1x/2x/4x/8x), Glide on + time, Velocity sensitivity, Key track | Yes | Voice (§4.1), Output (§4.7) |
| **FX rack** | Reorderable list of effect slots (drag handle, enable, mix) with the selected effect's panel below | Yes | FX chain (§4.6) |
| **LFO 1 / LFO 2** | Display, 3 knobs, 2 toggles, LFO1/LFO2 tabs (from the sketch) | After MVP. The space is kept empty so adding it does not move other panels | Requires a modulation system |
| **Other** | Reserved space | To be defined | |

## Standalone audition strip

Only in the Standalone, below the grid: Auto-play toggle, BPM and an on-screen keyboard. It is not part of the plugin layout and does not exist in VST3/AU. See [standalone-audition](specs/standalone-audition.md).

## Component tree

```
PluginEditor                 (scales one fixed grid to the window)
├── PresetBar
├── OscillatorPanel ×2       (same class, bound to osc1 / osc2)
│   ├── header: On, Wavetable menu, Load
│   ├── strip: Pitch, Octave, Fine
│   ├── WavetableDisplay
│   └── knobs: Position, Level, Phase
├── EnvelopePanel ×2         (amp and pitch, same class)
├── FxArea
│   └── FxRackPanel          (one strip per registered effect; EqDisplay above the EQ's bands)
├── PlaceholderPanel         (LFO, post-MVP)
├── TockPanel                (a ControlPanel with a Load button)
├── ControlPanel             (Punch / Tail)
├── ControlPanel             (Global)
├── PlaceholderPanel         (Other)
└── AuditionPanel            (Standalone only)
```

## Revision history

| Version | Date | Change |
|---|---|---|
| v1 | 2026-10-11 | First layout from the product owner's sketch. |
| v2 | 2026-10-11 | Red areas decided: two identical oscillators, punch/tail balance with split slider, global panel contents, LFOs after MVP, tock with synth and sample modes. |
| v3 | 2026-10-13 | Implemented as a fixed design grid scaled to the window; the Punch / Tail balance knob was removed; the FX rack became vertical strips; the EQ gained a response display. |
