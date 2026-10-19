# Frenchcore Machine

[Español](README.es.md)

A wavetable synthesizer plugin built for one job: making frenchcore kicks quickly.

Frenchcore Machine sits between dragging in a sample and building a kick from scratch in a full synth. It gives you two wavetable oscillators, an envelope that already has the classic kick + bass shape, a click layer (the "tock") and a reorderable chain of the effects frenchcore kicks actually need. Think of it as a heavily simplified Serum or Vital, focused on kicks.

> **Status:** version 0.1.0 is the latest release (tried in FL Studio only). Version 0.2.0 is built and waiting for review and release: see the [plan](docs/PLAN.md) and the [changelog](CHANGELOG.md).

## Features

New in v0.2.0:

- A mixer (Osc 1, Osc 2, Noise, Tock) with Level, Mute, Solo and a destination for each sound: Main, Bus A or Bus B. Each bus has its own row of effects and can act on the whole kick, only the punch or only the tail.
- Up to four copies of every effect, eight effects per row; add, duplicate and remove from the window.
- New effects: Filter, Transient shaper, Compressor (with an OTT-style multiband mode) and Reverb. The EQ gets Extreme bands (up to +-36 dB and Q 50) and Key Track.
- A noise oscillator: eight generated noises or your own WAV.
- A reworked Tock: its own volume (Amp Env switch), a waveform with Trim handles and Detect, Pogo pitch bend and samples up to 10 seconds.
- Projects and presets from 0.1 open and sound the same (checked by tests); a project saved with 0.2 does not open correctly in 0.1.

From v0.1.0:

- Two wavetable oscillators with factory kick wavetables, plus loading your own (Serum/Vital-compatible WAV).
- Editable amplitude envelope (default kick + bass shape) and pitch envelope, with points and knobs.
- Tock: an optional click layer, synthesized or played from factory tocks or your own samples.
- Separate punch and tail levels with an adjustable split point.
- Monophonic with optional glide; the MIDI note tunes the kick's tail.
- Effects you can drag into any order: distortion (soft clip, hard clip, overdrive, foldback, bitcrush), 4-band parametric EQ, multiband stereo (in 0.1; 0.2 adds more).
- Presets: five factory ones, and save your own.
- VST3, AU and Standalone for Windows, macOS and Linux.

The full list is in [MVP scope](docs/MVP_SCOPE.md).

## Download

Version 0.1.0 is on the [Releases page](https://github.com/nocturn444/frenchcore-machine/releases/latest): a zip for Windows, macOS and Linux. Installation steps, including the one-time warnings of unsigned plugins, are in the [user guide](docs/USER_GUIDE.md#installation). Development builds are attached to each run in the Actions tab.

## Building from source

See [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

All documentation is in [docs/](docs/README.md). Start with:

- [Development plan](docs/PLAN.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Glossary](docs/GLOSSARY.md) for the technical terms

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) first: this project is documentation-driven, so every feature starts with a written spec.

## Security

To report a vulnerability, use GitHub's private reporting as described in [SECURITY.md](SECURITY.md). Please do not open a public issue for it.

## License

Frenchcore Machine is free software under the [GNU Affero General Public License v3.0](LICENSE). Sounds you make with it are yours, with no restrictions.

Built with [JUCE](https://juce.com). VST is a registered trademark of Steinberg Media Technologies GmbH.
