# Spec: Preset browser

- Status: Implemented (Phase 6, third part)
- Owner: nocturn4 (product), implemented by agent session
- Related: [presets](presets.md), [UI_LAYOUT](../UI_LAYOUT.md)

## 1. What it does for the user

A strip at the top of the plugin window shows the current preset and lets you:

- open a menu listing the factory presets, then your own presets, and pick one;
- step to the previous or next preset with two arrow buttons (wrapping around);
- type a name and press **Save** to store the current sound as one of your presets; if the name already exists you are asked before it is replaced;
- see a short message when something could not be loaded or saved (for example "That file is not a Frenchcore Machine preset").

After you change a knob the name stays; it does not say "modified" in this first version. `INIT` of the layout document is the first factory preset, "Init".

## 2. User interface

One row of height 32 px above the parameter panel: `[<] [preset menu ........] [>]  [name field] [Save]` and a message label on the right. It is plain JUCE components; the visual design pass comes later. Keyboard: the menu and buttons are focusable like any component.

## 3. Parameters

None.

## 4. Implementation approach

- `ui::PresetBar` talks to `core::PresetManager` only (entries, `load(index)`, `next`, `previous`, `save(name)`, `wouldReplace`, a change listener). It never touches `ProjectState` or DSP.
- The menu is rebuilt from the manager's list each time it is opened, so files added from outside show up.
- Saving uses the name field; an empty or unusable name shows a message and writes nothing.

## 5. Tests

UI components are not unit tested in this project; the behaviour is covered by the manager tests in [presets](presets.md) §5, and by building the editor in the plugin test of the Standalone/pluginval run.

## 6. Out of scope

- A "modified" marker, rename, delete or reveal-in-folder buttons.
- Preset search and tags.

## 7. Decisions taken

1. The preset bar is a fixed strip above the parameters, not a pop-up window.
2. Overwriting asks for confirmation; deleting presets is left to the file manager.
