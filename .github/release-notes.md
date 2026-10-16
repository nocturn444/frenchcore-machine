Frenchcore Machine {TAG}: a wavetable synth for designing frenchcore kicks. VST3 and Standalone for Windows and Linux; VST3, AU and Standalone for macOS (Intel and Apple Silicon in one).

What is new: https://github.com/nocturn444/frenchcore-machine/blob/{TAG}/CHANGELOG.md
How to install: https://github.com/nocturn444/frenchcore-machine/blob/{TAG}/docs/USER_GUIDE.md#installation

Known limitations
- The plugins are not signed: macOS and Windows warn the first time (the install guide explains the one-time steps).
- Tested by hand in FL Studio only. Not tested yet: other DAWs, the AU in Logic, parameter automation, sample rates other than the default, and the Quality setting.
- The Linux build needs a recent distribution (glibc 2.38, for example Ubuntu 24.04 or newer).
- The factory wavetables and presets are a first selection and may change.

Checking a download
SHA256SUMS.txt lists the checksum of each zip. Each zip also has a build provenance attestation signed by GitHub; with the GitHub CLI: `gh attestation verify FrenchcoreMachine-Windows.zip --repo nocturn444/frenchcore-machine`.

Free software under the GNU AGPL v3.0 (LICENSE.txt is in every zip). The source code of this version is on this page.
