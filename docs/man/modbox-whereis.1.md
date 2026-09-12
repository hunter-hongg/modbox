% MODBOX-WHEREIS(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-whereis - locate the binary, source, and manual-page files for a command

# SYNOPSIS

**modbox whereis** [*OPTION*]... [**-BMS** *dir*... **-f**] *NAME*...

# DESCRIPTION

**modbox whereis** locates the binary, source, and manual-page files for each
given *NAME* and prints them on one line as:

```
name: b /path... m /path... s /path...
```

The three classes are printed in that fixed order; a class with no match
contributes nothing. A name with no matches at all is printed as `name:` with
an empty list. The command exits `0` for every looked-up name.

By default all three classes are searched. Giving one or more of **-b**,
**-m**, or **-s** restricts the output to the selected class(es). The search
roots for each class are independent: **-B**, **-M**, and **-S** replace only
their own class's root list, leaving the other classes at their defaults.

# OPTIONS

**-b**
:   Search only for binaries.

**-B** *dirs*
:   Replace the binary lookup roots with the whitespace-separated list
    *dirs*. The list ends at the next option or at **-f**.

**-m**
:   Search only for manuals (and info files).

**-M** *dirs*
:   Replace the manual/info lookup roots with *dirs*.

**-s**
:   Search only for sources.

**-S** *dirs*
:   Replace the source lookup roots with *dirs*.

**-f**
:   Terminate the current **-B**/**-M**/**-S** directory list. The next
    argument is treated as a *NAME* or as a further option.

**-u**
:   Print only *unusual* names. A name is unusual when it does **not** have
    exactly one entry for each explicitly requested class — that is, a
    requested class with no match, or with more than one match, makes the name
    unusual. Names with exactly one hit in every requested class are omitted.

**-l**
:   Print the effective lookup paths and exit.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# DEFAULT SEARCH ROOTS

- **bin**: `/usr/local/bin`, `/usr/local/sbin`, `/usr/bin`, `/usr/sbin`,
  `/bin`, `/sbin`, plus the directories listed in **PATH** (appended).
- **man**: `/usr/local/man`, `/usr/share/man`, `/usr/man` (each expanded to
  its `man1`..`man9` subdirectories when present), plus `/usr/local/info` and
  `/usr/share/info`.
- **src**: `/usr/local/src`, `/usr/share/src`, `/usr/src`.

# FILE MATCHING

- **binaries**: an executable regular file named exactly *NAME*.
- **sources**: a regular file *NAME*`.c`.
- **manuals**: a file *NAME*`.`*SECTION* for sections 1–9, with an optional
  compression suffix (`.gz`, `.xz`, `.bz2`, `.lzma`); info files *NAME*`.info`
  are also counted.

# EXIT STATUS

`0`
:   All looked-up names were processed (whether or not anything was found).

`2`
:   An option was invalid.

# EXAMPLES

```bash
# Show the binary and manual for ls
modbox whereis ls

# Show only the binary
modbox whereis -b ls

# Search a controlled tree
modbox whereis -b -B /opt/bin -f -m -M /opt/man -f tool

# Print names with missing or duplicated files only
modbox whereis -u ls

# Print the effective lookup paths
modbox whereis -l
```

# NOTES

- The **-g** (glob) mode and the long option forms
  (`--binary`/`--man`/`--info`) of util-linux **whereis** are not supported.
- A *NAME* that contains a slash is labelled and searched by its final path
  component, so `modbox whereis /usr/bin/ls` prints `ls: /usr/bin/ls ...`.
- An unknown option prints a message on standard error and exits `2`.

# SEE ALSO

**modbox-which**(1), **modbox-command**(1), **modbox**(1)
