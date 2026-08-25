# ausearch — Work Plan

## TL;DR (For humans)

**What you'll get:** A fully working `modbox ausearch` command that reads Linux audit log files, parses key=value records, assembles multi-record events, applies filters (type, user, filename, key, syscall, time range, etc.), and outputs in default/raw/interpret mode. Plus tests and a man page.

**Why this approach:** Follows the established `audit2allow` pattern (same parent domain: audit log tools). No new dependencies — reads log files directly with `std::ifstream`. No libaudit needed. Single new `.cpp` + `.hpp` + test file + man page.

**What it will NOT do:** No libaudit netlink integration, no checkpoint/incremental search, no CSV/text output formats, no UUID/VM filters. Those are explicitly out of scope.

**Effort:** ~4–6 hours for an experienced C++20 developer familiar with the codebase. 4 sequential tickets.

**Risk:** Low. Self-contained command, no shared infrastructure changes. Synthetic test data means CI is not blocked on audit subsystem availability.

**Decisions:** libaudit is skipped (pattern from audit2allow); Phase 1 covers ~30 options with clear unsupported errors for the rest; 3 output formats (default/interpret/raw).

## Scope

- **In scope:** `ausearch` command implementation, test suite, man page. 4 new files, 0 modified existing files (Makefile auto-discovers `.cpp`).
- **Out of scope:** libaudit integration, checkpoint support, CSV/text output, escape modes, UUID/VM filters, `-a`/`-b`/`-l` flags (clear error messages instead).

## Verification strategy

- `bash tests/run_tests.sh` exits 0 with all 20+ test cases passing
- `modbox ausearch --help` shows usage with all documented options
- `modbox --help` lists `ausearch` among available commands
- `make man` generates `build/man/modbox-ausearch` without errors
- `make lint` passes with no clang-tidy warnings on new code

## Execution strategy

Linear dependency chain: skeleton → parser+filters → output+interpret → tests+manpage. Each ticket is a complete vertical slice that builds and tests independently. Run `bash tests/run_tests.sh` after each ticket to confirm zero regressions.

## Todos

### Wave 1: Skeleton & Input

- [ ] 1. Register ausearch command with argument parsing and input source resolution — `modbox ausearch --help` works, unknown options produce argtable3 errors, stdin/file input paths are wired up, `-a`/`-b`/`-l` produce "libaudit not available" errors, `--input FILE` reads from file, stdin pipe works when not a terminal, no-input-on-terminal produces stderr error and exits 1. Evidence: `./target/modbox ausearch --help | grep -q 'ausearch'`, `./target/modbox ausearch --foo 2>&1 | grep -q 'unrecognized option'`, `echo '' | ./target/modbox ausearch -m SYSCALL` exits 0 with no output. Commit: `feat(ausearch): add command skeleton with arg parsing and input sources`

- [ ] 2. Implement audit log record parser — parses `type=X msg=audit(EPOCH.MSEC:SERIAL): key=val ...` lines into `AuditRecord` structs with epoch/serial/fields, extracts `msg=audit()` stamp correctly, handles lines with or without the `msg=` field, returns empty fields map for malformed lines. Evidence: synthetic log line `type=SYSCALL msg=audit(1717056137.482:90412): arch=c000003e syscall=2 success=yes exit=0 uid=0 comm="cat"` parses to type=SYSCALL, epoch=1717056137, serial=90412, fields contains arch/syscall/success/exit/uid/comm. Commit: `feat(ausearch): implement audit log record parser`

### Wave 2: Event Assembly & Filters

- [ ] 3. Implement multi-record event assembly — groups records with the same `msg=audit(EPOCH:SERIAL)` stamp into `AuditEvent` objects, completes events on PROCTITLE/AUDIT_EOE records or after 2-second time gap, outputs assembled events in order. Evidence: three lines with same `msg=audit(1717056137.482:90412)` but types SYSCALL/PATH/PROCTITLE assemble into one event with 3 records; lines with different stamps produce separate events. Commit: `feat(ausearch): implement multi-record event assembly`

- [ ] 4. Implement message type filter (`-m`) — filters events by record type (SYSCALL, AVC, PATH, etc.), multiple `-m` flags combine with OR, `-m ALL` returns everything, unknown types produce clear error, `-m` without arguments lists valid types. Evidence: `echo 'type=SYSCALL msg=audit(1717056137.482:90412): ...' | ./target/modbox ausearch -m SYSCALL` returns the event; same input with `-m AVC` returns nothing. Commit: `feat(ausearch): implement message type filter`

- [ ] 5. Implement user ID filters (`-ui`/`-ue`/`-ua`/`-ul`, `-gi`/`-ge`/`-ga`) — filters events by uid/euid/auid/gid/egid fields in SYSCALL records, supports numeric and name lookup via getpwuid/getgrgid, `-ua` matches any UID field. Evidence: synthetic event with `uid=1000` matches `-ui 1000` but not `-ui 0`; `-ua 1000` matches even if only `euid=1000`. Commit: `feat(ausearch): implement user/group ID filters`

