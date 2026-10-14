# Spec: Licence notice in the window

- Status: Implemented (0.1.1)
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADR 0002](../decisions/0002-license-agplv3.md), [ui-grid-layout](ui-grid-layout.md), [UI_LAYOUT](../UI_LAYOUT.md), review of the 0.1.0 release

## 1. What it does for the user

The window says which version of Frenchcore Machine is running, that it is free software under the AGPL-3.0 with no warranty, and where its source code is. It answers "which version do I have?" and meets the AGPL's request that an interactive program shows an appropriate legal notice (section 0, "Appropriate Legal Notices").

## 2. User interface

The text sits in the free **Other** box of the bottom row, in the same faint style as the reserved boxes, centred, on three lines:

```
Frenchcore Machine 0.1.1
Free software, AGPL-3.0, no warranty
Source: github.com/nocturn444/frenchcore-machine
```

It does not move or resize anything in the agreed grid. When the Other box gets its own contents, the notice moves to another corner of the window (for example the preset bar); that change goes through this spec.

## 3. Parameters

None.

## 4. DSP / implementation approach

`core::legalNotice()` (`src/core/AboutText.h`) builds the text. The version comes from `FCM_VERSION`, a compile definition that `src/core/CMakeLists.txt` sets from the CMake project version, so the window, the plugin binaries and the release tag always agree. The Other box is a `PlaceholderPanel` whose note is this text; the note is drawn fitted to the box on up to four lines.

## 5. Tests

- `AboutTextTests.cpp`: the version is three numbers, and the notice contains "Frenchcore Machine <version>", "AGPL-3.0", "no warranty" and the repository address.
- Manual: the notice is readable in the Other box at the smallest and largest window size.

## 6. Out of scope

- An "About" window with credits and the full licence text (the zips carry `LICENSE.txt`).
- A clickable link: the plugin never opens the network or a browser.

## 7. Open questions

None.
