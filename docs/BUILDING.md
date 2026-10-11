# Building Frenchcore Machine

> The build system is created in Phase 1 of the [plan](PLAN.md). This guide describes the intended setup and will be verified against a clean checkout when Phase 1 lands.

## Requirements

| Platform | Tools |
|---|---|
| All | CMake 3.25 or newer, Git, Ninja (recommended) |
| Windows | Visual Studio 2022 with the "Desktop development with C++" workload |
| macOS | Xcode 15 or newer (Command Line Tools) |
| Linux | GCC 11+ or Clang 15+, plus JUCE's dependencies: `libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev` |

JUCE and Catch2 are downloaded automatically on the first configure. Set `CPM_SOURCE_CACHE` to a folder outside the repo to reuse the download between builds.

## Build

```bash
git clone https://github.com/nocturn444/frenchcore-machine.git
cd frenchcore-machine
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

On macOS, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for a universal binary.

## Where the plugin ends up

Build artefacts are in `build/FrenchcoreMachine_artefacts/Release/`. By default the build does not copy plugins into your system plugin folders; pass `-DFCM_COPY_PLUGIN_AFTER_BUILD=ON` to do so.

## Downloading builds without compiling

Every pull request and every push to `main` produces downloadable builds in the GitHub Actions run (section "Artifacts"). Tagged releases are on the GitHub Releases page.

macOS builds are not signed during the MVP. To open them, remove the quarantine flag: `xattr -dr com.apple.quarantine "Frenchcore Machine.vst3"`.
