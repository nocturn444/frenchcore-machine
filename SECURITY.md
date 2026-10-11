# Security policy

## Reporting a vulnerability

Please report security problems privately, not in a public issue.

Use GitHub's private vulnerability reporting: open the **Security** tab of this repository, choose **Report a vulnerability**, and describe what you found, how to reproduce it (a state, preset or wavetable file helps), and which version you used.

You can expect an answer within a few days. Once a fix is released, the report can be made public and you will be credited if you want.

## Supported versions

Only the latest release and the `main` branch receive fixes.

## What counts

Anything that lets a file or host data crash the plugin or its host, read or write memory it should not, write outside the presets folder, or otherwise break the rules in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) section 12 ("Untrusted input"). Problems in the build and release pipeline (CI, dependencies) are also in scope.
