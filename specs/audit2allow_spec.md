# ModBox Implementation Spec: audit2allow Command

> **Reference**: Real `audit2allow` from the `setools` package (Fedora 42). This spec covers the core functionality — parsing audit log records and generating SELinux policy allow rules — with a pragmatic scope appropriate for a BusyBox-style multi-call binary.

## Problem Statement

SELinux denies access to processes when their actions violate the loaded policy. When a legitimate application is denied, the administrator must generate allow rules to permit the access. The standard tool for this is `audit2allow`, which reads audit log records (AVC denials) and outputs SELinux policy rules. modbox currently has no audit-related commands, leaving a gap in the SELinux toolchain: administrators must invoke an external `audit2allow` rather than using the unified modbox binary.

## Solution

Implement `audit2allow` as a modbox command that reads audit log records (from a file or stdin) and generates SELinux policy allow rules. The core behavior parses AVC denial messages and emits `allow` rule statements. Support for the most commonly used flags (`-i`/`--input`, `-m`/`--module`, `-o`/`--output`, `-r`/`--requires`, `-D`/`--dontaudit`, `-R`/`--reference`, `--help`, `--version`) is included. Input formats supported: raw audit log text (the default), and `ausearch`-style output. The implementation uses `libselinux` (already a dependency) for policy context parsing and `libaudit` (already installed on the system, add to `PKGS` in the Makefile) for reading audit records when `-a` is used.

## User Stories

1. As a sysadmin, I want to run `audit2allow -i /var/log/audit/audit.log` so that I can generate policy rules from the system audit log.
2. As a sysadmin, I want to pipe audit output into audit2allow (`ausearch -m avc | audit2allow`) so that I can quickly generate rules for the latest denials.
3. As a sysadmin, I want to run `audit2allow -m mymodule` so that the output is wrapped in a loadable SELinux module format with `require` and `module` statements.
4. As a sysadmin, I want to run `audit2allow -o policy.te` so that the generated rules are appended to a file rather than printed to stdout.
5. As a sysadmin, I want to run `audit2allow -D` so that dontaudit rules are generated instead of (or in addition to) allow rules.
6. As a sysadmin, I want to run `audit2allow -r` so that `require` statements for unknown types are included in module output.
7. As a developer, I want `audit2allow --help` to display concise usage information and exit successfully.
8. As a developer, I want `audit2allow --version` to display version information consistent with other modbox commands.
9. As a sysadmin, I want `audit2allow` to handle empty input gracefully and produce no output.
10. As a sysadmin, I want `audit2allow` to skip non-AVC lines in the input without errors.
11. As a sysadmin, I want `audit2allow` to extract the source type, target type, class, and permissions from AVC denial messages.
12. As a script writer, I want `audit2allow` to exit with code 0 on success so that I can use it in automation pipelines.
13. As a sysadmin, I want `audit2allow -R` to generate reference policy style output using macros.
14. As a developer, I want `audit2allow` to support the `-t`/`--type` filter so that I can restrict processing to messages matching a specific type regex.
15. As a sysadmin, I want `audit2allow` to deduplicate identical rules so that the output is concise.
16. As a user, when I pass an unrecognized option, I want a clear error on stderr and a non-zero exit code.
17. As a sysadmin, I want the default behavior (no flags) to read from stdin so that piping works naturally.
18. As a developer, I want `-a`/`--all` to read from the system audit log so that I don't need to specify a file path.
19. As a sysadmin, I want `audit2allow -w` to translate AVC messages into human-readable explanations instead of generating policy.
20. As a script writer, I want `audit2allow` to handle AVC messages with and without the `avc:` prefix.

## Implementation Decisions

### Command Interface

- **Signature**: `int audit2allow_command(int argc, char** argv);`
- **Input sources** (mutually exclusive where noted):
  - No flags or `-i FILE`/`--input=FILE`: read from the specified file
  - `-a`/`--all`: read from system audit log (requires `libaudit`)
  - `-b`/`--boot`: read audit messages since last boot (requires `libaudit`)
  - `-d`/`--dmesg`: read from `dmesg` output
  - stdin (default when no input flag is given and no positional args)
- **Output options**:
  - `-o FILE`/`--output=FILE`: append to file (conflicts with `-M`)
  - `-m NAME`/`--module=NAME`: wrap output as a SELinux module with `require` + `module NAME { ... }` block
  - `-M NAME`/`--module-package=NAME`: generate a loadable module package (out of scope for v1 — reject with error)
  - stdout (default)
