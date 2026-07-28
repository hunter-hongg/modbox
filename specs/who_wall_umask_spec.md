# ModBox: GNU Coreutils Compatibility - wall, who, umask Implementation Specification

## Problem Statement

ModBox aims to be a comprehensive replacement for GNU Core Utilities. Currently, the following core utilities are missing from the implementation:

1. **wall** — Write a message to all logged-in users (like GNU `wall`)
2. **who** — Report information about users currently logged in (like GNU `who`)
3. **umask** — Set or read file creation permission mask (like GNU `umask`)

These are fundamental system administration and user communication tools present in GNU coreutils. Their absence limits modbox's completeness as a coreutils replacement.

## Solution

Implement three commands (`wall`, `who`, `umask`) that follow GNU coreutils behavior in terms of command-line interface, exit codes, error messages, and functional semantics. Each will be implemented as a C++ source file under `src/commands/`, with a corresponding header in `include/commands/`, registered via the existing `REGISTER_COMMAND` macro, and tested via bash test scripts under `tests/`.

All commands use argtable3 for consistent argument parsing across the codebase.

## User Stories

### wall Command

1. As an administrator, I want to send a system-wide message to all logged-in users so that I can communicate important announcements.

2. As a user, when I run `wall --help`, I want to see usage instructions for the wall command.

3. As a user, when I run `wall --version`, I want to see version information like "wall (modbox) 1.0".

4. As an administrator, when I run `wall Hello World!`, I want the message "Hello World!" to be sent to all users currently logged into terminals.

5. As an administrator, when I run `wall < file.txt`, I want all lines from `file.txt` to be broadcast to all logged-in users.

6. As an administrator using root, when I run `wall -n Message`, I want the message to be sent without the standard banner line at the top.

7. As an administrator, when I run `wall -g admins user_message`, I want to send the message only to members of the "admins" group instead of all users.

8. When `wall -g` is specified with a non-existent group, I want an error message printed and an appropriate exit code returned.

9. As a non-root user, when I try to use the `-n` (nobanner) option, I want to receive a permission denied error since only root can suppress the banner.

10. As an administrator, when I run `wall -t 30 message`, I want the broadcast message to time out after 30 seconds.

11. When all terminal devices (TTYs) are unwritable by the current process, when I run wall, I want appropriate error messages per TWRITER rather than failing entirely.

12. When running `wall` with no arguments, it reads the message from standard input until EOF (Ctrl+D).

13. As a user, when I attempt to run `wall` on a system where utmp/wtmp access fails due to permissions, I want a clear error message.

14. When wall successfully broadcasts the message to at least one user, it should return exit code 0.

15. When wall fails to broadcast to any user or encounters fatal errors, it returns a non-zero exit code.

16. The default banner format for wall should include the username, host, and date in a recognizable format similar to GNU wall.

17. As a remote user connected via SSH, when receiving a wall message, I want to see the originating hostname (not just localhost).

### who Command

18. As a user checking who else is on the system, when I run `who`, I want a simple list of currently logged-in users showing username, terminal, login time, and optionally remote host.

19. As a user, when I run `who --help`, I want to see usage information for the who command.

20. As a user, when I run `who --version`, I want to see version information like "who (modbox) 1.0".

21. When invoked as `who -a` or `who --all`, I want all available information displayed in a detailed format including boot times, dead processes, login processes, system runs-level changes, and more.

22. When invoking `who --boot` or `-b`, I want to display the time of the last system boot.

23. When invoking `who --dead` or `-d`, I want to list entries marked as "dead" processes.

24. When invoking `who --login` or `-l`, I want to show lines describing system login processes waiting for user authentication.

25. When invoking `who --process` or `-p`, I want to list active processes spawned by init (like getty).

26. When invoking `who --runlevel` or `-r`, I want to display the current and previous system runlevels.

27. When invoking `who --time` or `-t`, I want to display the last time the system clock was changed.

28. When invoking `who --short` or `-s`, I want a short output format showing only username, terminal, and login time (this is often the default).

29. When invoking `who --full` or `-u`, I want to display idle time and message status for each user.

30. When invoking `who --writable` or `-T` or `-W`, I want to display whether each user's terminal accepts messages (+, -, or ?).

