% MODBOX-ARCH(1) modbox | User Commands
% modbox project
% 2026-08-04

# NAME

modbox-arch - print machine hardware name

# SYNOPSIS

**modbox arch** [*OPTION*]...

# DESCRIPTION

Print the machine hardware name, as reported by the kernel.
This is equivalent to **uname** **-m**.

The output is a single line such as `x86_64`, `aarch64`, `armv7l`, or
`i686`, depending on the host architecture.

# OPTIONS

`-h`, `--help`
:   Display help and exit.

`-V`, `--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox arch
modbox arch --version
echo "Build for: $(modbox arch)"
```

# NOTES

- The output depends on the kernel's `uname(2)` call; it reflects the
  architecture of the running kernel, not the compiler target.
- This is a BusyBox-style no-op command — there are no data-processing
  options; it simply prints one string and exits.

# EXIT STATUS

`0` on success.

# SEE ALSO

**modbox**(1)