- **Rule type options**:
  - `-D`/`--dontaudit`: generate `dontaudit` rules instead of `allow` rules (default behavior of reference is `dontaudit`; modbox default will be `allow` for script-friendliness)
  - `-r`/`--requires`: include `require` blocks for types not in the running policy
  - `-R`/`--reference`: generate reference policy style output (out of scope for v1 — emit a warning and fall back to traditional style)
  - `-C`/`--cil`: generate CIL output (out of scope for v1 — reject with error)
- **Filtering**:
  - `-t REGEX`/`--type=REGEX`: only process messages with a matching type field
- **Explanations**:
  - `-w`/`--why`: translate AVC denials into human-readable descriptions instead of generating rules
  - `-e`/`--explain`: fully explain generated output (builds on `-w`)
- **Other flags**:
  - `--help`, `--version`
  - `--perm-map`, `--interface-info` (out of scope for v1 — ignore with warning)
  - `-x`/`--xperms` (out of scope for v1 — ignore with warning)
  - `-l`/`--lastreload` (requires `libaudit`, read messages since last reload)
  - `-N`/`--noreference` (default behavior; no action needed)

### AVC Record Parsing

AVC denial messages in audit logs have the following format:

```
type=AVC msg=audit(1680000000.000:123): avc:  denied  { permission } for  pid=1234 comm="binary" srcname="file" tclass=file byuser=user ruser=user host=host salabel=label label=label
```

The parser must extract:
- **scontext** (source context): `user:role:type:level`
- **tcontext** (target context): `user:role:type:level`
- **tclass** (target class): `file`, `dir`, `tcp_socket`, etc.
- **perms** (permissions set): `read`, `write`, `open`, etc.

If the full context strings are present, parse them using `libselinux`'s `context_new()` / `context_str()`. If only short-form types are present (as in some audit formats), extract the type field directly.

### Output Format

**Traditional allow rules** (default):
```
#============= binary ==============
allow src_type target_class { perm1 perm2 };
```

**Module output** (with `-m MODULE_NAME`):
```
module MODULE_NAME 1.0;

require {
    type unknown_type1;
    type unknown_type2;
    class file { read write };
}

#============= binary ==============
allow src_type target_class { perm1 perm2 };
```

**Require-only output** (with `-r` but no `-m`):
```
require {
    type some_type;
    class some_class { perm };
}

allow src_type target_class { perm };
```

**Dontaudit rules** (with `-D`):
```
dontaudit src_type target_class { perm };
```

**Why/explain output** (with `-w`):
```
# avc:  denied  { read } for  pid=1234 comm="binary" name="file" dev="sda1" ino=12345 scontext=user:role:type:level tcontext=user:role:type:level tclass=file byuser=user ruser=user host=host salabel=user:role:type:level label=user:role:type:level
    # comm=binary  name=file  dev=sda1  ino=12345
    # source user:role:type:level
    # target user:role:type:level
    # known false positives: 0
```

### Arguments and Options

All options use argtable3. The following table summarizes supported flags for v1:

| Short | Long | Description |
|-------|------|-------------|
| `-i` | `--input=FILE` | Read from file |
| `-a` | `--all` | Read from audit log (unsupported in v1 — error) |
| `-b` | `--boot` | Read since last boot (unsupported in v1 — error) |
| `-d` | `--dmesg` | Read from dmesg |
| `-m` | `--module=NAME` | Generate module output |
| `-M` | `--module-package=NAME` | Generate module package (reject in v1) |
| `-o` | `--output=FILE` | Append to file |
| `-D` | `--dontaudit` | Generate dontaudit rules |
| `-r` | `--requires` | Generate require statements |
| `-R` | `--reference` | Reference policy style (warn + fallback) |
| `-C` | `--cil` | CIL output (reject in v1) |
| `-N` | `--noreference` | No-op (default) |
| `-t` | `--type=REGEX` | Filter by message type |
| `-w` | `--why` | Human-readable explanation |
| `-e` | `--explain` | Full explanation |
| `-l` | `--lastreload` | Read since last reload |
| `-x` | `--xperms` | Extended permissions (warn + ignore) |
| — | `--perm-map=FILE` | Permission map file (warn + ignore) |
| — | `--interface-info=FILE` | Interface info file (warn + ignore) |
| `--help` | — | Show help |
| `--version` | — | Show version |

**Mutual exclusion constraints**:
- `-a` conflicts with `-i` and `-d`
- `-b` conflicts with `-i`
- `-d` conflicts with `-a` and `-i`
- `-M` conflicts with `-o` and `-m`
- `-m` and `-r` are compatible; `-r` implies module-style require block
- `-w` and `-e` are mutually exclusive with rule generation flags (`-m`, `-D`, `-r`)

