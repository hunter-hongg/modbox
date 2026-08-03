# 06 — audit2allow: comprehensive test suite

**What to build:** A complete `tests/test_audit2allow.sh` test suite covering all user-facing behavior: argument validation, AVC parsing, traditional rule output, module output, dontaudit output, type filtering, why mode, deduplication, edge cases, and error conditions. Tests use only hardcoded input strings (no real audit log or SELinux policy required) so they run in any environment.

**Blocked by:** 05 (all features must be implemented before comprehensive testing)

**Status:** ready-for-agent

- [ ] `--help` test: stdout contains `Usage:` and exits 0
- [ ] `--version` test: stdout matches `audit2allow \(modbox\) 1\.0` and exits 0
- [ ] Unknown option test: stderr matches `unrecognized option` and exits non-zero
- [ ] Conflicting flags test: `-a -i /dev/null` produces conflict error
- [ ] Empty input test: `echo "" | audit2allow` exits 0 with no output
- [ ] No-stdin test: `audit2allow` with no input produces error
- [ ] Basic AVC parsing test: single denial produces correct `allow` rule
- [ ] Short-form AVC test: `avc: denied { ... }` format is parsed correctly
- [ ] Multiple perms test: `{ read write }` produces `{ read write }` in output
- [ ] Deduplication test: two identical denials produce one rule
- [ ] Different types test: two different source types produce separate rule blocks
- [ ] Module output test: `-m testmod` produces correct module wrapper
- [ ] Require-only test: `-r` without `-m` produces require block + allow rules
- [ ] Module + require test: `-m testmod -r` produces correct combined output
- [ ] Dontaudit test: `-D` produces `dontaudit` not `allow`
- [ ] Dontaudit + module test: `-D -m testmod` produces module-wrapped dontaudit
- [ ] Type filter test: `-t AVC` processes AVC lines; `-t SYSCALL` produces no output
- [ ] Output to file test: `-o FILE` writes rules to file correctly
- [ ] Non-AVC line skip test: mixed input only produces rules for AVC lines
- [ ] All existing modbox tests still pass (`bash tests/run_tests.sh` exits 0)
