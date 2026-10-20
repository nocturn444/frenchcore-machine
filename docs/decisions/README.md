# Architecture Decision Records

Each file records one significant decision: the context, the decision and its consequences. ADRs are never rewritten after they are accepted; a changed decision gets a new ADR that marks the old one as superseded.

| # | Title | Status |
|---|---|---|
| [0001](0001-juce-cmake-stack.md) | JUCE 9 with CMake and CPM | Accepted |
| [0002](0002-license-agplv3.md) | License: AGPL-3.0-only | Accepted |
| [0003](0003-fx-chain-model.md) | FX chain: one reorderable slot per effect type | Superseded by 0010 |
| [0004](0004-documentation-driven-development.md) | Documentation-driven development | Accepted |
| [0005](0005-engine-parameter-handoff.md) | The engine receives parameters as a plain struct | Accepted |
| [0006](0006-envelope-model-and-publishing.md) | Envelope model, and publishing shapes to the audio thread | Accepted |
| [0007](0007-envelope-tempo-sync.md) | Envelope tempo sync stores times at a reference tempo | Accepted |
| [0008](0008-wavetable-selection-and-publishing.md) | Wavetables are chosen by a parameter and published to each oscillator | Accepted |
| [0009](0009-effect-parameters-and-chain-latency.md) | Effects describe their parameters as plain data, and the chain keeps a constant latency | Accepted |
| [0010](0010-effect-instances-buses-and-mixer.md) | Effect instances from a fixed pool, three racks (Bus A, Bus B, Main) and a source mixer | Accepted |
| [0011](0011-effect-interface-additions.md) | Effect interface additions for 0.2: note context, conditional controls, default mix, more parameters | Accepted |
| [0012](0012-effect-note-triggers-curves-and-decorations.md) | Effects get note triggers, a drawable curve and interactive decorations | Accepted |

New ADRs copy [`_TEMPLATE.md`](_TEMPLATE.md) and take the next number.
