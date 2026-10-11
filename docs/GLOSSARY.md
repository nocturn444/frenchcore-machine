# Glossary

Plain-language explanations of the technical terms used in this project's documents.

| Term | Meaning |
|---|---|
| **ADR** | Architecture Decision Record. A short document recording an important technical decision and why it was made. |
| **AGPLv3** | The open-source license of this project. Anyone can use and change the code, but if they distribute a modified version they must publish its source code too. |
| **Aliasing** | Harsh, unwanted frequencies that appear when a digital sound contains frequencies too high for the sample rate. Avoided with band-limited wavetables and oversampling. |
| **APVTS** | JUCE's system for storing plugin parameters so the DAW can automate and save them. |
| **AU** | Audio Unit, Apple's plugin format (Logic Pro, GarageBand). macOS only. |
| **Audio thread** | The part of the program that computes audio in real time. If it waits for anything, you hear clicks or dropouts. |
| **Band-limited / mipmaps** | Pre-computed versions of a wavetable with high harmonics removed, one per pitch range, so high notes do not alias. |
| **Breakpoint envelope** | An envelope drawn as points connected by curves, like Serum's or Vital's. |
| **CI** | Continuous Integration. GitHub automatically builds and tests the plugin on Windows, macOS and Linux every time code changes. |
| **CMake / CPM** | The build tool that turns source code into the plugin, and the helper that downloads libraries automatically. |
| **DSP** | Digital Signal Processing: the math that generates and transforms sound. |
| **Glide** | Smooth pitch slide between consecutive notes (portamento). |
| **JUCE** | The C++ framework used to build the plugin for all formats and platforms. |
| **Key track** | When on, the MIDI note decides the pitch of the kick's tail. When off, the pitch is fixed. |
| **Linkwitz-Riley crossover** | A filter that splits audio into frequency bands that add back together cleanly. Used by the multiband stereo effect. |
| **MVP** | Minimum Viable Product: the first version that is genuinely useful. |
| **Oversampling** | Processing audio internally at a higher sample rate to reduce aliasing from distortion. |
| **pluginval** | A tool that stress-tests a plugin the way DAWs use it, to catch crashes before users do. |
| **PR (pull request)** | A proposed change to the code, reviewed before it is merged into the main branch. |
| **Release (envelope)** | The part of a sound that happens after you let go of the key. Here it is a short fade-out, so the length of the MIDI note decides how long the kick's tail rings. |
| **Spec** | A short document describing a feature before it is built. |
| **Standalone** | The plugin running as its own application, without a DAW. |
| **VST3** | Steinberg's plugin format, supported by almost every DAW on Windows, macOS and Linux. |
| **Wavetable** | A set of single-cycle waveforms (frames). Moving the position morphs between them. |
