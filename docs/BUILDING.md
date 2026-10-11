# Building Frenchcore Machine

## Requirements

| Platform | Tools |
|---|---|
| All | CMake 3.25 or newer, Git, Ninja (recommended) |
| Windows | Visual Studio 2022 with the "Desktop development with C++" workload |
| macOS | Xcode 15 or newer (Command Line Tools) |
| Linux | GCC 11+ or Clang 15+, plus JUCE's dependencies: `libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev` (web browser and curl support are disabled, so WebKit and curl are not needed) |

JUCE and Catch2 are downloaded automatically on the first configure. Set `CPM_SOURCE_CACHE` to a folder outside the repo to reuse the download between builds.

## Build

```bash
git clone https://github.com/nocturn444/frenchcore-machine.git
cd frenchcore-machine
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

On macOS, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for a universal binary. On Windows, run these commands from a "x64 Native Tools Command Prompt for VS 2022" so Ninja finds the MSVC compiler, or drop `-G Ninja` to generate a Visual Studio solution.

The first configure downloads JUCE and Catch2 with git and takes about a minute; a full Release build takes a few minutes.

## Validating the plugin

CI runs [pluginval](https://github.com/Tracktion/pluginval) on every build. To run it locally:

```bash
pluginval --strictness-level 10 --skip-gui-tests --validate "build/src/plugin/FrenchcoreMachine_artefacts/Release/VST3/Frenchcore Machine.vst3"
```

On a headless Linux machine, prefix the command with `xvfb-run -a`.

## Where the plugin ends up

Build artefacts are in `build/src/plugin/FrenchcoreMachine_artefacts/Release/` (`VST3/`, `AU/` on macOS, `Standalone/`). By default the build does not copy plugins into your system plugin folders; pass `-DFCM_COPY_PLUGIN_AFTER_BUILD=ON` to do so.

## Downloading builds without compiling

Every pull request and every push to `main` produces downloadable builds in the GitHub Actions run (Actions tab, open a "Build" run, section "Artifacts": one zip per platform). Tagged releases are on the GitHub Releases page.

macOS builds are not signed during the MVP. To open them, remove the quarantine flag: `xattr -dr com.apple.quarantine "Frenchcore Machine.vst3"`.