- [ ] 6. Implement path/key/syscall/process filters (`-f`/`-k`/`-sc`/`-c`/`-x`/`-p`/`-pp`/`--arch`/`-e`/`-sv`) — filename filter searches PATH records, key filter searches `key=` field, syscall filter matches by name or number using ausyscall/local table, comm/exe/pid/ppid filter SYSCALL record fields, arch filters by hex architecture value, exit filters by success/no, success filters by yes/no. Evidence: event with PATH `name="/etc/passwd"` matches `-f /etc/passwd`; event with `syscall=59` matches `-sc execve`; event with `success=yes` matches `-sv yes`. Commit: `feat(ausearch): implement path/key/syscall/process filters`

- [ ] 7. Implement time filters (`-ts`/`-te`/`--start`/`--end`) — parses special keywords (today/yesterday/recent/this-hour/boot/now/this-week/week-ago/this-month/this-year), parses explicit date/time strings via strptime, compares event epoch against range, both start and end are inclusive. Evidence: event at epoch 1717056137 matches `-ts 1717056000 -te 1717057000`; same event does not match `-ts 1717060000`. Commit: `feat(ausearch): implement time range filters`

### Wave 3: Output & Interpretation

- [ ] 8. Implement default and raw output formats — default format prints `---- time->...` separator then formatted records with key=value pairs, raw format prints unformatted original log lines, `--raw` flag produces raw output, `--format raw` is alias for `--raw`. Evidence: piped synthetic log with no filters outputs timestamp separator + records in default mode; `--raw` flag outputs original lines unchanged. Commit: `feat(ausearch): implement default and raw output formats`

- [ ] 9. Implement interpret mode (`-i`) — resolves numeric UIDs to usernames via getpwuid, resolves syscall numbers to names via ausyscall/local table, resolves hex arguments to readable form, prints interpreted values alongside or instead of raw numbers. Evidence: event with `uid=0` shows `uid=root` in interpret mode; event with `syscall=2` shows `syscall=open` when interpret flag is set. Commit: `feat(ausearch): implement interpret mode`

- [ ] 10. Implement remaining output/control flags (`-r`/`--just-one`/`--word`/`-l`/`-su`/`-o`/`-se`/`-hn`/`-tm`/`-v`/`--version`) — `--just-one` stops after first match, `--word` enforces whole-word matching on string filters, `-l` enables line-buffered output, SELinux context filters search subject/object fields, host/terminal filters search relevant fields, version prints modbox version. Evidence: `--just-one` with 10 matching events outputs only first; `--word -f passwd` does not match `/etc/password`. Commit: `feat(ausearch): implement remaining output and control flags`

### Wave 4: Tests & Documentation

- [ ] 11. Write test suite `tests/test_ausearch.sh` — covers help/version (2 tests), error handling (3 tests), empty input (1 test), event assembly (2 tests), message type filter (3 tests), user ID filter (3 tests), path/key/syscall filters (5 tests), time filter (3 tests), interpret/raw modes (4 tests), stdin/file input (3 tests), unsupported flags (2 tests), success filter (2 tests). Total ~33 tests. Evidence: `bash tests/run_tests.sh` exits 0; all 33 assertions pass. Commit: `test(ausearch): add comprehensive test suite`

- [ ] 12. Add man page `docs/man/modbox-ausearch.1.md` and register in Makefile — follows ADR-014 template with NAME/SYNOPSIS/DESCRIPTION/OPTIONS/EXAMPLES/EXIT STATUS/SEE ALSO sections, documents all Phase 1 options, lists unsupported options in NOTES, `make man` generates successfully. Evidence: `make man` produces `build/man/modbox-ausearch`; `man modbox-ausearch` renders correctly. Commit: `docs(ausearch): add man page and register in Makefile`

## Final verification wave

- [ ] F1. Plan compliance audit — verify every todo in the plan exists and has been completed; check no scope creep occurred; confirm all acceptance criteria from each todo are met.
- [ ] F2. Code quality review — run `make lint` to confirm zero clang-tidy warnings on new code; verify no GLib usage, no raw pointers where RAII suffices, proper argtable3 RAII wrapper usage, correct C++20 idioms.
- [ ] F3. Real manual QA — run `bash tests/run_tests.sh` and confirm exit 0; manually verify `modbox ausearch --help` lists all Phase 1 options; manually test a few filter combinations with synthetic data to confirm correctness.
- [ ] F4. Scope fidelity — confirm no libaudit dependency was added to Makefile PKGS; confirm no unsupported options were accidentally implemented; confirm tests use only synthetic data (no `/var/log/audit/audit.log` requirement); confirm man page matches actual implemented options.

## Commit strategy

4 sequential commits, one per wave, following Conventional Commits:
1. `feat(ausearch): add command skeleton with arg parsing and input sources`
2. `feat(ausearch): implement audit log parser, event assembly, and all filters`
3. `feat(ausearch): implement output formats and interpret mode`
4. `test(ausearch): add test suite and man page`

Alternatively squash to single commit `feat(ausearch): implement ausearch command` if preferred by project convention.

## Success criteria

- `modbox ausearch` appears in `modbox --help` command list
- `modbox ausearch --help` displays usage with all Phase 1 options documented
- `bash tests/run_tests.sh` exits 0 with all tests passing
- `make man` generates man page without errors
- `make lint` passes with no warnings on new code
- No existing tests are regressed
- Binary size increase is minimal (~one new .o file)
