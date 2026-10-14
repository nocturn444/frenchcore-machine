# Spec: Editor grid layout

- Status: Implemented (2026-10-13). Replaces the generic parameter list and the tabs of the first editor versions. The sizes are first choices by the agent.
- Owner: nocturn4 (product), implemented by agent session
- Related: [UI_LAYOUT](../UI_LAYOUT.md), [fx-rack](fx-rack.md), [preset-browser](preset-browser.md), [fx-eq](fx-eq.md)

## 1. What it does for the user

The window follows the layout the product owner drew ([UI_LAYOUT.md](../UI_LAYOUT.md)): preset bar on top; the two oscillators at the top left; the amp and pitch envelopes under them; the effects in one tall box on the right; and a bottom row with the LFO box (kept free), the Tock, Punch / Tail, Global and a free box. In the Standalone the keyboard strip is under all of it.

- **It is wider than tall**, so it fits a screen: 1500 by about 770 points, plus 110 for the Standalone strip.
- **It never rearranges.** The whole grid is drawn at one fixed size and scaled to the window, so making the window smaller or larger changes only its size. Text, knobs and displays keep their proportions and nothing is cut or squeezed. The window keeps the grid's shape (the sizes allowed go from 60 % to 160 %).
- Every control is a knob, a switch or a menu with its name, grouped in the box of its feature. Double-click a knob for its default.

## 2. User interface

Design grid, in points at scale 1 (the width is 1500; margins 8, gaps 6):

| Area | Size and place |
|---|---|
| Preset bar | full width, 32 high |
| Oscillator 1 and 2 | 453 x 280 each, side by side, under the preset bar |
| Amp and Pitch envelope | 453 x 250 each, under the oscillators |
| Effects | 566 x 536, right of the two rows above |
| LFO (reserved) | 300 x 180, bottom row |
| Tock | 440 x 180 |
| Punch / Tail | 250 x 180 |
| Global | 350 x 180 |
| Other (reserved) | the rest of the row |
| Audition strip (Standalone) | full width, 110 high, under everything |

- **Oscillator**: header with On, the Wavetable menu and Load...; a strip with Pitch, Octave and Fine; the wavetable display; knobs Position, Level, Phase.
- **Envelope**: title, Sync, "Load shape..." menu; the display; one row of knobs (the five shape knobs and Time Scale).
- **Tock**: On, Mode, Sample menus and a "Load sample..." button; knobs Level, Tune, Start, Decay, Tone.
- **Punch / Tail**: knobs Split, Punch Level, Tail Level ([voice-punch-tail](voice-punch-tail.md)).
- **Global**: knobs Output, Root Note, Glide Time, Velocity; switches Limiter, Key Track, Glide; the Quality menu.
- **Effects**: [fx-rack](fx-rack.md).
- Messages about loading a wavetable or a sample appear in the preset bar ([preset-browser](preset-browser.md)).

## 3. Parameters

None new. The panels show existing parameters.

## 4. DSP / implementation approach

- No DSP. `PluginEditor` holds one `content` component of the design size, with every panel as its child placed once in design coordinates, and applies `AffineTransform::scale` to `content` whenever the window changes size. The scale is the smaller of width / design width and height / design height and the grid is centred, so a host that does not honour the fixed aspect ratio still gets a whole, undistorted grid. Mouse hit-testing follows the transform (JUCE).
- Parts: `ui::SectionPanel` (a titled box), `ui::ControlPanel` (rows of controls and custom components in a box: Tock, Punch / Tail, Global), `ui::ParameterControl` (one parameter as a knob, switch or menu, attached in the usual way), `ui::OscillatorPanel`, `ui::EnvelopePanel`, `ui::FxRackPanel`, `ui::PresetBar`, `ui::PlaceholderPanel` (the two free boxes), `ui::AuditionPanel`.
- `ui::FcmLookAndFeel` and `ui/Style.h` hold the fonts and colours, so the visual pass changes them in one place.
- The previous `GenericParameterPanel` and `WavetablePanel` are removed.
- Real-time: the UI only talks to parameters through attachments and to the processor through the same calls as before; nothing here touches the audio thread.

## 5. Tests

UI components are not unit tested in this project. What can be tested without a display is tested: the pure functions the UI uses (`FxOrderState::positionIn`, the EQ axes in `core::EqResponse`). The layout is checked by running the Standalone under a virtual display and looking at screenshots (not committed): every control of the previous generic list is reachable, nothing overlaps or is cut at the design size, and the Standalone strip fits.

## 6. Out of scope

- The LFO and "Other" boxes (kept free; since 0.1.1 the Other box shows the licence notice, [legal-notice](legal-notice.md)).
- A visual redesign, themes, animation.
- Remembering the window size between sessions.

## 7. Decisions taken

1. One fixed design grid scaled to the window, instead of re-flowing panels: it cannot squeeze or garble at any size, and it is the simplest thing that follows the sketch.
2. The Global box has Output, Root Note, Glide Time and Velocity as knobs and Limiter, Key Track, Glide and Quality as a row of switches and a menu (the sketch left its contents to the agent).
3. The Punch / Tail box has the Split and the two levels; the Balance knob was removed ([voice-punch-tail](voice-punch-tail.md)).
4. The Envelope's Time Scale is a knob in the same row as the shape knobs, to leave more height to the display.