31. When invoking `who --heading` or `-H`, I want column headers printed above the output (NAME, LINE, FROM, TIME, IDLE, JCPU, PCWP, LOGIN@).

32. When invoking `who --count` or `-q`, I want a quick summary listing all usernames followed by the total count of logged-in users.

33. When invoking `who --mesg` or `-m`, I want to show information only about the current terminal (equivalent to `am i`).

34. When two arguments are provided to `who` like `who am i`, the `-m` option is implicitly enabled.

35. When specifying multiple output options together (e.g., `who -du`), all requested information should be combined appropriately in the output.

36. When `who` cannot read the utmp file (e.g., `/var/run/utmp`) due to permissions, it prints an error message to stderr and exits with a non-zero status.

37. By default, who reads from `/var/run/utmp` unless another file path is provided as an argument.

38. When an alternate utmp/wtmp file path is specified (e.g., `who /var/log/wtmp`), the command reads user session history from that file.

39. Hostnames are resolved via DNS unless disabled by an option; unresolvable hosts display IP addresses or remain as-is.

40. Output format follows the standard utmp entry ordering conventions, with new entries appearing first.

41. If no users are logged in, `who` produces no output (or reports nothing) but does not produce an error.

42. The `who` command properly handles entries from various login types including console, SSH, virtual terminals, and graphical sessions.

43. The idle time format for `who -u` matches GNU who format (old, minutes, or ?).

44. JCPU and PCWP columns show appropriate values when available.

### umask Command

45. As a user checking my file creation permissions, when I run `umask`, I want to see the current file mode mask in octal format (typically 0022).

46. As a user, when I run `umask --help`, I want to see usage instructions for the umask command.

47. As a user, when I run `umask --version`, I want to see version information like "umask (modbox) 1.0".

48. When invoking `umask -S`, I want the mask displayed in symbolic form (e.g., u=rwx,g=rx,o=rx) instead of octal.

49. When invoking both `umask -p` and omitting any mask argument, I want the output in a format suitable for re-input as a command (e.g., `umask 0022`).

50. When setting a new mask via `umask 0002`, I want the process's file creation mask to be updated to the specified value.

51. When setting a new mask via `umask u+rwx,g=rx,o=rx` (symbolic mode), I want the mask computed and set according to chmod-like rules.

52. When a symbolic mask is provided that contains invalid characters or modes (e.g., `umask xyz`), I want an error message printed and a non-zero exit status returned.

53. When an octal mask begins with digits other than 0-7 or is too long, I want appropriate error handling.

54. When combining `-p` with a mask setting (e.g., `umask -p 0022`), the masked value should be printed in re-input form before applying the change.

55. When combining `-S` with a mask setting (e.g., `umask -S u=rwx`), the symbolic representation should be printed in re-input form and the new mask applied.

56. After calling `umask` to set a new mask, subsequent child processes inherit the updated mask.

57. Multiple invocations of `umask` in the same shell session reflect cumulative changes correctly.

58. When providing an invalid combination of options (e.g., both formats conflicting), `umask` reports an error.

59. The default octal output of `umask` includes the leading zero (e.g., 0022 rather than 22) to indicate octal interpretation.

60. `umask` respects the requirement that the mask value is interpreted as octal when given as digits.

### General / Integration

61. All three commands appear in the modbox command help output when invoking `modbox` without arguments.

62. Each command works when invoked directly through the modbox binary: `modbox wall`, `modbox who`, `modbox umask`.

63. Error messages follow the modbox convention: `command: error message` on stderr.

64. Help output uses the same formatting style as other modbox commands.

65. Version output follows the pattern "command (modbox) 1.0".

66. All commands compile successfully with the Makefile build system without requiring modifications to the build configuration.

67. Commands do not introduce new linking dependencies beyond what is already used (argtable3, libc).

## Implementation Decisions

### Modules and Interfaces

Three new commands will be added, each with a header and source file:

1. **wall**: `include/commands/wall.hpp` and `src/commands/wall.cpp`
2. **who**: `include/commands/who.hpp` and `src/commands/who.cpp`
3. **umask**: `include/commands/umask.hpp` and `src/commands/umask.cpp`

Each command function must match the signature: `void command_name(int argc, char** argv)`.

Each source file will include its own header and `command_macros.hpp`, then register itself using `REGISTER_COMMAND("name", command_name, "help description")`.

