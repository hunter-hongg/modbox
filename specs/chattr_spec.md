# ModBox: Linux File Attributes — chattr Implementation Specification

## Problem Statement

ModBox implements 129 commands covering all standard GNU CoreUtils, but lacks filesystem attribute manipulation tools like `chattr`. Users who rely on modbox as a drop-in CoreUtils replacement for Linux systems cannot change file attributes (immutable, append-only, etc.) on ext2/ext3/ext4 filesystems. This is a fundamental filesystem administration tool present on every Linux distribution.

## Solution

Implement `chattr` — a command that changes file attributes on Linux ext2/ext3/ext4/ext4 filesystems via the `ioctl()` `FS_IOC_SETFLAGS`/`FS_IOC_GETFLAGS` interface. The command follows the standard e2fsprogs `chattr` behavior in terms of command-line interface, exit codes, error messages, and functional semantics.

## User Stories

1. As a system administrator, I want to mark a file as immutable (`+i`), so that it cannot be modified, deleted, or renamed even by root.

2. As a system administrator, I want to mark a file as append-only (`+a`), so that data can only be appended to it (useful for logs).

3. As a user, I want to remove file attributes (`-i`, `-a`), so that I can revert a file to normal state.

4. As a user, I want to set multiple attributes in one invocation (`chattr +ia file`), so that I can efficiently configure file protections.

5. As a user, I want to view the current attributes of a file via `lsattr` (companion command), so that I can verify what attributes are set.

6. As a user, when I run `chattr --help`, I want to see usage instructions for the chattr command.

7. As a user, when I run `chattr --version`, I want to see version information.

8. As a user, I want to use the `-R` (recursive) flag to change attributes on an entire directory tree, so that I can bulk-configure file protections.

9. As a user, I want to use the `-V` (verbose) flag to see which files had their attributes changed, so that I can monitor the operation's progress.

10. As a user, I want to use the `-f` (suppress most error messages) flag to minimize output when processing many files, so that I can focus on actual errors.

11. As a user, I want to run `chattr -=attributes file` to explicitly set only the specified attributes (clearing all others), so that I can normalize file attributes to a known state.

12. As a user, when I try to set the immutable flag on a file I don't own (without CAP_LINUX_IMMUTABLE), I want an appropriate "Operation not permitted" error.

13. As a user, when I specify an invalid attribute letter (e.g., `chattr +z file`), I want an error message listing valid attributes.

14. As a user, when I run `chattr` with no arguments, I want a usage error message displayed.

15. As a user, when I specify a non-existent file, I want an appropriate error message.

16. As a user, I want to use the `-p` (project) flag to set the project ID on a file, so that I can manage project quota hierarchies.

17. As a user, I want to use the `-v` (version) flag to set the file's version/generation number, so that I can manage versioning metadata.

18. As a user, when I apply attributes to a symlink, I want the operation to affect the symlink target (not the symlink itself), matching standard chattr behavior.

19. As a user, when I run `chattr -R +i /path/to/dir`, I want the immutable flag applied recursively to all files and directories under the given path.

20. As a user, when I run `chattr` on a filesystem that doesn't support extended attributes (e.g., tmpfs, procfs, sysfs), I want an appropriate error rather than a crash.

21. As a user, when I run `chattr` with `--preserve-root`, I want it to refuse to operate recursively on `/`.

22. As a user, when I run `chattr` with `--no-preserve-root`, I want it to allow recursive operation on `/`.

## Implementation Decisions

### Command Interface

- **Signature**: `int chattr_command(int argc, char** argv)`
- **Argument parsing**: argtable3, following the pattern established by `chmod` and `chcon`
- **Registration**: `REGISTER_COMMAND("chattr", chattr_command, "Change file attributes on a Linux file system")`

### Options (matching e2fsprogs chattr)

