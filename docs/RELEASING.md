# Releasing

How a version goes out. The agent prepares everything; **the owner decides when**, because a published release is public and cannot be taken back quietly.

## Before the tag

1. Everything for the version is merged into `main` and CI on `main` is green.
2. The manual checklist in [TESTING.md](TESTING.md) is done on the systems you have, and the result is written in `docs/STATUS.md`.
3. `CHANGELOG.md`: rename `Unreleased` to the version and date, and add a new empty `Unreleased` above it. `docs/PLAN.md` Phase 7 is ticked.
4. The `VERSION` in the top-level `CMakeLists.txt` is the version being released. The release workflow refuses a tag that does not match it.

## Starting the release

There are two ways; both end in the same **draft** release with three zips.

**A. By hand from GitHub (no terminal needed).** Open the Actions tab, choose **Release** on the left, press **Run workflow**, keep the branch `main`, type the version (`v0.1.0`) and run it. The workflow creates the draft against the current `main`; the tag itself is created when you publish the draft.

**B. With a tag from a terminal.**

```bash
git checkout main && git pull
git tag v0.1.0
git push origin v0.1.0
```

Either way `.github/workflows/release.yml` builds Linux, macOS (arm64 and Intel in one) and Windows, runs the unit tests, and attaches three zips to a **draft** release:

- `FrenchcoreMachine-Windows.zip`: VST3 and Standalone.
- `FrenchcoreMachine-macOS.zip`: VST3, AU and Standalone.
- `FrenchcoreMachine-Linux.zip`: VST3 and Standalone.

Publishing a draft that was started by hand creates the tag, which starts the workflow once more; it notices the release already exists and builds nothing. Run it once per version: starting it again before the first draft is published would make a second draft.

## Publishing

Open the draft on the Releases page, check the three files are there, write the notes (start from the changelog) and press **Publish release**. Only then is it public. To cancel, delete the draft (and the tag, if you made one with B).

## What is not done yet

- The plugins are **not signed or notarized**. macOS and Windows show a warning the first time; the user guide explains the one-time steps. Signing needs paid developer accounts and is listed after the MVP in the plan.
- There is no installer: the user copies the files (user guide, Installation).