### Technical Clarifications

- **Argument Parsing**: Use argtable3 consistently across all three commands, matching the pattern established by existing commands like `tty`, `groups`, and `whoami`.

- **Who Implementation**: Access the utmp database using `getutent()`, `setutent()`, and `endutent()` from `<utmp.h>` or `<sys/utmp.h>` (Linux-specific). Also need `<pwd.h>`, `<grp.h>`, and possibly `<netdb.h>` for hostname resolution. Support multiple output flags simultaneously.

- **Wall Implementation**: Iterate over logged-in users by reading utmp, find their associated TTY devices, and open them for writing (`/dev/ttyXX`). Broadcast the message to each writable TTY. Handle group filtering via `getgrnam()` and group membership checks. Root check via effective UID. Banner suppression requires EACCED check. Timeout may require signal handling with alarm() or selectable writes.

- **Umasks**: Use `umask()` system call to get/set the process file creation mask. For symbolic mode conversion, implement a mini-chmod-like parser to convert symbolic masks (like `u=rwx,g=rx,o=rx`) to octal values.

- **Header Files**: Minimal headers containing only the `command_name(int argc, char** argv)` declaration, following the pattern of other command headers.

- **Build System**: No changes required to `Makefile` — automatic source discovery via `find $(SRC_DIR) -name '*.cpp'` will pick up new `.cpp` files automatically.

### Schema Changes

No schema changes — this is pure implementation of command-line utilities without persistent data storage.

### API Contracts

All commands adhere to the same contract as existing modbox commands: take `argc` and `argv`, print to stdout/stderr, exit via `exit()` or return from main (commands don't call exit directly except in error cases following tty pattern), and register via the macro.

Exit codes:
- 0 for success
- 1 for usage errors or recoverable problems
- 2 for serious errors (e.g., cannot read utmp, permission denied)

### Specific Interactions

- **Wall and TTY devices**: Wall will open `/dev` directory entries, attempt to write to each user's controlling terminal. Unwritable terminals should result in individual error messages (if silent flag not set) but not abort the entire operation.

- **Who and utmp**: Who reads `/var/run/utmp` by default; fallback to `/etc/utmp` if needed. Should gracefully handle missing or unreadable files with appropriate error messages.

- **Umask and process state**: The umask change affects only the modbox process and its descendants, not persisting beyond the command invocation. When called via `modbox umask`, the change happens inside the modbox process and does not affect the caller's shell — this is inherent limitation of sub-process execution (standard for all shell built-in-like commands). Users should note this difference from shell builtin behavior.

## Test Strategy

Each command will have a dedicated test script:

1. `tests/test_wall.sh` — wall command tests
2. `tests/test_who.sh` — who command tests  
3. `tests/test_umask.sh` — umask command tests

Tests will cover:
- Basic functionality verification
- Help output (`--help`, `-h`)
- Version output (`--version`, `-V`)
- Option combinations
- Edge cases (empty input, special characters, etc.)
- Error conditions (invalid arguments, permission denied, file not found)
- Exit code correctness
- Output format compliance

The test runner `tests/run_tests.sh` will aggregate all tests.

## Out of Scope

- **Persistent umask shell builtin**: Implementing umask as a shell builtin that would modify the calling shell's environment is out of scope for the standalone command. The current design treats it as a standalone utility that changes its own process mask only.

- **Full wall security model**: Advanced features like access control lists, per-user permission checks beyond basic writability, and message approval workflows are out of scope for initial implementation.

- **Who time zone localization**: Displaying times in localized time zones based on user environment is out of scope; UTC/local system time is sufficient.

- **Wall encryption/signature**: Message signing or encryption for authenticity is out of scope.

- **Who alternative database sources**: Reading who information from network directories (LDAP, NIS) or alternative sources beyond local utmp/wtmp files is out of scope for initial implementation.

## Further Notes

- The implementation should aim for functional parity with GNU coreutils versions (wall 8.x+, who 8.x+, umask 8.x+) in terms of command-line interface and basic behavior.

- Exact output string matching for banner lines and formatting should approximate GNU behavior while being reasonable for a POSIX/Linux environment.

- Localization (gettext) support is not required initially; English-only output is acceptable.

- The commands should compile cleanly with warnings enabled and pass clang-tidy analysis.
