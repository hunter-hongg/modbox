# ModBox Implementation Spec: hostname Command

## Problem Statement

The `hostname` command is used to get or set the system's host name (the network name of the machine). This command is part of the standard GNU Core Utilities package but is not currently implemented in ModBox, creating a gap in compatibility with expected Unix toolset behavior.

## Solution

Implement the `hostname` command that follows the GNU coreutils `hostname` specification, allowing users to both retrieve and optionally set the system host name via appropriate options and arguments.

## User Stories

1. As a developer, I want to run `hostname` without any options so that my current host name is printed to stdout.
2. As a script writer, I want to run `hostname -f` so that the fully qualified domain name is printed.
3. As an administrator, I want to run `hostname -i` so that the IP address(es) associated with the host name are printed.
4. As a user, I want to run `hostname -a` so that the alias names for the host are printed.
5. As a user, I want to run `hostname -s` so that the short (single label) host name is printed.
6. As a user, I want to run `hostname -p` so that the domain name is printed.
7. As a user, I want to run `hostname -d` so that the DNS domain name is printed.
8. As an administrator, I want to set a new host name by running `hostname NEWNAME` (with appropriate privileges).
9. As a user, I want to run `hostname --help` to display usage information and exit successfully.
10. As a user, I want to run `hostname --version` to display version information and exit successfully.
11. As a script writer, I want `hostname` to behave consistently with GNU coreutils when multiple options are combined.
12. As a security-conscious user, I expect that setting the host name requires appropriate privileges (CAP_SETUID) and fails gracefully otherwise.
13. As a portable script author, I want `hostname -I` to print all configured IPv4/IPv6 addresses (one per line, space-separated).
14. As a diagnostic tool, I want `hostname --fqdn` to output the fully qualified domain name.
15. As a user, I expect `hostname` to resolve the host name to an IP address via standard system calls and handle resolution failures gracefully.

## Implementation Decisions

### Command Interface

- Single argument after options is interpreted as the new host name if no option is given that prints information. Options must appear before the new host name.
- The default behavior (no options) prints the nodename (same as `-n`).
- All printing options (`-a`, `-d`, `-f`, `-i`, `-I`, `-s`, `-p`, `--fqdn`) output information and ignore any provided host name argument.

### Option Set (matching GNU coreutils)

| Option | Description |
|--------|-------------|
| `-a`, `--aliases` | Print alias names |
| `-d`, `--domain` | Print DNS domain name |
| `-f`, `--fqdn` | Print fully qualified domain name |
| `-i`, `--addresses` | Print IP addresses (space separated) |
| `-I`, `--all-addresses` | Print all configuration addresses (one per line) |
| `-s`, `--short` | Print node name (short form) |
| `-p`, `--precise` | Print domain name (precise, same as `-d`) |
| `-y`, `--ypbind`, `-N`, `--nis` | Deprecated / ignored (NIS support removed in modern systems) |
| `--help` | Display help and exit |
| `--version` | Display version and exit |

**Note**: GNU coreutils also includes `-A`, `--ethers`, `-m`, `--mac-address`, `-Y`, `--yp-match` — these may be out of scope for initial implementation.

### System Calls and Resolution Logic

1. **Get host name**: Use `gethostname()` system call (POSIX.1-2001) to retrieve the current nodename. Maximum size: `HOST_NAME_MAX` (from `<limits.h>`), typically 64 bytes.
2. **Set host name**: Use `sethostname()` system call; requires CAP_SETUID capability (root). Failure with appropriate perror message.
3. **Fully qualified domain name**: Combine nodename and domain from `gethostname()` + DNS resolution via `gethostbyname()` or modern `getaddrinfo()` / `getnameinfo()`. Prefer `/etc/hosts` resolution followed by DNS.
4. **IP addresses**: Use `getaddrinfo()` with AI_ADDRCONFIG to retrieve all local addresses associated with the host name. Filter to only active/configured addresses.
5. **Domain name**: Can be obtained from `getdomainname()` (POSIX) or parsed from FQND by extracting the suffix after the first dot. Also check `/etc/resolv.conf` for search domain.

### Error Handling

- Permission denied when setting host name without appropriate privileges → print "hostname: permission denied" to stderr.
- Host name lookup failures (e.g., `gethostname` returns -1) → print error via `perror` and exit non-zero.
- Invalid option combinations (e.g., both `-i` and `-I` together should be allowed; they are complementary) → follow GNU behavior (permissive).
- Unknown option → print usage error to stderr and exit non-zero.

### File Structure

- Header: `include/commands/hostname.hpp` — declares `hostname_command(int argc, char** argv)`
- Source: `src/commands/hostname.cpp` — implements the command with argtable3 parsing, system calls, and registration via `REGISTER_COMMAND("hostname", hostname_command, "Show or set the host name")`

### Versioning

```
hostname (modbox) 1.0
Copyright (C) 2026 modbox
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute.
There is NO WARRANTY, to the extent permitted by law.
```

## Testing Decisions

### Test Approach

Use the existing test framework at `tests/run_tests.sh`. Follow the pattern of simple bash assertions checking command output against expected strings. See existing tests for similar commands like `uname`, `id`, `pwd`.

### Test Cases to Implement

1. **Basic functionality**: `hostname` outputs a non-empty string matching the actual system host name.
2. **-n option**: Explicitly `-n` should give the same output as default.
3. **-s option**: Should output the short name (first dot-separated component).
4. **-d option**: Should output the DNS domain name.
5. **FQDN combination**: `-f` should print full domain including host name.
6. **-i option**: Should print one or more IP addresses associated with the host.
7. **-I option**: Should print all configured IP addresses, one per line.
8. **Setting host name**: Test successful set with root/capability, verify subsequent get shows new name.
9. **Permission denied**: Attempting to set host name as non-root should fail with appropriate error message.
10. **Help output**: `--help` displays usage, exits with status 0.
11. **Version output**: `--version` displays version string, exits with status 0.
12. **Invalid option**: Passing unknown option prints error, exits non-zero.
13. **Mixed options**: `-fs` or similar combinations produce correct output order.

### Existing Test Patterns

Examine `tests/` directory for patterns:
- Simple output comparison: `./modbox echo hello | grep -q "hello"`
- Exit status checks: `! ./modbox invalid_option`
- Error message regex matches: `./modbox bad_option 2>&1 | grep -q "error"`

## Out of Scope

- Ethernet/MAC address options (`-A`, `-m`) — deprecated and rarely used.
- NIS/YP bind operations (`-y`, `-N`) — legacy functionality.
- Complex DNS resolver configuration beyond basic getaddrinfo/gethostbyname.
- IPv6-only address handling nuances (basic IPv4/IPv6 mix handled by `getaddrinfo`).
- Persistent host name changes across reboots (systemd/hwclock/etc. integration).

## Further Notes

- The GNU coreutils hostname has evolved over versions; aim for compatibility with GNU coreutils 8.x/9.x behavior.
- On Linux, `/etc/hostname` file may contain the persistent host name — consider reading/writing this for persistence is out of scope.
- Consider linking against `-lnsl` if needed for older gethostbyname, but prefer `getaddrinfo` which is in libc.
- For cross-platform compatibility (future), wrap system calls appropriately; initial implementation targets Linux.
