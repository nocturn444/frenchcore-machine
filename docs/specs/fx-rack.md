# Spec: FX rack (user interface for the effects)

- Status: Implemented (Phase 5, fifth part), reworked on 2026-10-13 from columns in a tab into strips in the Effects box of the grid ([ui-grid-layout](ui-grid-layout.md)). Layout and look are first choices by the agent.
- Owner: nocturn4 (product), implemented by agent session
- Related: [fx-chain](fx-chain.md), [fx-distortion](fx-distortion.md), [fx-eq](fx-eq.md), [fx-stereo](fx-stereo.md), [UI_LAYOUT](../UI_LAYOUT.md), [ARCHITECTURE §7](../ARCHITECTURE.md)

## 1. What it does for the user

The effects have their own box on the right of the window, **Effects**, as in the agreed layout.

- The box shows **one strip per effect**, from top to bottom, in the order the sound goes through them.
- Each strip has a **handle bar** at the top with the effect's name, its **On** switch and its **Mix**; **drag the bar up or down** to move the effect in the chain. The other strips slide to make room. The sound changes order at once, without clicks.
- Under the bar: the effect's own controls, as knobs, menus and switches. They are normal controls: they can be automated and are saved with the project. An effect whose controls come in groups (the EQ's four bands) shows one column per group, each with a title.
- An effect may have a picture above its controls (the EQ's response curve, [fx-eq](fx-eq.md)).
- The order is saved with the project.

Adding a new effect to the plugin adds its strip here automatically.

## 2. User interface

```
+- Effects ---------------------------------------+
| = Distortion ...................  On   Mix ---o-|   <- drag the bar to reorder
|  Type [Soft Clip]  Drive  Pre-gain  Post-gain ... |
+--------------------------------------------------+
| = EQ ...........................  On   Mix ---o-|
|  [ response curve with four dots ]               |
|  Band 1     Band 2     Band 3     Band 4         |
|  On [Type]  On [Type]  On [Type]  On [Type]      |
|  Freq Gain Q  ...                                |
+--------------------------------------------------+
| = Stereo ......................  On   Mix ---o-|
+--------------------------------------------------+
```

- A strip highlights while you drag it.
- The strips keep their own height (a strip with a picture is taller); the box has room for all the registered effects.

## 3. Parameters

None new. The strips show the parameters of the effects (`<id>.enabled`, `<id>.mix` and the effect's own).

## 4. DSP / implementation approach

- No DSP. `ui::FxRackPanel` builds one slot per registered effect from the effect's parameter descriptions (so a new effect needs no UI code). Every parameter becomes a `ui::ParameterControl` (a knob for a number, a switch for an on/off, a menu for a choice), attached to the parameter in the usual way ([ARCHITECTURE §7](../ARCHITECTURE.md)).
- **Groups.** A parameter whose ID suffix has a dot ("b1.freq") belongs to the group before the dot; the group's title is the parameter name without its last word ("Band 1 Freq" gives "Band 1"). An effect with groups shows a column per group (switches and menus on a row, knobs below); one without shows a single row (menus, switches, then knobs).
- **Pictures.** `ui/EffectDecorations.cpp` is a table from an effect ID to an optional component and its height. An effect without an entry shows only its controls. Adding an entry does not touch the rack.
- The rack reads and changes the order only through `core::FxOrderState` (`move`), which publishes it to the audio thread ([fx-chain](fx-chain.md)); it repositions the strips when `FxOrderState` announces a change.
- **Dragging**: while the handle is dragged, the position under the mouse is computed from the vertical position and the heights of the strips in the current order (`core::FxOrderState::positionIn`, a pure function) and the effect is moved there whenever it differs from its current position.

## 5. Tests

- `core::FxOrderState::positionIn(sizes, offset)` (`fcm_core_tests`): the first slot for an offset inside it and for negative offsets; the last slot beyond the end; the correct slot at every boundary (a boundary belongs to the next slot); sizes that are not finite positive numbers count as 0; an offset that is not a number, an empty list and a single slot are all safe (position 0). `positionAt` keeps its tests for slots of equal size.
- Dragging logic combined with `FxOrderState::move`: dragging effect 0 across a three-slot row to the far end gives the order 1, 2, 0 and publishes it; dragging it back restores the registration order.
- UI behaviour is checked by running the Standalone under a virtual display: one strip per registered effect with its On, Mix and own controls, the EQ picture and band columns, and every strip inside the box. (Screenshots are not committed.)
- Existing tests keep passing: the layout and the generic tests for every registered effect are unchanged.

Manual check (Standalone): switch Distortion on, drag the EQ above it and listen for the order changing without clicks while the loop runs.

## 6. Out of scope

- Custom panels beyond a picture per effect (a transfer-curve view for the distortion).
- Dragging with a ghost image that follows the mouse across the window; the strips jump to their new place.
- Keyboard reordering, collapsing a strip, resizing strips.
