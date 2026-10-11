# Frenchcore Machine

[Español](README.es.md)

A wavetable synthesizer plugin built for one job: making frenchcore kicks quickly.

Frenchcore Machine sits between dragging in a sample and building a kick from scratch in a full synth. It gives you a wavetable oscillator, an envelope that already has the classic kick + bass shape, a click layer and a reorderable chain of the effects frenchcore kicks actually need. Think of it as a heavily simplified Serum or Vital, focused on kicks.

> **Status:** early development. Nothing is usable yet. See the [plan](docs/PLAN.md) for progress.

## Features (planned for v0.1.0)

- Wavetable oscillator with factory kick wavetables, plus loading your own (Serum/Vital-compatible WAV).
- Editable amplitude envelope (default kick + bass shape) and pitch envelope.
- Tock sampler: an optional transient layer with factory tocks or your own samples.
- Monophonic with optional glide; the MIDI note tunes the kick's tail.
- Effects you can drag into any order: distortion (soft clip, hard clip, overdrive, foldback, bitcrush), 3-band EQ, multiband stereo.
- Presets.
- VST3, AU and Standalone for Windows, macOS and Linux.

The full list is in [MVP scope](docs/MVP_SCOPE.md).

## Download

No releases yet. Once available they will be on the [Releases page](https://github.com/nocturn444/frenchcore-machine/releases). Development builds are attached to each run in the Actions tab.

## Building from source

See [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

All documentation is in [docs/](docs/README.md). Start with:

- [Development plan](docs/PLAN.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Glossary](docs/GLOSSARY.md) for the technical terms

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) first: this project is documentation-driven, so every feature starts with a written spec.

## License

Frenchcore Machine is free software under the [GNU Affero General Public License v3.0](LICENSE). Sounds you make with it are yours, with no restrictions.

Built with [JUCE](https://juce.com). VST is a registered trademark of Steinberg Media Technologies GmbH.
