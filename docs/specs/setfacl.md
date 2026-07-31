# Spec: `setfacl` Command

## Problem Statement

modbox currently has a `getfacl` command for reading file access control lists (ACLs), but no ability to modify or remove ACL entries. Users who manage file permissions with ACLs must resort to the system's `/usr/bin/setfacl` — breaking the modbox-all-in-one experience and the promise of having a complete coreutils replacement. Without `setfacl`, users cannot add user/group ACL entries, remove old entries, set default ACLs on directories, or reset ACLs back to plain mode bits — all operations that `getfacl`'s output implies should be possible.

## Solution

Implement a `setfacl` command that accepts ACL entry specifications (in the standard text format also produced by `getfacl`) and applies them to files and directories. The command follows the GNU `setfacl` interface, using the `libacl` library already linked into modbox. The implementation lives alongside `getfacl` in the same command family, reuses the same display-path and traversal patterns, and is tested at the CLI level — matching the existing `test_getfacl.sh` approach.

## User Stories

### Core ACL Modification

1. As a system administrator, I want to grant read access to a specific user on a file, so that user can read a shared file without changing group ownership.
2. As a system administrator, I want to add a named group with write permissions to a directory, so that group members can modify its contents.
3. As a system administrator, I want to remove a specific user's ACL entry from a file, so that user no longer has the access previously granted.
4. As a system administrator, I want to modify an existing ACL entry's permissions (e.g., upgrade from read to read-write), so that I can adjust access without removing and re-adding the entry.

### Default ACLs on Directories

5. As a system administrator, I want to set a default ACL on a directory so that new files and subdirectories inside it inherit those ACL entries automatically.
6. As a system administrator, I want to remove the default ACL from a directory, so that new children no longer inherit custom permissions.

### Bulk Operations

7. As a system administrator, I want to remove all extended ACL entries from a file, restoring it to plain POSIX mode bits, so that I can reset permissions to a clean state.
8. As a system administrator, I want to remove only the default ACL entries from a directory while preserving access ACL entries, so that existing access controls stay intact while inheritance stops.
9. As a system administrator, I want to apply ACL changes recursively to a directory tree, so that I don't have to operate on each file individually.

### File-Based ACL Specifications

10. As a system administrator, I want to read ACL modifications from a file (the format produced by `getfacl`), so that I can batch-apply a set of ACL changes I previously captured.
11. As a system administrator, I want to read ACL entries to remove from a file, so that I can clean up many stale entries across multiple files.
12. As a system administrator, I want to completely replace a file's ACL with the ACL entries specified in a file (using `--set-file`), so that I can restore a known ACL configuration from backup.
13. As a system administrator, I want to restore an entire ACL state from a `getfacl` backup file using `--restore`, so that I can recover ACLs after file migration or disaster recovery.

### Safety and Validation

14. As a cautious administrator, I want a dry-run or test mode (`--test`) that shows me what would change without actually modifying anything, so that I can verify my ACL specifications before applying them.
15. As a system administrator, I want setfacl to preserve the root directory with `--preserve-root`, so that I don't accidentally recursively modify `/` and damage the system.
16. As a system administrator, I want setfacl to reject invalid ACL specifications with a clear error, so that I know immediately when my syntax is wrong rather than silently getting an incorrect result.

### Symlink Handling and Traversal

17. As a system administrator, I want to control whether setfacl follows symlinks or operates on the symlink itself (using `-P`, `-L`, `-H`), so that ACL changes land on the right file in symlink-heavy filesystems.
18. As a system administrator, I want to skip symlinks encountered during recursive traversal, so that I don't accidentally apply ACL changes to files across mount points or network shares.

### Output and Feedback

19. As a system administrator, I want setfacl to exit with a non-zero status when any file operation fails, so that scripts can detect and handle errors.

## Implementation Decisions

### Module Architecture

- A new command module: `setfacl` (header `include/commands/setfacl.hpp`, source `src/commands/setfacl.cpp`)
- No new library dependencies — `libacl` is already linked via `pkg-config` in the Makefile
- Companion test file: `tests/test_setfacl.sh`

### ACL Text Parsing

- ACL entry specifications use the standard text format: `[d:]TYPE:QUALIFIER:PERMISSIONS`
- Parsing delegates to `libacl`'s `acl_from_text()` where possible for single entries
- For modify (`-m` / `-M`) and remove (`-x` / `-X`) operations: parse the entry text, then use `acl_get_file()` to get the existing ACL, `acl_create_entry()` / `acl_delete_entry()` to add or remove entries, and `acl_set_file()` to write back
- For `--set` / `--set-file`: use `acl_from_text()` to parse the full ACL specification, validate with `acl_valid()`, then `acl_set_file()`
- Mask recalculation: call `acl_calc_mask()` after modifications unless the user passes `-n` / `--no-mask`

### Options Struct

