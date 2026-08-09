% MODBOX-TR(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-tr - translate, squeeze, and delete characters

# SYNOPSIS

**modbox tr** [*OPTION*]... *SET1* [*SET2*]

# DESCRIPTION

Translate, squeeze, or delete characters from standard input, writing to
standard output.

SET1 is the set of characters to act on. SET2 is the replacement set, used
by translation (the default operation) when neither **-d** nor **-s** is
the only operation requested.

# OPTIONS

**-c**, **--complement**
:   Use the complement of SET1 (all characters not in SET1).

**-d**, **--delete**
:   Delete characters in SET1; no translation is performed.

**-s**, **--squeeze-repeats**
:   Replace each input sequence of a repeated character with a single
    occurrence (applied after translation/deletion).

**-t**, **--truncate-set1**
:   First truncate SET1 to the length of SET2 before translating.

**-h**, **--help**
:   Display help and exit.

# SET SYNTAX

SETs are strings of characters. Most characters represent themselves.
Interpreted sequences:

`\NNN`
:   Character with octal value NNN (1 to 3 digits).

`\\`
:   Backslash.

`\a`
:   Alert (BEL).

`\b`
:   Backspace.

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

`CHAR1-CHAR2`
:   All characters from CHAR1 to CHAR2 in ascending order.

`[CHAR*N]`
:   CHAR repeated N times.

`[CHAR*]`
:   CHAR repeated to fill the length of SET1.

`[:class:]`
:   All characters in the named class.

# CHARACTER CLASSES

`alnum`
:   Letters and digits.

`alpha`
:   Letters.

`blank`
:   Horizontal whitespace.

`cntrl`
:   Control characters.

`digit`
:   Digits.

`graph`
:   Printable characters excluding space.

`lower`
:   Lowercase letters.

`print`
:   Printable characters including space.

`punct`
:   Punctuation.

`space`
:   Whitespace.

`upper`
:   Uppercase letters.

`xdigit`
:   Hexadecimal digits.

# EXAMPLES

```bash
# Convert lowercase to uppercase
echo "hello" | modbox tr 'a-z' 'A-Z'

# Delete carriage returns (DOS to Unix)
modbox tr -d '\r' < file.txt > file.unix.txt

# Squeeze repeated spaces to one
echo "a    b" | modbox tr -s ' '

# Complement: keep only digits
echo "ab12cd34" | modbox tr -cd '0-9'

# Translate using a class
echo "Hello World" | modbox tr '[:lower:]' '[:upper:]'
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. SET2 longer than SET1 without **-t**, or
    invalid escape).

# NOTES

- When translating, SET1 and SET2 are paired positionally. Characters in
  SET1 with no corresponding SET2 position are deleted.
- **-c** applies to the entire SET1 specification, including ranges and
  classes.

# SEE ALSO

**modbox-sed**(1), **modbox-cut**(1), **modbox**(1)
