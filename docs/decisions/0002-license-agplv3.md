# 0002: License: AGPL-3.0-only

- Status: Accepted
- Date: 2026-10-11

## Context

The plugin is meant to be open source. JUCE 9 is dual-licensed: AGPLv3 or a paid commercial license. Using JUCE for free requires the combined work to be distributed under AGPLv3.

## Decision

License Frenchcore Machine under AGPL-3.0-only. Every source file carries an SPDX header: `// SPDX-License-Identifier: AGPL-3.0-only`.

## Consequences

- Anyone may use, study, modify and redistribute the plugin, including commercially, as long as they publish the source of their modified version under the same license.
- Music made with the plugin is not affected by the license in any way.
- Closed-source forks are not possible without a different JUCE license and the copyright holders' agreement.
- Third-party code and assets added to the repo must have AGPLv3-compatible licenses. Factory wavetables and presets must be original or explicitly licensed for redistribution.