| Option | Long | Description |
|--------|------|-------------|
| `-R` | `--recursive` | Recursively change attributes of directories and their contents |
| `-V` | `--verbose` | Output a diagnostic for every file processed |
| `-f` | `--suppress` | Suppress most error messages |
| `-v` | `--version` | Set the file's version/generation number |
| `-p` | `--project` | Set the project ID |
| | `--preserve-root` | Fail to operate recursively on `/` |
| | `--no-preserve-root` | Do not treat `/` specially (the default) |
| | `--help` | Display help and exit |
| `<mode>` | | `[+-=][aAcCdDeijsStTu]` — operator followed by attribute letters |

The mode is a positional argument (not an option flag). The operator is `+` (add), `-` (remove), or `=` (set exactly). Multiple mode strings can be provided (e.g., `chattr +i -a file`).

### Supported Attributes

| Letter | Flag | Constant | Description |
|--------|------|----------|-------------|
| `a` | ATR_APPEND | `FS_APPEND_FL` | Append-only |
| `A` | ATR_NOATIME | `FS_NOATIME_FL` | No atime updates |
| `c` | ATR_COMPR | `FS_COMPR_FL` | Compressed |
| `d` | ATR_NODUMP | `FS_NODUMP_FL` | No dump |
| `D` | ATR_DIRSYNC | `FS_DIRSYNC_FL` | Synchronous directory updates |
| `e` | ATR_EXTENT | `FS_EXTENT_FL` | Extent format (read-only, cannot be changed) |
| `i` | ATR_IMMUTABLE | `FS_IMMUTABLE_FL` | Immutable |
| `j` | ATR_JOURNAL | `FS_JOURNAL_FL` | Data journaling |
| `s` | ATR_SECRM | `FS_SECRM_FL` | Secure deletion |
| `S` | ATR_SYNC | `FS_SYNC_FL` | Synchronous updates |
| `t` | ATR_NOTAIL | `FS_NOTAIL_FL` | No tail-merging |
| `T` | ATR_TOPDIR | `FS_TOPDIR_FL` | Top of directory hierarchy |
| `u` | ATR_UNRM | `FS_UNRM_FL` | Undeletable |
| `x` | ATR_COMPRBLK | `FS_COMPRBLK_FL` | Compression (deprecated, no-op) |
| `X` | ATR_COMPRESS_RAW | `FS_COMPRESS_RAW_FL` | Compression raw access (deprecated, no-op) |
| `Z` | ATR_COMPR_DIRTY | `FS_COMPR_DIRTY_FL` | Compressed dirty file (deprecated, no-op) |

The `e` (extent) attribute is read-only — it will be listed but cannot be added or removed. The deprecated compression attributes (`x`, `X`, `Z`) are accepted as no-ops for compatibility.

### Technical Implementation

- **Syscall interface**: Use `ioctl(fd, FS_IOC_GETFLAGS, &flags)` to read, `ioctl(fd, FS_IOC_SETFLAGS, &flags)` to write, both from `<linux/fs.h>`.
- **File opening**: Open with `O_RDONLY | O_NONBLOCK` (to avoid blocking on FIFOs or devices).
- **Symlink handling**: Use `open()` (follows symlinks) by default; no `-h` flag needed for initial implementation since `chattr` does not support `-h` (unlike `chmod`).
- **Recursive traversal**: Use `nftw()` with `FTW_PHYS`, following the same pattern as `chmod` and `chcon`.
- **Project ID**: Use `ioctl(fd, FS_IOC_SETPROJECT, &projid)` for `-p`.
- **Version number**: Use `ioctl(fd, FS_IOC_SETVERSION, &version)` for `-v`.
- **Error accumulation**: Continue processing all files even if some fail, accumulate error count, return non-zero if any failed.

### Error Handling

- Print errors to stderr in format: `chattr: <path>: <error message>`
- Exit code 0 on success, non-zero if any errors occurred.
- Suppress error messages when `-f` is given.
- Missing operand errors print to stderr and return exit code 1.

### Files

