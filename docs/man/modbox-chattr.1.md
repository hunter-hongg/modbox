% MODBOX-CHATTR(1) modbox | User Commands
% modbox project
% 2026-08-14

# NAME

modbox-chattr - change file attributes on a Linux file system

# SYNOPSIS

**modbox chattr** [*OPTION*]... *MODE*... *FILE*...

# DESCRIPTION

Change the attributes of each FILE. An attribute changes the behavior of the
Linux kernel with respect to that file.

A MODE begins with `+`, `-`, or `=` and is followed by one or more attribute
letters. Multiple MODE specifications may be given, separated by spaces or by
commas within a single argument.

- **+**: Add the listed attributes to the file's current attribute set.
- **-**: Remove the listed attributes from the file's current attribute set.
- **=**: Set the file's attribute set *exactly* to the listed attributes. Any
  attribute not listed is cleared (the read-only `e` attribute is always
  preserved).

# ATTRIBUTE LETTERS

The following attribute letters are recognized. Each may be used with `+`,
`-`, or `=`.

`a`
:   append only

`A`
:   no atime updates

`c`
:   compressed

`d`
:   no dump

`D`
:   synchronous directory updates

`e`
:   extent format (**read-only**; see NOTES)

`i`
:   immutable

`j`
:   journal data

`s`
:   secure deletion

`S`
:   synchronous updates

`t`
:   no tail-merging

`T`
:   top of directory hierarchy

`u`
:   undeletable

`x`
:   compression hint (deprecated, no-op in modbox)

`X`
:   raw compression hint (deprecated, no-op in modbox)

`Z`
:   compressed-dirty hint (deprecated, no-op in modbox)

Unknown attribute letters produce an error listing the valid set.

# OPTIONS

**-R**, **--recursive**
:   Change files and directories recursively.

**-v**, **--verbose**
:   Output a diagnostic for every file processed.

**-f**, **--suppress**
:   Suppress most error messages.

**--version=***VERSION*
:   Set the file's version/generation number (numeric).

**--preserve-root**
:   Fail to operate recursively on '/'.

**--no-preserve-root**
:   Do not treat '/' specially (the default).

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Make a file immutable (cannot be modified, deleted, or renamed)
modbox chattr +i secret.txt

# Allow only appending to a log file
modbox chattr +a /var/log/app.log

# Remove the immutable flag
modbox chattr -i secret.txt

# Set an exact attribute set (immutable + synchronous updates), clearing others
modbox chattr =is data.db

# Recursively mark a tree as having no dump and no atime updates
modbox chattr -R +dA /srv/archive

# Set the file version/generation number
modbox chattr --version=42 file.bin
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. unknown attribute, attempt to remove the
    read-only `e` attribute, missing operand or mode, or an I/O error such as
    permission denied or a filesystem that does not support the attribute).

# NOTES

- The **e** (extent format) attribute is read-only. It is set by the
  filesystem and cannot be removed; an attempt to do so (`chattr -e`) fails
  with an error. When you use `=` to set an exact attribute set, `e` is always
  retained.
- The **x**, **X**, and **Z** attributes are accepted for compatibility but
  are no-ops in modbox.
- Attribute changes require appropriate permissions on the underlying
  filesystem; not all filesystems support every attribute (unsupported
  requests fail with an I/O error).

# SEE ALSO

**modbox-chmod**(1), **modbox-chown**(1), **modbox-chgrp**(1),
**modbox-chcon**(1), **modbox**(1)
