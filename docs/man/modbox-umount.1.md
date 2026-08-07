% MODBOX-UMOUNT(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-umount — unmount a filesystem

# SYNOPSIS

**modbox umount** [*OPTION*]... device|directory

# DESCRIPTION

Unmount a previously mounted filesystem.

# OPTIONS

**-l**, **--lazy**
:   Lazy unmount. Detach the filesystem immediately and clean up all
    references when it is no longer busy. Equivalent to the
    `MNT_DETACH` flag.

**-f**, **--force**
:   Force unmount. Useful for unreachable NFS mounts or other
    situations where the standard unmount would hang. Equivalent to
    the `MNT_FORCE` flag.

**--fake**
:   Dry-run mode. Print what would be done without actually calling
    the kernel unmount syscall.

**-h**, **--help**
:   Display help and exit.

**-v**, **--version**
:   Output version information and exit.

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. permission denied, not mounted, or
    invalid operand).

# EXAMPLES

```bash
# Unmount a filesystem
modbox umount /mnt/data

# Lazy unmount
modbox umount -l /mnt/data

# Force unmount (e.g. for stuck NFS)
modbox umount -f /mnt/nfs

# Dry-run
modbox umount --fake /mnt/data

# Show help
modbox umount --help
modbox umount --version
```

# NOTES

- This implementation requires **CAP_SYS_ADMIN** (root) to perform
  actual unmount operations. In unprivileged environments, use
  **--fake** to preview the operation.
- Unlike GNU *umount*, this command does not support recursive
  unmount (`-R`).

# SEE ALSO

**modbox-mount**(1), **modbox-df**(1), **modbox**(1)