### Data Structures

An internal `AvcDenial` struct to hold parsed denial data:
```cpp
struct AvcDenial {
    std::string scontext;
    std::string tcontext;
    std::string tclass;
    std::vector<std::string> perms;
    std::string comm;       // process name
    std::string source_type; // extracted from scontext
    std::string target_type; // extracted from tcontext
};
```

A `RuleKey` struct for deduplication:
```cpp
struct RuleKey {
    std::string source_type;
    std::string target_type;
    std::string tclass;
    std::string perms; // sorted, deduplicated
    bool operator<(const RuleKey&) const;
};
```

### Error Handling

| Condition | stderr Output | Exit Code |
|-----------|---------------|-----------|
| No input source specified (no file, no stdin pipe) | `audit2allow: no input specified` | 1 |
| Input file not found | `audit2allow: cannot open 'FILE': No such file or directory` | 1 |
| `-M` with `-o` or `-m` | `audit2allow: --module-package conflicts with --output/--module` | 1 |
| `-a` with `-i` or `-d` | `audit2allow: --all conflicts with --input/--dmesg` | 1 |
| Unrecognized option | `audit2allow: unrecognized option '--foo'` | 1 |
| `-M` or `-C` flag (not supported in v1) | `audit2allow: --module-package is not supported` | 1 |
| `-R` flag (not fully supported) | warning printed, fallback to traditional output | 0 |
| `-x`, `--perm-map`, `--interface-info` | warning printed, flag ignored | 0 |
| Empty input | no output, exit 0 | 0 |
| Malformed AVC line | skipped with no error (matching reference behavior) | 0 |
| libaudit not linked | `audit2allow: libaudit not available` | 1 |

### Library Dependencies

- **libselinux** (already in `PKGS`): for `context_new()`, `context_str()`, `freecon()`, and AVC message formatting.
- **libaudit** (runtime library present but **no dev headers/pkg-config**): `-a`/`--all`, `-b`/`--boot`, `-l`/`--lastreload` are **out of scope for v1** due to missing `audit-libs-devel`. These flags are accepted but emit a "not supported" error.
- **No new build dependencies** — the project does not gain any new `PKGS` entries.

### File Structure

- Header: `include/commands/audit2allow.hpp` — declares `int audit2allow_command(int argc, char** argv);`
- Source: `src/commands/audit2allow.cpp` — implements the command
- Registration: `REGISTER_COMMAND("audit2allow", audit2allow_command, "Generate SELinux policy rules from audit logs")`

### Build System

Add `libaudit` to the `PKGS` line in `Makefile`:
```makefile
PKGS := argtable3 ftxui openssl libselinux libacl libaudit
```
No other Makefile changes needed — source discovery is automatic.

### Output Deduplication

Rules are deduplicated using a `std::map<RuleKey, std::vector<std::string>>` where the key is `(source_type, target_type, tclass, sorted_perms)` and the value is the list of `comm` values that triggered the rule. When emitting output, all comm values for a given key are grouped under a single rule.

### Input Format Handling

The parser handles two input formats:
1. **Raw audit log lines**: lines containing `avc:  denied` (with varying whitespace). Parse using regex to extract fields.
2. **ausearch-style output**: lines beginning with `avc:` or containing `denied` with structured key=value pairs.

Lines that do not match AVC denial patterns are silently skipped.

## Testing Decisions

### Test Approach

Use the existing test framework at `tests/run_tests.sh`. Create `tests/test_audit2allow.sh`.

### Test Seams

1. **Argument validation seam** (no audit log or SELinux required): Tests for `--help`, `--version`, unknown options, conflicting flags, missing input. This is the highest seam and requires no privileges or audit infrastructure.
2. **Input parsing seam** (no SELinux required): Tests with hardcoded AVC denial text passed via stdin or temporary files. Verifies correct extraction of scontext, tcontext, tclass, and perms.
3. **Output format seam** (no SELinux required): Tests with known input produce expected output strings. Covers traditional rules, module output, require blocks, dontaudit rules, and why output.
4. **Deduplication seam** (no SELinux required): Tests that duplicate denials produce a single rule.
5. **Filtering seam** (no SELinux required): Tests `-t` regex filtering.
6. **Real audit log seam** (requires audit log or root): Conditional tests that read from actual audit logs. Skip if no audit infrastructure is available.

