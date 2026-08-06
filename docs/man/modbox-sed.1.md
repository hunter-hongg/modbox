% MODBOX-SED(1) modbox | User Commands
% modbox project
% 2026-08-06

# NAME

modbox-sed - stream editor for filtering and transforming text

# SYNOPSIS

**modbox sed** [*OPTION*]... {script-only-if-no-other-script} [*FILE*]...

# DESCRIPTION

**sed** is a stream editor. It reads lines from the input, applies commands
from a script to each line (the "pattern space"), and writes the result
to standard output. When no FILE is given, or when FILE is `-`, input
is read from standard input.

By default, sed prints every line after processing. Use **-n** to suppress
automatic output.

# OPTIONS

**-n**, **--quiet**, **--silent**
:   Suppress automatic printing of the pattern space.

**-e SCRIPT**, **--expression=SCRIPT**
:   Add the script to the commands to be executed.
    Up to 200 **-e** expressions may be supplied; they are executed in order.

**-f SCRIPT-FILE**, **--file=SCRIPT-FILE**
:   Add the contents of SCRIPT-FILE to the commands to be executed.
    Multiple **-f** options may be supplied.

**-E**, **-r**, **--regexp-extended**
:   Use extended regular expressions in the script.
    Without this flag, basic regular expression (BRE) syntax is used.

**-s**, **--separate**
:   Consider files as separate rather than as a single continuous stream.

**-i[SUFFIX]**, **--in-place[=SUFFIX]**
:   Edit files in place. A backup copy is made if SUFFIX is supplied;
    otherwise the file is edited without a backup.
    This is a GNU extension not found in POSIX sed.

**-h**, **--help**
:   Display help and exit.

# COMMANDS

**s**/**regexp**/**replacement**/**[flags]**
:   Substitute. Replace the first (or all, with `g`) occurrence of
    **regexp** in the pattern space with **replacement**.
    `&` in the replacement refers to the matched text.
    `\1` through `\9` are backreferences.
    Flags: `g` (global), `p` (print), `w file` (write to file),
    `i` or `I` (case-insensitive), `N` (replace Nth occurrence).

**d**
:   Delete the pattern space and start the next cycle.

**p**
:   Print the pattern space. Use with **-n** to print only selected lines.

**q**
:   Quit immediately; do not process any more input.

**a** **text**
:   Append **text** after the current line.

**i** **text**
:   Insert **text** before the current line.

**c** **text**
:   Replace the current line with **text**.

**=**
:   Print the current line number.

**y**/**src**/**dst**/**
:   Transliterate characters. Characters in **src** are replaced by
    the corresponding character in **dst**.

**n**
:   Print the current pattern space (if not suppressed), then read
    the next line into the pattern space.

**N**
:   Append the next line of input to the pattern space, separated by
    a newline.

**w** **file**
:   Write the current pattern space to **file**.

**r** **file**
:   Read the contents of **file** and append them to the pattern space.

# ADDRESSES

Commands can be restricted to specific lines using addresses:

**number**
:   The line numbered *number* (1-indexed).

**$**
:   The last line of input.

**/regex/**
:   Lines matching the regular expression.
    Append `I` for case-insensitive matching.

**addr1,addr2**
:   A range from *addr1* to *addr2* (inclusive).

**!command**
:   Apply the command to all lines that do NOT match the address.

**first~step**
:   Every *step* lines starting from *first*.

# SCRIPT FORMAT

- Commands can be separated by semicolons (`;`) or newlines.
- Text commands (**a**, **i**, **c**) consume the text that follows them
  on the same line (or the following line if a backslash is used).
- The first positional argument is treated as the script when neither
  **-e** nor **-f** is given.
- Scripts are executed in the order they appear.

## BRE vs ERE

By default, sed uses Basic Regular Expression (BRE) syntax.
With **-E** or **-r**, Extended Regular Expression (ERE) syntax is used.
BRE metacharacters `\( \) \{ \}` are converted to their ERE equivalents
` ( ) { }` internally.

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred.

# NOTES

- In-place editing (**-i**) uses a temporary file internally.
- The **n** and **N** commands have special control-flow semantics;
  they interact with the automatic print cycle.
- Without **-n**, sed prints every line by default. With **-n**, only
  explicitly printed lines (via **p** flag or **p** command) appear.
- When both **-e** and **-f** are used, scripts are concatenated in
  the order given on the command line.
- The `-i` option with no suffix edits the file in place without
  creating a backup.

## Differences from GNU sed

Not implemented: `b` (branch), `t` (test), `z` (print with NUL),
`G` (append newline + next line), `h/H` (hold space), `g/G`
(get/replace from hold space), `x` (exchange), labels (`:label`),
`c\` with multiline text, `w` with `-a` append mode.

# EXAMPLES

```bash
# Replace all occurrences of "foo" with "bar"
modbox sed 's/foo/bar/g' file.txt

# Print lines 2 through 5 only
modbox sed -n '2,5p' file.txt

# Edit a file in place with backup
modbox sed -i.bak 's/old/new/g' config.txt

# Use extended regex
modbox sed -E 's/[0-9]+/NUM/' data.txt

# Read from stdin
echo "hello" | modbox sed 's/h/H/'

# Multiple expressions
modbox sed -e 's/a/A/' -e 's/b/B/' file.txt

# Delete empty lines
modbox sed '/^$/d' file.txt

# Print line numbers
modbox sed '=' file.txt

# Transliterate lowercase to uppercase
modbox sed 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' file.txt
```

# SEE ALSO

**modbox-grep**(1), **modbox-awk**(1), **modbox-find**(1), **modbox**(1)
