# Testing

## Automated (every pull request)

- **Unit tests** (Catch2) for `fcm_dsp` and `fcm_core`: `ctest --test-dir build --output-on-failure`.
- **pluginval** at strictness level 10 against the VST3 (and AU on macOS).
- **Formatting** check with clang-format.

## Manual checklist (before each release)

Run in at least Ableton Live, FL Studio and Reaper, on Windows and macOS where available.

- [ ] Plugin scans and loads without errors.
- [ ] Default patch produces a kick on MIDI input; pitch follows the note when key track is on.
- [ ] Glide on/off behaves as specified.
- [ ] Load a factory wavetable and a user wavetable; position sweeps without clicks.
- [ ] Edit both envelopes (drag points, add and delete extra points, bend segments, turn the knobs, load each shape); changes are heard immediately without clicks or dropouts, and the knobs and the display always agree.
- [ ] Punch/Tail split and levels: the dashed marker follows Split, and moving the controls while a kick plays does not click.
- [ ] Every distortion type at extreme settings; no NaN, no crash, safety limiter catches peaks.
- [ ] Reorder the FX chain while audio plays; no dropouts.
- [ ] Automate a parameter of each effect, reorder the chain, automation still controls the same effect.
- [ ] Save the project, close the DAW, reopen: sound is identical, including the user wavetable and FX order.
- [ ] Save, load and browse presets.
- [ ] Resize the editor to minimum and maximum sizes.
- [ ] Sample rates 44.1, 48, 96 kHz; buffer sizes 32 to 2048.
- [ ] CPU usage reasonable with one instance at max oversampling.
