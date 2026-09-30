% MODBOX-ADDR2LINE(1) modbox | User Commands
% modbox project
% 2026-09-30

# NAME

modbox-addr2line - convert addresses to file names and line numbers

# SYNOPSIS

**modbox addr2line** [*OPTION*]... [addr]...

# DESCRIPTION

Read addresses from the command line or from standard input and print the
file name and line number for each address.

The addresses should be virtual addresses from the executable.  They may be
specified in decimal or hexadecimal (with a `0x` prefix).

With no addresses on the command line, the addresses are read from standard
input, one per line.

# OPTIONS

**-a**, **--addresses**
:   Print the address before the function name, file name and line number.

**-b** *BFDNAME*, **--target=***BFDNAME*
:   Specify the object-code format.  Accepted for compatibility but has no
    effect; modbox reads the format from the file itself.

**-C**, **--demangle**[=*STYLE*]
:   Print demangled symbol names.  An optional *STYLE* can be given as
    `--demangle=STYLE`, where *STYLE* is one of `gnu`, `lucid`, `arm`,
    `java`, or `auto`.  When no style is specified, `auto` is used.

**-e** *FILENAME*, **--exe=***FILENAME*
:   Specify the executable to be read.  Defaults to `a.out`.

**-f**, **--functions**
:   Also display the function name for each address.

**-i**, **--inlines**
:   Print the enclosing (inlined) functions for each address.  Currently
    a placeholder — modbox does not yet support inline function tracking.

**-p**, **--pretty-print**
:   Print the output in a human-friendly, single-line format.

**-s**, **--basenames**
:   Strip the directory from file names in the output.

**-j** *NAME*, **--section=***NAME*
:    Specify the section to use.  Accepted for compatibility but has no
     effect.

**-r**, **--no-recurse-limit**
:    Disable the recursion limit when demangling.

**-R**, **--recurse-limit**
:    Enable the recursion limit when demangling (default).

**-h**, **--help**
:    Display a usage summary and exit.

**-V**, **--version**
:    Output version information and exit.

# EXIT STATUS

**modbox addr2line** exits 0 on success.

# EXAMPLES

Resolve an address from a binary:

```
modbox addr2line -e program.elf 0x401020
```

Show the function name with demangling enabled:

```
modbox addr2line -f -C -e program.elf 0x401020
```

Read addresses from stdin, pretty-printed with addresses:

```
echo 0x401020 | modbox addr2line -p -a -f -e program.elf
```

# IMPLEMENTATION NOTES

This command uses libelf and libdw (elfutils) for DWARF debugging information
and libbfd (from binutils) for symbol demangling.  The module load bias is
retrieved via `dwfl_module_getelf` and added to user-supplied addresses before
calling `dwfl_module_getsrc` and `dwfl_module_addrsym`, so that PIE executables
are handled correctly.

# SEE ALSO

**modbox**(1), **modbox-strings**(1), **modbox-nm**(1), **modbox-objdump**(1)
