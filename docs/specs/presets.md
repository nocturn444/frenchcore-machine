# Spec: Presets (file format, factory presets, your own presets)

- Status: Implemented (Phase 6, second part). Which factory presets ship, and how they sound, is for nocturn4 to judge by ear.
- Owner: nocturn4 (product), implemented by agent session
- Related: [project-state](project-state.md), [state-safe-loading](state-safe-loading.md), [ARCHITECTURE §5, §12](../ARCHITECTURE.md), [PLAN Phase 6](../PLAN.md)

## 1. What it does for the user

A preset is a complete sound: everything that [project-state](project-state.md) saves (knobs, both envelope shapes, effect order, and your own wavetable and Tock sample embedded in it).

- **Factory presets** ship inside the plugin: a handful of starting points built from the plugin's own wavetables, envelope shapes and effects. They are listed first in the preset menu.
- **Your presets** are saved with a name of your choice as files in your Documents folder, `Frenchcore Machine/Presets`, and appear in the menu after the factory ones. A file is `<name>.fcmpreset`, which you can copy between computers or send to someone.
- Loading any preset replaces the whole sound (a control the preset does not mention goes to its default). Loading a preset never changes the DAW's tempo, the host's other tracks or the window size.
- A file that is damaged, too big or not a preset is refused with a short message and changes nothing.

## 2. User interface

The user interface is in the [preset browser spec](preset-browser.md) (next part). This part is the model underneath: files, factory table, library.

## 3. Parameters

None. A preset is not a parameter; it does not move when the host automates.

## 4. Implementation approach

### 4.1 File format (`.fcmpreset`)

An XML file in UTF-8:

```xml
<FrenchcorePreset presetFormat="1" name="My Kick">
  <FrenchcoreMachineState stateVersion="1"> ... exactly the project state ... </FrenchcoreMachineState>
</FrenchcorePreset>
```

- `presetFormat` is the version of this wrapper (1). A file with a higher number is refused ("made by a newer version") because the wrapper might mean something we do not know. The inner state has its own `stateVersion`, handled by the state loader (newer versions load as far as they are understood).
- `name` is the display name; it is sanitised on load like on save.
- There is no author, tag or path field in version 1. File paths are never stored.

### 4.2 Safety (presets are untrusted input)

- Files larger than 16 MiB are refused before they are read in full. Reading is capped, and a file that is not valid XML, has the wrong root tag, has no state child, or has a state that the loader rejects, is refused with a message. Before the XML is parsed, a file that declares a DTD or entity, or nests deeper than 64 elements, is refused the same way (`core/UntrustedXml.h`, shared with the host state).
- The state inside goes through `ProjectState::restore`, which already clamps values and ignores unknown entries ([state-safe-loading](state-safe-loading.md)). Nothing else is read from the file.
- **Names become file names, so they are sanitised**: only letters, digits, space, `-`, `_`, `.` and `(`/`)` are kept (anything else is dropped, including path separators and control characters), runs of spaces are collapsed, leading and trailing spaces and dots are removed, and the result is cut to 64 characters. A name that is empty after that is refused. Windows reserved device names (`CON`, `NUL`, `COM1`...) get an underscore appended.
- Saving only ever writes `<sanitised name>.fcmpreset` directly inside the preset folder, and double checks that the final file's parent is that folder. Writing is atomic (temporary file, then replace) so a failed save never leaves half a preset.
- Listing reads only regular files with the `.fcmpreset` extension in that one folder (no sub-folders, no symbolic links), at most 1000 of them, sorted by name without regard to case. Listing reads only names, not contents.
- Saving over an existing name replaces it (the UI asks first).

### 4.3 Factory presets

`core::FactoryPresets` is a table in code (adding a preset is one entry): a name, a list of parameter overrides `{id, value}` in real units, and the name of the amp and pitch envelope shape to use (from `EnvelopePresets`). A factory preset is turned into a state tree by starting from nothing: only the overridden parameters and the two envelope nodes are written, and `ProjectState::restore` makes every other control its default. So a factory preset keeps working when new controls are added later, and it can never carry stale values.

The table is checked by tests: every id exists, every value is within the parameter's range, every envelope name exists, names are unique, and each preset renders finite, non-silent audio in the engine tests.

The first set (to be reviewed by ear by nocturn4):

| Name | Idea |
|---|---|
| Init | Everything at its defaults (a plain kick with the default shapes). |
| Classic Punch | Hard Clip table, Short Punch shapes, Distortion on (Soft Clip, moderate drive). |
| Rumble Kick | Rumble Core table, Long Tail amp shape, Distortion Overdrive, EQ low boost. |
| Hardcore Crunch | Hardcore Punch table, Distortion Foldback, Tock on (Click). |
| Gabber Fold | Wave Fold table, Classic Drop pitch, Distortion Hard Clip, Stereo with Mono Below. |

### 4.4 The library

`core::PresetLibrary` works on a folder given to it (the plugin gives it `Documents/Frenchcore Machine/Presets`; tests give it a temporary folder): `list()`, `save(name, state)`, `load(file)`. It has no UI and no knowledge of the processor beyond `ProjectState`.

- `core::PresetManager` presents one numbered list: factory presets first, then your files. It knows the current preset name and index, offers `next()` / `previous()` (wrapping), `load(index)`, `save(name)`, and a "changed" broadcaster for the UI.
- A preset is applied on the message thread through `ProjectState::restore`, exactly like a host state load, so the audio thread sees the same atomic swaps as for any state change.

## 5. Tests

Catch2 (`fcm_core_tests`):

- Name sanitising: path separators and `..` are dropped, control characters are dropped, reserved Windows names are altered, long names are cut, empty results are refused, Unicode is dropped to the allowed set.
- Encode/decode: a captured state round-trips through preset text; the name is kept; the inner state is equivalent after a second round trip.
- Untrusted files: empty file, random bytes, truncated XML, wrong root tag, no state child, a state with a wrong root, a newer `presetFormat`, a file over 16 MiB, deep nesting, a DTD or entity, NaN and huge numbers inside the state, and embedded audio with a false size (loads without user audio). Each is refused or clamped and never changes the previous sound when refused.
- Library: save creates exactly `<name>.fcmpreset` in the folder and nothing outside it (a name like `../../x` stays inside); overwrite replaces; listing ignores other extensions, sub-folders and files that are not regular; listing is sorted, bounded at 1000 and case-insensitive; the save is atomic (no temporary file left behind).
- Factory presets: all entries valid as listed in 4.3; each loads without being rejected; loading twice gives the same state; after loading one, a control it does not mention is at its default even if it was changed before.
- Preset manager: next/previous wrap over factory then user presets; loading a user preset restores the sound and the current name; a refused load leaves the previous sound and name.

- The current preset name is kept by the manager for the session only. It is not part of the project state (no state format change); a project reopened in the DAW shows the sound it saved and the name of the preset last loaded in this plugin instance (Init for a new instance).

## 6. Out of scope

- Preset tags, author, search, rating, online sharing (MVP scope).
- Importing presets from other synths.
- Folders inside the preset folder.

## 7. Decisions taken

1. XML wrapper with a `presetFormat` version around the project state, extension `.fcmpreset`.
2. User presets live only in `Documents/Frenchcore Machine/Presets`; file names come from a strict whitelist of characters.
3. Factory presets are data in code that is applied over the defaults, so they follow new controls automatically.
4. Which factory presets ship is a placeholder set; the owner decides the final list by ear.
