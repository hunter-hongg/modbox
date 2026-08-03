# 01 — audit2allow: CLI scaffold, arg parsing, and error handling

**What to build:** A working `audit2allow` command skeleton that accepts all flags (including unsupported ones), handles `--help` and `--version` correctly, validates mutually exclusive options, and produces the right error messages for invalid input. The command currently reads from stdin but produces no output yet — this ticket establishes the foundation so every subsequent ticket can be tested incrementally.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] Command compiles and registers via `REGISTER_COMMAND("audit2allow", ...)`
- [ ] `audit2allow --help` prints usage matching the reference implementation's flag list and exits 0
- [ ] `audit2allow --version` prints `audit2allow (modbox) 1.0` and exits 0
- [ ] Unknown option (e.g. `--foo`) prints `unrecognized option` to stderr and exits non-zero
- [ ] Conflicting input flags are rejected:
  - `-a -i /dev/null` → error about conflict
  - `-b -i /dev/null` → error about conflict
  - `-d -i /dev/null` → error about conflict
  - `-a -d` → error about conflict
- [ ] Unsupported flags produce errors (not crashes):
  - `-a` (requires libaudit, not available) → error message
  - `-b` → error message
  - `-l` → error message
  - `-M test` → error message
  - `-C` → error message
- [ ] Warning flags are silently ignored (no output, exit 0):
  - `-x` (xperms)
  - `--perm-map=/dev/null`
  - `--interface-info=/dev/null`
  - `-R` (reference policy) — falls back to traditional output
  - `-N` (no-op, default)
- [ ] No input specified and stdin is a terminal → error to stderr, exit non-zero
- [ ] Empty stdin → exits 0 with no output (graceful no-op)
- [ ] `-o /tmp/out.te` writes to file; file is created/appended correctly
- [ ] All tests in `tests/test_audit2allow.sh` pass