- Header: `include/commands/chattr.hpp` — contains `ChattrOptions` struct and `chattr_command` declaration
- Source: `src/commands/chattr.cpp` — contains implementation and `REGISTER_COMMAND`
- No build system changes needed (automatic source discovery via glob)

## Testing Decisions

### Test approach

- Test at the command level via `tests/test_chattr.sh` using the `assert_cmd`, `assert_cmd_pat`, `assert_cmd_not_pat`, and `assert_cmd_pat_stderr` helpers from `tests/framework.sh`.
- Tests run under the calling user's permissions; immutable/append-only tests that require `CAP_LINUX_IMMUTABLE` will be skipped gracefully when the test environment lacks the capability.
- Temporary files are created in `$TMPDIR` (managed by the framework's `mktemp` cleanup).

### Prior art

- `tests/test_chcon.sh` — demonstrates recursive operation, error handling, and argtable3-based commands
- `tests/test_chmod.sh` — demonstrates mode parsing, recursive, verbose, and reference-based operations
- `tests/test_chown.sh` — demonstrates ownership changes with similar option patterns

### Test cases

1. **Help output**: `chattr --help` prints usage information
2. **Version output**: `chattr --version` prints version
3. **No arguments**: prints error about missing operand
4. **Non-existent file**: prints error for non-existent file
5. **Invalid attribute**: prints error for invalid attribute letter (e.g., `chattr +z file`)
6. **Set and verify immutable flag**: `chattr +i file` and verify via `lsattr` (if available) or direct ioctl
7. **Remove immutable flag**: `chattr -i file` after setting it
8. **Set exact attributes**: `chattr =i file` clears all other attributes and sets only immutable
9. **Multiple attributes**: `chattr +ia file` sets both append-only and immutable
10. **Recursive operation**: `chattr -R +i dir/` applies to directory tree
11. **Verbose output**: `chattr -V +i file` prints diagnostic
12. **Suppress errors**: `chattr -f +i nonexistent` suppresses error output
13. **Preserve root**: `chattr --preserve-root -R +i /` refuses
14. **Project ID**: `chattr -p 0 file` sets project ID (if supported by filesystem)
15. **Version number**: `chattr -v 1 file` sets version number
16. **Symlink traversal**: `chattr +i symlink` changes the target's attributes

Tests that modify actual file attributes (immutable, append-only) will be best-effort — they may fail on filesystems that don't support them or when run without sufficient privileges, but the command should still parse arguments and produce correct output for all common cases.

## Out of Scope

- **lsattr**: The companion command for listing attributes is a separate command and not part of this spec. It should be implemented in a follow-up.
- **chattr for non-Linux platforms**: The command is Linux-specific due to `FS_IOC_*` ioctls. No portability layer.
- **ACL manipulation**: Access control lists are managed by `setfacl`/`getfacl`, not `chattr`.
- **Extended attributes (xattr)**: User-space extended attributes are managed by `setfattr`/`getfattr`, not `chattr`.
- **`-h` no-dereference flag**: Not supported by e2fsprogs `chattr`; symlinks are always followed.
- **BSD `chflags` compatibility**: The semantics are Linux-specific; no attempt to abstract for BSD.
- **`-H`/`-L`/`-P` traversal flags**: e2fsprogs `chattr` does not support these; follow symlinks only.

## Further Notes

- `chattr` is not part of GNU CoreUtils — it comes from e2fsprogs. However, it is a standard Linux command that modbox should support for completeness.
- The `FS_IOC_GETFLAGS`/`FS_IOC_SETFLAGS` ioctls are defined in `<linux/fs.h>` and require `#include <linux/fs.h>` plus `#include <sys/ioctl.h>`.
- File attributes are stored in the inode and persist across reboots.
- Setting `+i` or `+a` requires `CAP_LINUX_IMMUTABLE` capability (typically root). The command should handle `EPERM` gracefully.
- The `e` (extent) attribute is automatically set on ext4 files and cannot be removed — the command should accept it in `=` mode (it's always set) but issue a warning or silently ignore attempts to remove it.