```cpp
struct SetfaclOptions {
    int is_recursive = 0;
    int is_test_mode = 0;         // --test
    int no_mask = 0;              // -n / --no-mask
    int recalc_mask = 1;          // --mask (default on)
    int is_physical = 0;          // -P
    int is_logical = 0;           // -L
    int is_dereference = 0;       // -H
    int preserve_root = 0;        // --preserve-root
    int one_file_system = 0;      // --one-file-system
    const char *modify = nullptr;       // -m entry list
    const char *modify_file = nullptr;  // -M file
    const char *remove_entries = nullptr;  // -x entry list
    const char *remove_file = nullptr;  // -X file
    int remove_all = 0;           // -b
    int remove_default = 0;       // -k
    const char *set_acl = nullptr;      // --set
    const char *set_file = nullptr;     // --set-file
    const char *restore_file = nullptr; // --restore
};
```

### Operation Order

When multiple operations are specified, they apply in this GNU-compatible order:
1. Read existing ACL from file
2. Apply `--set` or `--set-file` (replaces ACL entirely)
3. Apply `-m` / `-M` modifications
4. Apply `-x` / `-X` removals
5. Apply `-b` (remove all) and `-k` (remove default)
6. Recalculate mask (unless `-n`)
7. Validate (unless `--test`)
8. Write back

### Restore Mode

The `--restore` flag reads a file produced by `getfacl` and applies ACLs to each file listed. The file format includes `# file:`, `# owner:`, and `# group:` header lines followed by ACL entries. The implementation parses the file, extracts the target path and ACL entries for each file block, and applies them. Chown/chgrp operations from the `# owner:` / `# group:` lines are intentionally out of scope (see Out of Scope).

### Traversal Flags

- Default walk mode is physical (`-P`): do not follow symlinks
- `-H`: dereference symlinks given on the command line before starting traversal
- `-L`: follow all symlinks encountered during traversal
- These flags mirror the pattern already implemented in `getfacl` and `chmod`

### Error Handling

- Missing required operand: print usage to stderr, exit 1
- File not found / cannot stat: print error to stderr, continue processing remaining files, exit non-zero
- Invalid ACL entry text: print error to stderr, exit 1
- ACL application failure (e.g., filesystem doesn't support ACLs): print error to stderr, continue, exit non-zero

## Testing Decisions

### What Makes a Good Test

Tests exercise the binary at the command-line level using `assert_cmd`, `assert_cmd_pat`, `assert_cmd_pat_stderr`, and `assert_cmd_not_pat` helpers from the shared test framework. Tests verify externally observable behavior only — output text, exit codes, and the resulting ACL state as read back by `getfacl` or the system ACL tools. Implementation internals (e.g., which libacl functions are called) are not tested.

### Modules Under Test

- `setfacl` command (the only new module)
- Indirect coverage of existing ACL infrastructure through round-trip testing (`setfacl` modifies, `getfacl` reads back)

### Prior Art

The existing `test_getfacl.sh` provides the pattern:
- Setup phase: create test files and directories in `$TMPDIR`, optionally use system `setfacl` to prepare known ACL states
- Test assertions: run the modbox binary and verify output with `assert_cmd_pat` / `assert_cmd_pat_stderr`
- Conditional testing: check `HAVE_ACL` before running ACL-dependent tests (some filesystems/tmp directories don't support ACLs)
- Cleanup: remove temporary test files

### Test Strategy for setfacl Tests

The `test_setfacl.sh` will follow the same conditional pattern — using `setfacl` (the system's, if available) to prepare ACL states, then testing modbox's `setfacl` by modifying those states and reading back with modbox's `getfacl`, creating a full round-trip.

## Out of Scope

- **Changing file ownership**: The `# owner:` and `# group:` lines in `--restore` mode are parsed but ownership changes are not applied. That behavior belongs to `chown` and `chgrp`, not `setfacl`.
- **Filesystem-level ACL enablement**: `setfacl` only manages ACL entries on filesystems that already support them. Mount options and filesystem creation are out of scope.
- **NFSv4 / RichACL support**: Only POSIX.1e ACLs (the `acl_get_file` / `acl_set_file` API from libacl) are supported. This matches the scope of the existing `getfacl` command.
- **Non-recursive symbolic link operations**: `setfacl` operates on the target of symlinks by default, like GNU setfacl. Utilities like `chmod -h` (operate on the symlink itself) apply to a different permission model and are out of scope.

## Further Notes

- The `getfacl` test file already uses the system `setfacl` as a helper to create ACL test fixtures. Once modbox's `setfacl` is implemented, the `getfacl` tests could optionally be updated to use modbox's own `setfacl` instead, but this is a separate, non-blocking cleanup task.
- The Makefile auto-discovers all `.cpp` sources under `src/`, so adding `src/commands/setfacl.cpp` requires no build system modifications — just create the file and it gets compiled.
- Command registration uses the `REGISTER_COMMAND("setfacl", setfacl_command, "...")` macro at the bottom of the implementation file, matching all existing commands.
