# UI Layout

- Status: Waiting for the product owner's layout sketch

This document will describe the functional layout of the editor, based on the sketch from the product owner. Visual styling (colours, fonts, knob graphics) is a separate, later task and is not specified here.

## Principles

- Function first: every MVP feature is reachable from the main window without menus.
- Resizable with a fixed aspect ratio; all positions are relative to the window size.
- All styling goes through a custom `LookAndFeel`, so the visual redesign does not move components.
- Double-click any knob to reset it to default; Ctrl/Cmd-drag for fine adjustment.

## Areas (provisional, to be replaced by the sketch)

| Area | Contents |
|---|---|
| Header | Preset browser (name, prev/next, save, load), output gain, safety limiter toggle |
| Oscillator | Wavetable display, position, load wavetable, key track, glide, velocity |
| Click | On/off, type, level, decay, tone |
| Envelopes | Amp and pitch breakpoint editors, shape presets, pitch amount, time scale |
| FX rack | Reorderable list of effect slots (drag handle, enable, mix) and the selected effect's panel |

## Sketch

_To be added: the product owner's sketch and its translation into a component tree._
