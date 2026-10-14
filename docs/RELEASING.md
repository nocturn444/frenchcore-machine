# Releasing

How a version goes out. The agent prepares everything; **the owner decides when**, because a published release is public and cannot be taken back quietly.

## Before the tag

1. Everything for the version is merged into `main` and CI on `main` is green.
2. The manual checklist in [TESTING.md](TESTING.md) is done on the systems you have, and the result is written in `docs/STATUS.md`.
3. `CHANGELOG.md`: rename `Unreleased` to the version and date, and add a new empty `Unreleased` above it. `docs/PLAN.md` Phase 7 is ticked.
4. The `VERSION` in the top-level `CMakeLists.txt` is the version being released. The release workflow refuses a tag that does not match it.
5. Check the [JUCE security advisories](https://forum.juce.com/c/security-advisories) for a fix newer than the JUCE version in `cmake/Dependencies.cmake`. Dependabot only watches the GitHub actions; it cannot see JUCE or Catch2, which CPM fetches. If there is one, update JUCE first (pinned by the commit SHA of the release tag) and release after CI is green.
6. Read `.github/release-notes.md` (the text the draft starts with) and update its known limitations from [TESTING.md](TESTING.md).

## Starting the release

There are two ways; both end in the same **draft** release with three zips.

**A. By hand from GitHub (no terminal needed).** Open the Actions tab, choose **Release** on the left, press **Run workflow**, keep the branch `main`, type the version (for example `v0.1.1`) and run it. The workflow creates the draft against the current `main`; the tag itself is created when you publish the draft.

**B. With a tag from a terminal.**

```bash
git checkout main && git pull
git tag v0.1.1
git push origin v0.1.1
```

Either way `.github/workflows/release.yml` first checks that the version matches `CMakeLists.txt` and that the commit is on `main`, then builds Linux, macOS (arm64 and Intel in one) and Windows, runs the unit tests, and attaches to a **draft** release:

- `FrenchcoreMachine-Windows.zip`: VST3 and Standalone.
- `FrenchcoreMachine-macOS.zip`: VST3, AU and Standalone.
- `FrenchcoreMachine-Linux.zip`: VST3 and Standalone (binaries stripped).
- `SHA256SUMS.txt`: the checksum of each zip.

Every zip also has `LICENSE.txt` and a `README.txt` (from `packaging/README.txt`); `packaging/package.sh` builds it, and CI runs the same script on every pull request so a broken zip shows up before release day. The build jobs only have read access to the repository. A last small job, the only one that can write, downloads the three zips, writes `SHA256SUMS.txt`, creates a build provenance attestation for each zip (signed by GitHub, stored with the repository) and attaches everything to the draft.

Anyone can check a download: compare it with `SHA256SUMS.txt` (`sha256sum -c SHA256SUMS.txt` on Linux, `shasum -a 256 -c SHA256SUMS.txt` on macOS), or with the GitHub CLI run `gh attestation verify FrenchcoreMachine-Windows.zip --repo nocturn444/frenchcore-machine`, which proves the zip was built by this repository's release workflow and from which commit.

Publishing a draft that was started by hand creates the tag, which starts the workflow once more; it notices the release already exists and builds nothing. Run it once per version: starting it again before the first draft is published would make a second draft.

## Publishing

Open the draft on the Releases page, check the four files are there, read the notes (they start from `.github/release-notes.md`; add what is new from the changelog if you like) and press **Publish release**. Only then is it public. To cancel, delete the draft (and the tag, if you made one with B).

## What is not done yet

- The plugins are **not signed or notarized**. macOS and Windows show a warning the first time; the user guide explains the one-time steps. Signing needs paid developer accounts and is listed after the MVP in the plan.
- There is no installer: the user copies the files (user guide, Installation).
