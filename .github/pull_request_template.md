## What changes

Describe the change from a user's or developer's point of view.

## Related

Issue, spec (`docs/specs/...`) and ADR, if any.

## Checklist

- [ ] A spec exists in `docs/specs/` for new features, and it matches what was built
- [ ] `docs/ARCHITECTURE.md` and an ADR are updated if module boundaries, threading or state format changed
- [ ] Tests added or updated, and they pass locally
- [ ] No allocation, locks or I/O on the audio thread
- [ ] No released parameter ID was renamed or removed (or a migration was added)
- [ ] `CHANGELOG.md` updated under `Unreleased`
- [ ] User guide and `docs/PLAN.md` status updated
- [ ] Code formatted with clang-format
