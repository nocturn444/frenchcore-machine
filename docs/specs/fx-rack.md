# Spec: FX rack (user interface for the effects)

- Status: Implemented (Phase 5, fifth part; layout and look are first choices by the agent, on the product owner's instruction to decide and keep going, and wait for their eyes)
- Owner: nocturn4 (product), implemented by agent session
- Related: [fx-chain](fx-chain.md), [fx-distortion](fx-distortion.md), [fx-eq](fx-eq.md), [fx-stereo](fx-stereo.md), [UI_LAYOUT](../UI_LAYOUT.md), [ARCHITECTURE §7](../ARCHITECTURE.md)

## 1. What it does for the user

The effects get their own place in the plugin window: the **Effects** tab, next to **Envelopes**, in the lower half. (Both need a lot of room, so they share the same area instead of making the window taller.)

- The tab shows **one column per effect**, side by side, in the order the sound goes through them (left to right).
- Each column has a **handle bar** at the top with the effect's name; **drag it left or right** to move the effect in the chain. The other effects slide to make room. The sound changes order at once, without clicks.
- Under the handle: the effect's **On** switch, its **Mix**, and all its own controls. They are normal controls: they can be automated and are saved with the project.
- The order is saved with the project.
- The effects' controls no longer appear in the general controls area at the top (it keeps Global, the two oscillators and the Tock).

Adding a new effect to the plugin adds its column here automatically.

## 2. User interface

```
[ Envelopes ] [ Effects ]
+------------------+------------------+------------------+
| = Distortion =   | = EQ =           | = Stereo =       |   <- drag the bar to reorder
| On   [x]         | On   [ ]         | On   [ ]         |
| Mix  ---o---     | Mix  ---o---     | Mix  ---o---     |
| Type [Soft Clip] | Low Freq ...     | Crossover 1 ...  |
| Drive ...        | ...              | ...              |
+------------------+------------------+------------------+
```

- The window keeps its size; the lower area now has two tabs. The Envelopes tab is selected when the plugin opens.
- A column highlights while you drag it.

## 3. Parameters

None new. The columns show the parameters of the effects (`<id>.enabled`, `<id>.mix` and the effect's own).

## 4. DSP / implementation approach

- No DSP. `ui::FxRackPanel` builds one `ui::FxSlot` per registered effect (so a new effect needs no UI code). Each slot is a drag handle and a `GenericParameterPanel` limited to that effect's group, so every parameter kind (switch, choice, whole number, number) is already covered and the controls are attached to the parameters in the usual way ([ARCHITECTURE §7](../ARCHITECTURE.md)).
- The rack reads and changes the order only through `core::FxOrderState` (`move`), which publishes it to the audio thread ([fx-chain](fx-chain.md)); it repositions the slots when `FxOrderState` announces a change.
- **Dragging**: while the handle is dragged, the position under the mouse is computed from the horizontal position and the slot width (`core::FxOrderState::positionAt`, a pure function) and the effect is moved there whenever it differs from its current position.
- The editor puts `TabbedComponent` in the lower area with the amp and pitch envelope panels in one tab and the rack in the other. The general controls panel excludes the groups of all registered effects.

## 5. Tests

- `core::FxOrderState::positionAt(x, slotWidth, count)` (`fcm_core_tests`): the first slot for `x` inside it and for negative `x`; the last slot beyond the right edge; the correct slot at every boundary; a width of 0 or not a number, a count of 0 or 1 and an `x` that is not a number are all safe (position 0).
- Dragging logic combined with `FxOrderState::move`: dragging effect 0 across a three-slot row to the far right gives the order 1, 2, 0 and publishes it; dragging it back restores the registration order.
- UI behaviour is checked by running the Standalone under a virtual display: the Effects tab shows one column per registered effect with the On, Mix and own controls, and the general panel no longer lists the effects. (Screenshots are not committed.)
- Existing tests keep passing: the layout and the generic tests for every registered effect are unchanged.

Manual check (Standalone): open the Effects tab, switch Distortion on, drag the EQ before it and listen for the order changing without clicks while the loop runs.

## 6. Out of scope

- Custom panels per effect (a transfer-curve view for the distortion, an EQ response display); this spec uses the generic controls.
- Dragging with a ghost image that follows the mouse across the window; the slots jump to their new place.
- Keyboard reordering, collapsing a column, resizing columns.

## 7. Decisions taken

1. A tab for the effects (shared with the envelopes) instead of a taller window.
2. One column per effect, left to right in the order of the sound.
3. The slots reuse the generic parameter controls; custom effect panels can come later without changing the rack.
