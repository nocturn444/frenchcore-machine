# Spec: FX rack (user interface for the effects)

- Status: Implemented (Phase 5, fifth part), reworked on 2026-10-13 from columns in a tab into strips in the Effects box of the grid ([ui-grid-layout](ui-grid-layout.md)). Layout and look are first choices by the agent. Version 0.2 puts this rack in three tabs (Bus A, Bus B, Main) next to a Mixer tab and adds add, duplicate and remove ([mixer-and-buses](mixer-and-buses.md) section 2).
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

## 2b. Version 0.2: copies, add, duplicate, remove, scrolling (stage 1)

Implemented as described in [mixer-and-buses](mixer-and-buses.md) section 1 for the Main rack (the Bus A and Bus B tabs come with the mixer stage):

- Every strip's bar has, from the right: **x** (remove), **++** (duplicate; greyed when no copy of that effect is free or the rack is full), Mix and On. The name carries the copy number ("Distortion 2").
- **+ Add effect** at the end opens a menu with every effect type; an entry is greyed when its four copies are in use, the button when the rack has eight effects. An empty rack shows "Empty: the sound passes through".
- The strips sit in a vertical scroll area; the Effects box keeps its size.
- A control with `shownWhen` is hidden unless the controlling parameter has one of the listed values (the Distortion's Bits and Rate show only for Bitcrush).

## 3. Parameters

None new. The strips show the parameters of the effects (`<id>.enabled`, `<id>.mix` and the effect's own).

## 4. DSP / implementation approach

- No DSP. `ui::FxRackPanel` builds one strip per effect instance in its rack from the effect's parameter descriptions (so a new effect needs no UI code). Every parameter becomes a `ui::ParameterControl` (a knob for a number, a switch for an on/off, a menu for a choice), attached to the parameter in the usual way ([ARCHITECTURE §7](../ARCHITECTURE.md)).
- **Groups.** A parameter whose ID suffix has a dot ("b1.freq") belongs to the group before the dot; the group's title is the parameter name without its last word ("Band 1 Freq" gives "Band 1"). An effect with groups shows a column per group (switches and menus on a row, knobs below); one without shows a single row (menus, switches, then knobs).
- **Pictures.** `ui/EffectDecorations.cpp` is a table from an effect ID to an optional component and its height. An effect without an entry shows only its controls. Adding an entry does not touch the rack.
- The rack reads and changes its contents only through `core::FxRackState` (`add`, `duplicate`, `remove`, `move`), which publishes them to the audio thread ([fx-chain](fx-chain.md)); it builds and repositions the strips when `FxRackState` announces a change. A strip is kept while its effect stays in the rack, so a drag in progress is not interrupted.
- **Dragging**: while the handle is dragged, the new position is computed from the vertical position of the mouse and the heights of the OTHER strips (`core::FxRackState::insertionIndex`, a pure function): the dragged strip goes after as many of the others as have their middle above the mouse, and the effect is moved there whenever that differs from its current position. Because the answer never depends on where the dragged strip is now, strips of different heights cannot flip back and forth under a still mouse (the 0.2 trial found them "going crazy" with the first version, which looked up the strip under the mouse in the current order).

## 5. Tests

- `core::FxRackState::insertionIndex(otherSizes, offset)` (`fcm_core_tests`): index 0 above the first middle, the right index at every middle (a middle belongs to the slot after it), the end beyond the stack; sizes that are not finite positive numbers count as 0; an offset that is not a number or an empty list give 0. A replayed drag with strips of different heights settles at once for every mouse position and never changes again. `positionAt` keeps its tests for slots of equal size.
- Dragging logic combined with `FxRackState::move`: dragging effect 0 across a three-slot row to the far end gives the order 1, 2, 0 and publishes it; dragging it back restores the registration order.
- UI behaviour is checked by running the Standalone under a virtual display: one strip per registered effect with its On, Mix and own controls, the EQ picture and band columns, and every strip inside the box. (Screenshots are not committed.)
- Existing tests keep passing: the layout and the generic tests for every registered effect are unchanged.

Manual check (Standalone): switch Distortion on, drag the EQ above it and listen for the order changing without clicks while the loop runs.

## 6. Out of scope

- Custom panels beyond a picture per effect (a transfer-curve view for the distortion).
- Dragging with a ghost image that follows the mouse across the window; the strips jump to their new place.
- Keyboard reordering, collapsing a strip, resizing strips.
