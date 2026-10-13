# Testing

## Automated (every pull request)

- **Unit tests** (Catch2) for `fcm_dsp` and `fcm_core`: `ctest --test-dir build --output-on-failure`.
- **pluginval** at strictness level 10 against the VST3 (and AU on macOS).
- **Formatting** check with clang-format.

## Manual checklist (before each release)

Run in at least Ableton Live, FL Studio and Reaper, on Windows and macOS where available.

**Result for version 0.1.0** (owner, 2026-10-13): tried in **FL Studio only**, and no other DAW is planned. It loads and works well, RAM use is reasonable, saving and reopening a project works, and presets save, load and stay compatible between versions. **Not tried:** parameter automation, other sample rates and buffer sizes, the Quality setting, CPU at the highest Quality, Ableton Live and Reaper. The items below are left unticked for that reason; the first release says so in its notes.

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
- [ ] Resize the editor to minimum and maximum sizes: everything scales together, nothing is cut or overlaps, and the window keeps its shape.
- [ ] EQ display: drag each dot, turn the wheel over one for Q, double-click one to switch the band; the curve and the knobs below follow, and the sound follows too.
- [ ] Sample rates 44.1, 48, 96 kHz; buffer sizes 32 to 2048.
- [x] CPU usage reasonable with one instance at max oversampling. Measured by the agent on 2026-10-13 (see below); the owner did not measure it in a DAW and considers 8x an unusual setting anyway.

### CPU measurement (engine only, no DAW)

A kick about every 0.3 s for 10 s of audio at 48 kHz in blocks of 512, rendered offline on a 2.8 GHz Intel Xeon, one thread, Release build. The numbers are the share of one core that real-time playback would need.

| Quality | Effects off | Distortion, EQ with gains, and Stereo all on |
|---|---|---|
| 1x | 0.3 % | 1.3 % |
| 2x (default) | 0.3 % | 1.6 % |
| 4x | 0.3 % | 2.1 % |
| 8x | 0.3 % | 3.4 % |

Even at 8x with every effect on, one instance needs about 3 % of one core. This is the engine alone: the DAW's own work and the editor are not included, and other CPUs will differ, so treat it as an order of magnitude.