The primary testing seam is argument validation + input parsing with controlled input — no root or SELinux required.

### Test Cases to Implement

1. **`--help`**: `assert_cmd_pat 'Usage:' audit2allow --help` — exit 0.
2. **`--version`**: `assert_cmd_pat 'audit2allow \(modbox\) 1\.0' audit2allow --version` — exit 0.
3. **Unknown option rejected**: `assert_cmd_pat_stderr 'unrecognized option' audit2allow --foo` — exit non-zero.
4. **Conflicting flags**: `audit2allow -a -i /dev/null` prints conflict error to stderr and exits non-zero.
5. **Empty input**: `echo "" | audit2allow` exits 0 with no output.
6. ** AVC parsing — basic denial**: Pipe a single AVC denial line and verify the output contains `allow` with correct types and class.
7. ** AVC parsing — multiple denials**: Pipe multiple denial lines and verify all are parsed.
8. **Deduplication**: Pipe two identical denials and verify only one rule is emitted.
9. **Module output**: `echo 'AVC...' | audit2allow -m testmod` and verify output contains `module testmod` and `require` block.
10. **Require output**: `echo 'AVC...' | audit2allow -r` and verify `require` block is present.
11. **Dontaudit output**: `echo 'AVC...' | audit2allow -D` and verify output contains `dontaudit` not `allow`.
12. **Why output**: `echo 'AVC...' | audit2allow -w` and verify human-readable explanation is emitted.
13. **Type filter**: `echo 'AVC...' | audit2allow -t 'AVC'` and verify only matching lines are processed.
14. **Output to file**: `audit2allow -o /tmp/out.te -i /dev/stdin` with input piped in, then verify file contains expected rules.
15. **Non-AVC lines skipped**: Pipe a mix of AVC and non-AVC lines and verify only AVC lines produce output.
16. **Conditional: no input specified**: `audit2allow` with no stdin and no `-i` flag should print error to stderr and exit non-zero.

### Existing Test Patterns

Examine `tests/test_getenforce.sh` and `tests/test_chcon.sh` for assertion style. Use `assert_cmd_pat`, `assert_cmd_pat_stderr`, and `assert_cmd_not_pat` helpers from `tests/framework.sh`.

## Out of Scope

- **Module package generation** (`-M`): This requires compiling and packaging SELinux modules, which is complex and outside the scope of a multi-call binary. The flag is accepted but produces an error message.
- **CIL output** (`-C`): CIL (Common Intermediate Language) is a different policy format; out of scope for v1.
- **Reference policy generation** (`-R`): Full reference policy macro generation requires interface information files and is complex; v1 emits a warning and falls back to traditional output.
- **Permission map files** (`--perm-map`): These are advanced customization features; v1 ignores them with a warning.
- **Interface info** (`--interface-info`): Same as above.
- **Extended permissions** (`-x`): xperms are a niche feature; v1 ignores with a warning.
- **`-e`/`--explain`**: Builds on `-w`; out of scope for v1 (can be added later).
- **`-a`/`--all`**, **`-b`/`--boot`**, **`-l`/`--lastreload`**: Out of scope for v1 due to missing `libaudit` development headers. These flags are accepted but emit an error. Can be added later when `audit-libs-devel` is available.
- **Cross-platform support**: Targets Linux with libselinux and libaudit only.

## Further Notes

- The reference `audit2allow` is a Perl script in the `setools` package; this implementation rewrites the core logic in C++ for integration into modbox.
- `libaudit` is already installed on Fedora 42 (`audit-libs-4.1.4`) but was not previously a build dependency. Adding it to `PKGS` is a minimal change.
- AVC denial messages can appear in different formats depending on the audit subsystem version. The parser should be robust to whitespace variations and optional fields.
- The `comm` field in AVC messages is the truncated process name (16 chars max in audit logs). The output groups rules by `comm` as the reference does.
- On systems where SELinux is disabled, `audit2allow` should still parse AVC messages and generate rules — it does not need a running SELinux policy to function (unlike `getenforce`).
- The `--requires` (`-r`) flag without `--module` (`-m`) should emit only `require` blocks followed by the allow rules, matching the reference implementation.
- The reference implementation outputs a comment `#!!!! This avc is allowed in the current policy` when the denial is already allowed by the running policy (when `--policy` is available). modbox v1 should still emit this comment when the policy file is provided via `-p`.
- The default rule type is `allow` (not `dontaudit`), which differs from the reference implementation's default. This is a deliberate design choice for script-friendliness — users explicitly opt into `dontaudit` with `-D`.
