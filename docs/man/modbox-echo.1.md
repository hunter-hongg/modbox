% MODBOX-ECHO(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-echo - display a line of text

# SYNOPSIS

**modbox echo** [*SHORT-OPTION*]... [*STRING*]...

# DESCRIPTION

Echo the STRING(s) to standard output, separated by spaces and terminated
by a newline (unless **-n** is given).

# OPTIONS

**-n**
:   Do not output the trailing newline.

**-e**
:   Enable interpretation of backslash escapes in the strings.

**-E**
:   Disable interpretation of backslash escapes (the default).

# ESCAPE SEQUENCES (with -e)

When **-e** is used, the following backslash escapes are interpreted:

`\\`
:   Backslash.

`\a`
:   Alert (BEL).

`\b`
:   Backspace.

`\c`
:   Suppress further output.

`\e`
:   Escape character.

`\f`
:   Form feed.

`\n`
:   New line.

`\r`
:   Carriage return.

`\t`
:   Horizontal tab.

`\v`
:   Vertical tab.

`\0NNN`
:   Byte with octal value NNN.

`\xHH`
:   Byte with hexadecimal value HH.

# EXAMPLES

```bash
# Print a simple message
modbox echo "Hello, world"

# Print without a trailing newline
modbox echo -n "Prompt: "

# Interpret escapes
modbox echo -e "Line1\nLine2\tTabbed"

# Print a bell and a newline
modbox echo -e "\a"
```

# EXIT STATUS

`0`
:   Success.

# NOTES

- This implementation supports **short options only** (`-n`, `-e`, `-E`).
  Long options such as `--help` are NOT recognized; `modbox echo --help`
  will print "--help" as a literal string rather than showing usage.
- By default backslash escapes are **not** interpreted (equivalent to
  `-E`). Use `-e` to enable them.

# SEE ALSO

**modbox-printf**(1), **modbox-printenv**(1), **modbox**(1)
