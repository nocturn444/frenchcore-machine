# 0004: Documentation-driven development

- Status: Accepted
- Date: 2026-10-11

## Context

The product owner is a music producer, not a programmer, and much of the implementation will be done by AI agents in separate sessions. Shared, written understanding is the only reliable way to keep everyone (humans and agents) aligned over time.

## Decision

Documentation leads code:

- Every feature starts with a spec in `docs/specs/`, reviewed before implementation.
- Architecture changes update `docs/ARCHITECTURE.md` and add an ADR.
- Every PR that changes behaviour updates the user guide, `CHANGELOG.md` and `docs/PLAN.md` status.
- `CLAUDE.md` holds the rules every agent follows.

## Consequences

- Slower start per feature, much less rework and drift.
- The product owner can review behaviour in plain language before any code exists.
- Reviewers reject PRs whose docs are out of date with their code.
