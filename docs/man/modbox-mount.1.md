% MODBOX-MOUNT(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-mount — mount a filesystem or list mounted filesystems

# SYNOPSIS

**modbox mount** [*OPTION*]... [device [dir [type [options]]]]

# DESCRIPTION

Mount a filesystem or list currently mounted filesystems.

When invoked without arguments, or with **-a**/**--all**, the command
reads */proc/mounts* and prints a table of all currently mounted
filesystems.

Otherwise, it attempts to mount *device* at *dir* with the given
*type* and *options*.

# OPTIONS

**-a**, **--all**
:   List all currently mounted filesystems by reading */proc/mounts*.
    This is the default behaviour when no positional arguments are given.

**--fake**
:   Dry-run mode. Print what would be done without actually calling
    the kernel mount syscall. Useful for scripting and testing.

**-t**, **--type** *FSTYPE*
:   Specify the filesystem type explicitly (e.g. `ext4`, `tmpfs`,
    `bind`). When omitted, the kernel auto-detects the type.

**-O**, **--options** *OPTIONS*
:   Provide comma-separated mount options (e.g. `rw`, `ro`, `nosuid`,
    `noexec`). Special options `bind` and `remount` set the
    corresponding kernel flags.

**--target** *DIR*
:   Explicitly specify the mount point directory. Overrides the
    positional *dir* argument when both are provided.

**-h**, **--help**
:   Display help and exit.

**-v**, **--version**
:   Output version information and exit.

# OUTPUT FORMAT (LIST MODE)

When listing mounts, the output is a whitespace-aligned table with
columns: *source*, *target*, *fstype*, *options*, *dump*, *pass*.

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. permission denied, missing operand,
    invalid option, or kernel mount failure).

# EXAMPLES

```bash
# List all mounted filesystems
modbox mount
modbox mount -a

# Dry-run a mount operation
modbox mount --fake /dev/sdb1 /mnt/data -t ext4 -O rw

# Mount with explicit target
modbox mount --fake /dev/sdb1 --target /mnt/data

# Mount a bind mount (dry-run)
modbox mount --fake /source /dest -O bind

# Show help
modbox mount --help
modbox mount --version
```

# NOTES

- This implementation requires **CAP_SYS_ADMIN** (root) to perform
  actual mount operations. In unprivileged environments, use
  **--fake** to preview the operation.
- Loop device setup (`-O loop`) is not automated; users must set up
  the loop device themselves (e.g. with `losetup`).
- */etc/fstab* is not read; all mount parameters must be provided
  explicitly.

# SEE ALSO

**modbox-umount**(1), **modbox-df**(1), **modbox**(1)
