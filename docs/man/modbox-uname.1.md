% MODBOX-UNAME(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-uname - print system information

# SYNOPSIS

**modbox uname** [*OPTION*]...

# DESCRIPTION

Print certain system information. With no option, **-s** (kernel name) is
implied.

# OPTIONS

**-a**, **--all**
:   Print all information, in the order: kernel name, network node
    hostname, kernel release, kernel version, machine hardware name,
    processor type, hardware platform, operating system.

**-s**, **--kernel-name**
:   Print the kernel name.

**-n**, **--nodename**
:   Print the network node hostname.

**-r**, **--kernel-release**
:   Print the kernel release.

**-v**, **--kernel-version**
:   Print the kernel version.

**-m**, **--machine**
:   Print the machine hardware name.

**-p**, **--processor**
:   Print the processor type (non-portable; may report "unknown").

**-i**, **--hardware-platform**
:   Print the hardware platform (non-portable; may report "unknown").

**-o**, **--operating-system**
:   Print the operating system.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Print the kernel name (default behavior)
modbox uname

# Print all system information
modbox uname -a

# Print just the machine architecture (e.g. x86_64)
modbox uname -m

# Print kernel release for a build script
modbox uname -r

# Print the operating system name
modbox uname -o
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid option).

# NOTES

- `-p` and `-i` are non-portable and may report "unknown" on some
  platforms.
- When multiple options are given, information is printed in the fixed
  order used by **-a**, not the order the options appear on the command
  line.

# SEE ALSO

**modbox-hostname**(1), **modbox-arch**(1), **modbox**(1)
