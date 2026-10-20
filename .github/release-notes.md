Frenchcore Machine {TAG}: a wavetable synth for designing frenchcore kicks. VST3 and Standalone for Windows and Linux; VST3, AU and Standalone for macOS (Intel and Apple Silicon in one).

What is new: https://github.com/nocturn444/frenchcore-machine/blob/{TAG}/CHANGELOG.md
Highlights of 0.3.0: five new effects: Peak (a boosted zone into a soft-to-hard clipper, with an XY pad), Formant (a vowel filter), Phaser, Flanger and Volume LFO (a drawn volume shape in the style of LFOTool, with ten ready-made shapes); the Phaser, Flanger and Volume LFO restart at every note so every kick sounds the same. Highlights of 0.2.0: a mixer with two buses (each with its own effects, acting on the whole kick, the punch or the tail), up to four copies of every effect, new Filter, Transient shaper, Compressor (with an OTT-style multiband mode) and Reverb effects, extreme EQ bands with Key Track, a noise oscillator, and a reworked Tock (own volume, trim with Detect, Pogo, samples up to 10 seconds).
How to install: https://github.com/nocturn444/frenchcore-machine/blob/{TAG}/docs/USER_GUIDE.md#installation

Known limitations
- The plugins are not signed: macOS and Windows warn the first time (the install guide explains the one-time steps).
- Tested by hand in FL Studio only. Not tested yet: other DAWs, the AU in Logic, parameter automation, sample rates other than the default, and the Quality setting.
- New in 0.3 and not yet tried by hand in a DAW: Peak, Formant, Phaser, Flanger and the Volume LFO with its shape editor (the automated tests cover them, and projects and presets from 0.1 and 0.2 are checked to sound the same). Their sound was set by the agent and is still to be tuned by ear. The plugin now reports a few more samples of latency to the DAW because of Peak.
- A project saved with 0.3 opens in 0.2 without the new effects and without the drawn shapes.
- New in 0.2 and not yet tried by hand in a DAW: the mixer, the buses, the new effects, the noise oscillator and the new Tock (the automated tests cover them, and projects and presets from 0.1 are checked to sound the same).
- A project saved with 0.2 does not open correctly in 0.1.
- The Linux build needs a recent distribution (glibc 2.38, for example Ubuntu 24.04 or newer).
- The factory wavetables, noises and presets are a first selection and may change, and the sound of the new effects was set by the agent and is still to be tuned by ear.

Checking a download
SHA256SUMS.txt lists the checksum of each zip. Each zip also has a build provenance attestation signed by GitHub; with the GitHub CLI: `gh attestation verify FrenchcoreMachine-Windows.zip --repo nocturn444/frenchcore-machine`.

Free software under the GNU AGPL v3.0 (LICENSE.txt is in every zip). The source code of this version is on this page.
