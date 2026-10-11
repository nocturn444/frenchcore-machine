# Spec: User wavetables (load a WAV, saved with the project)

- Status: Implemented (Phase 4, second part; the decisions in section 7 were taken by the agent on the product owner's instruction to decide and keep going)
- Owner: nocturn4 (product), implemented by agent session
- Related: [wavetable-factory](wavetable-factory.md), [oscillator-wavetable](oscillator-wavetable.md), [state-safe-loading](state-safe-loading.md), [ADR 0008](../decisions/0008-wavetable-selection-and-publishing.md), [ARCHITECTURE §4.2, §12](../ARCHITECTURE.md)

## 1. What it does for the user

Each oscillator can play a wavetable from a WAV file of your own, as well as the nine factory ones.

- A **Load Wavetable** button next to each oscillator opens a file chooser. Pick a WAV file; if it is a valid wavetable, that oscillator switches to the new **User** entry of its Wavetable menu and plays it.
- What counts as a wavetable: a WAV whose length is a whole number of 2048-sample frames (1 to 256 frames, the format Serum and Vital use), or a **single cycle** of 64 to 16384 samples, which is stretched to one 2048-sample frame. Mono is used as is; with two or more channels only the first channel is used. The sample rate does not matter (the file is a shape, not a sound with a pitch).
- The sound is made loudness-safe: each frame is centred (no DC offset) and the whole table is scaled so its loudest sample is 1, keeping the relative level between frames.
- A file that cannot be used is **refused with a short message** under the button (for example "Not a WAV file", "Too long: at most 256 frames of 2048 samples", "The file contains invalid samples", "The file is silent"); the oscillator keeps playing what it had.
- The wavetable is **saved inside the project**. Reopening the project, even on another computer without the original file, plays the same wavetable. Only the file name is kept, for display; the path is never stored.
- If you choose **User** in the menu with nothing loaded, the oscillator plays the Sine.
- Each oscillator keeps its own user wavetable.

## 2. User interface

In the generic panel of Phase 4: a **Load Wavetable...** button below each oscillator's controls, and a line of text under the buttons that shows the last message (the name of the loaded file, or the reason a file was refused). The final oscillator panel with the wavetable display comes in the next part of this phase.

## 3. Parameters

The Wavetable choice of [wavetable-factory](wavetable-factory.md) gets one more entry, appended at the end:

| Entry | Name |
|---|---|
| 9 | User |

Nothing else is a parameter: the wavetable data and its name are state that is not a parameter.

## 4. DSP / implementation approach

- **Import (`core::importWavetable`)** turns the bytes of a WAV into frames, and rejects with a message everything that is not usable. It works on bytes, so it is tested without files. Limits, applied in this order: the byte size is at most 32 MiB; JUCE's WAV reader must open it; at least 1 and at most 8 channels; the declared length is checked before anything is read and must be 64 to 524288 samples (256 frames); the length must be a multiple of 2048 or between 64 and 16384; samples must all be finite; the table must not be silent. A single cycle is resampled to 2048 samples with cubic interpolation, treating the cycle as periodic.
- **Loading runs off the audio thread and off the message thread** for the heavy part (`core::WavetableLoader`, a one-thread pool): read the file, import it, then post the result to the message thread, which stores the frames, builds the band-limited table, publishes it ([ADR 0008](../decisions/0008-wavetable-selection-and-publishing.md)) and switches the parameter to User. A file larger than the byte limit is never read. If the processor is destroyed meanwhile, the pending result is dropped.
- **Saving.** Each oscillator with a user wavetable writes a node `UserWavetable1` or `UserWavetable2` into the state with the property `name` (text) and `frames` (the samples as 32-bit little-endian floats, base64). Nothing else is stored; in particular no file path.
- **Restoring** (`WavetableSelector::restoreFrom`) treats the node as untrusted input ([ARCHITECTURE §12](../ARCHITECTURE.md)): the text must be at most 3,000,000 characters, decode as base64 to a whole number of 2048-sample frames (1 to 256), every sample finite and at most 4 in magnitude; the name is cut to 64 printable characters. Anything else gives "no user wavetable" and the oscillator plays the Sine if User is selected. State loading still never fails because of this node.
- **Selection.** `WavetableSelector` builds the table of the User entry from the stored frames; without frames it builds the Sine. All building happens on the message thread, never on the audio thread.

## 5. Tests

Catch2, in `fcm_core_tests` (they need `juce_audio_formats`):

- Import: a valid 16-bit and a 32-bit float WAV of 1, 2 and 256 frames are accepted with the right frame count and values; a single cycle of 64, 600 and 2047 samples becomes one frame, a sine stays a sine (peak 1, frequency of one cycle); a 2-channel file uses the first channel; the table is centred and its peak is 1; relative frame levels are kept.
- Import rejections, each with a message: not a WAV (random bytes, an empty buffer, a text file); a truncated WAV; a WAV whose header declares a huge length; more than 256 frames; a length that is not a multiple of 2048 and above 16384; a float WAV with a NaN or an infinity; a silent file; a buffer above the byte limit (without reading it).
- Selector: loading frames publishes a table that reads those frames and reports the User entry; choosing User with nothing loaded selects the Sine; clearing returns to the Sine; the saved node round-trips (same frames bit for bit, same name); restoring rejects a node with a bad base64 text, a wrong size, a size not a multiple of 2048, more than 256 frames, a NaN, an infinite sample, a sample above 4, and an oversized text; the name is cut and cleaned.
- Loader: loading a file written to disk finishes on the message thread with the frames; a missing file reports an error; destroying the loader with a job pending does not call back.
- Fuzz: 3000 randomly damaged copies of a valid WAV (flipped bytes, cuts) are refused or give a valid table, never a crash; the same tests ran once under the address and undefined-behaviour sanitizers, clean.
- Not covered by an automated test: the processor's own save and restore of the `UserWavetable` nodes and the file chooser (the processor is only built into the plugin). The selector round trip above covers the data; the rest is a manual check, and pluginval exercises state save and load.
- Released-ID guard and the list guard are updated for the new entry.

Manual check (Standalone): load a few WAVs, including a bad one, and reopen the project.

## 6. Out of scope

- Other file formats (AIFF, FLAC, MP3) and folders of wavetables.
- Drawing or editing wavetables; resynthesis or spectral import.
- The wavetable display and frame names (next part of Phase 4).
- Remembering the folder used last.
- Sharing one user wavetable between both oscillators (each loads its own file).

## 7. Decisions taken

1. WAV only, first channel only, 2048-sample frames up to 256, plus single cycles of 64 to 16384 samples: the formats of Serum and Vital and the cycle files people commonly have.
2. Only the display name is saved, never the file path, so a project cannot make the plugin read or write another location (ARCHITECTURE §12). This changes the line of ARCHITECTURE §4.2 that said the path is stored.
3. The data is stored as 32-bit floats in base64: exact, no extra code. A table of 256 frames makes the project about 2.8 MB larger; acceptable.
4. A refused file leaves the current wavetable untouched.
