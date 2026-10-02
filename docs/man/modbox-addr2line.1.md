% MODBOX-ADDR2LINE(1) modbox | User Commands
% modbox project
% 2026-10-02

# NAME

modbox-addr2line - convert addresses to file names and line numbers

# SYNOPSIS

**modbox addr2line** [*OPTION*]... [*addr*]...

# DESCRIPTION

Read addresses from the command line or from standard input and print the
file name and line number for each address.

The addresses should be virtual addresses from the executable.  They may be
given in hexadecimal (`0x1234` or `1234`), or as a symbol name with an
optional offset (`main+0x10`).  A symbol may be spelled either mangled
(`_ZN3Foo3barEi`) or, when **-C** is in effect, demangled
(`Foo::bar(int)`).

With no addresses on the command line, they are read from standard input,
one per line.

# OPTIONS

**-a**, **--addresses**
:   Print the address before the function name, file name and line number.

**-b** *BFDNAME*, **--target=***BFDNAME*
:   Specify the object-code format.  Accepted for compatibility; modbox
    reads the format from the file itself.

**-C**, **--demangle**[=*STYLE*]
:   Print demangled symbol names.  An optional *STYLE* selects the
    demangling convention: `gnu-v3` (C++), `java`, `gnat` (Ada) or `auto`
    (the default, which sniffs the input).  The style names accepted here
    are the ones GNU addr2line recognises.

**-e** *FILENAME*, **--exe=***FILENAME*
:   Specify the executable to be read.  Defaults to `a.out`.

**-f**, **--functions**
:   Also display the function name for each address.

**-i**, **--inlines**
:   Print the enclosing inlined functions for each address, following
    `**FUNC** at **FILE**:**LINE** (inlined by) **FUNC** at ...`.

**-p**, **--pretty-print**
:   Print the output in a human-friendly single-line format.

**-s**, **--basenames**
:   Strip the directory from file names in the output.

**-j** *NAME*, **--section=***NAME*
:   Read *section-relative* offsets instead of absolute addresses.

**-r**, **--no-recurse-limit**
:   Disable the recursion limit when demangling.

**-R**, **--recurse-limit**
:   Enable the recursion limit when demangling (default).

**-h**, **--help**
:   Display a usage summary and exit.

**-v**, **-V**, **--version**
:   Output version information and exit.

# EXIT STATUS

**modbox addr2line** exits 0 on success and 1 when the executable cannot be
opened, is not an object file, has an unknown section given to **-j**, or is
given an unknown demangling style.

# EXAMPLES

Resolve an address from a binary:

```
modbox addr2line -e program.elf 0x401020
```

Show the demangled function name as well:

```
modbox addr2line -f -C -e program.elf 0x401020
```

Look up a symbol plus an offset:

```
modbox addr2line -f -C -e program.elf 'main+0x10'
```

Read addresses from stdin, pretty-printed with addresses:

```
echo 0x401020 | modbox addr2line -p -a -f -e program.elf
```

# IMPLEMENTATION NOTES

Symbol lookup, DWARF line resolution, inline unwinding and demangling all go
through libbfd, so modbox's answers match GNU addr2line's, including the
quirks of its token grammar: a leading `+` belongs to a hex number (`+5`
parses as `0x5`), a leading hex letter with no `+` in the token is a number
rather than a symbol, and a bare blank line on stdin resolves to address 0.

The demangling recursion limit and the `discriminator` suffix on line
numbers follow the reference as well.
