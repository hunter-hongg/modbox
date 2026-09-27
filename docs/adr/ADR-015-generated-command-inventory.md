# ADR-015: Generated Command Inventory

Date: 2026-09-27

Status: Accepted

## Context

`registered_cmds.txt` and `man_pages.txt` are git-ignored derived files, but
they are the **only** input to the registry-driven man-page coverage check in
`tests/test_man_pages.sh` (ADR-014). That check asserts every registered
command has both a `docs/man/modbox-<cmd>.1.md` source and a `Makefile`
`MAN_SOURCES` entry.

Nothing in the build regenerated those two files — they were hand-edited after
each new command. Two consequences had already materialised:

* A command added without refreshing them (`hexdump`) left the coverage
  assertion scoped to a 201-command set that no longer matched the binary, so
  the check passed while covering less than it claimed.
* `man_pages.txt` declared 201 entries while listing 202, and its header count
  had drifted from `docs/man/*.md`.

The failure mode is silent by construction: the registry-driven test is
designed to catch a command shipped *without* documentation, but it cannot
notice that its own input has gone stale.

## Decision

Treat the built binary as the single source of truth for the command set, and
make regeneration a build target rather than a manual step.

* `tests/regen_registry.py` derives the command set from `modbox help` — the
  commands are whatever `REGISTER_COMMAND` registered — and rewrites
  `registered_cmds.txt` and `man_pages.txt`.
* It also reports commands missing a man page or `MAN_SOURCES` entry, so
  `make regen` surfaces a documentation gap instead of writing an inventory
  that the next test run will fail on.
* `make regen` regenerates; `make check-registry` verifies without writing.
* `make test` now depends on `check-registry`, so a stale inventory fails the
  suite before it can pass on a narrowed assertion.

The binary is read rather than the sources parsed: modelling `REGISTER_COMMAND`'s
argument syntax and any conditional registration in a regex would re-derive
what the compiler already knows, and would drift from it.

## Changes

- `tests/regen_registry.py` (new), with `--check` for verification
- `Makefile`: `regen` and `check-registry` targets; `test` depends on
  `check-registry`
- `registered_cmds.txt` / `man_pages.txt` refreshed to 202 commands
- `README.md` and `specs/missing_commands_overview.md` counts 201 → 202

## Consequences

- Adding a command now *fails* the suite when its inventory entry is missing,
  instead of quietly reducing coverage.
- The files stay git-ignored: they are reproducible from the build, so
  committing them would only add a second thing to keep in sync.
- A developer adding a command must run `make regen` (or `make test`, which
  tells them so). The failure is loud and names the command.
