% MODBOX-AWK(1) modbox | User Commands
% modbox project
% 2026-08-04

# NAME

modbox-awk - pattern scanning and processing language

# SYNOPSIS

**modbox awk** [*OPTION*]... **-e** *program* [--] [*file*]...
**modbox awk** [*OPTION*]... **-f** *progfile* [--] [*file*]...

# DESCRIPTION

**awk** is a text-processing language. It scans files for lines matching
one or more patterns, and performs actions on matching lines. When no
**file** is given, or when **file** is `-`, input is read from standard
input.

The program can be supplied as a command-line string (enclosed in quotes
to protect it from the shell), in a program file with **-f**, or with
the **-e** short option.

# OPTIONS

## Program input

`-f` *progfile*
:   Read the awk program from the specified file.

`-e` *program*
:   Read the awk program from the next command-line argument.

Positional argument (first non-option string)
:   Used as the awk program when neither **-f** nor **-e** is given.

## Fields and variables

`-F` *fs*
:   Set the input field separator to the given string (equivalent to
    setting **FS** before the program runs).

`-v` *var*=\ *val*
:   Assign the value **val** to the awk variable **var** before the
    program begins execution.

`-W` *assign*=\ *var*=\ *val*
:   GNU-style variable assignment (equivalent to **-v**).

`-W` *version*
:   Print the GNU Awk version string and exit.

`--`
:   End of options; all remaining arguments are treated as file names
    or the program text.

# BUILT-IN VARIABLES

## Record and field variables

**FS**
:   Input field separator (default: single space).

**OFS**
:   Output field separator (default: single space).

**RS**
:   Input record separator (default: newline).

**ORS**
:   Output record separator (default: newline).

**NF**
:   Number of fields in the current record.

**NR**
:   Number of the current record (global across all files).

**FNR**
:   Number of the current record within the current file.

**FILENAME**
:   Name of the current input file.

**$0**
:   The entire current record.

**$1** ... **$NF**
:   Individual fields (1-indexed). `$0` refers to the whole record.

## Formatting variables

**OFMT**
:   Numeric output format for `print` (default: `"%.6g"`).

**CONVFMT**
:   Numeric-to-string conversion format (default: `"%.6g"`).

**SUBSEP**
:   Subscript separator for multidimensional array indexing
    (default: `\034`).

## Position variables

**RSTART**
:   Start position of the string matched by **match()**.

**RLENGTH**
:   Length of the string matched by **match()** (-1 if no match).

## Argument variables

**ARGC**
:   Number of command-line arguments (excluding options and program).

**ARGV**
:   Array of command-line arguments (0-indexed; `ARGV[0]` is `"awk"`).

# BUILT-IN FUNCTIONS

`length`([*string*])
:   Return the length of *string* (or of `$0` if omitted). If given an
    array, return the number of elements.

`substr`(*s*, *m*[, *n*])
:   Return the substring of **s** starting at position **m** (1-indexed)
    of length **n** (default: rest of string).

`index`(*s*, *t*)
:   Return the position of string **t** within **s** (1-indexed), or 0
    if not found.

`split`(*s*, *a*[, *fs*])
:   Split string **s** into array **a** using **fs** as separator
    (default: **FS**). Returns the number of fields produced.

`tolower`(*string*)
:   Return *string* with all uppercase characters converted to
    lowercase.

`toupper`(*string*)
:   Return *string* with all lowercase characters converted to
    uppercase.

`match`(*s*, *r*)
:   Search **s** for the extended regular expression **r**. Set
    **RSTART** and **RLENGTH** on success. Returns **RSTART** or 0.

`int`(*value*)
:   Truncate *value* to an integer.

`sqrt`(*value*)
:   Return the square root of *value*.

`exp`(*value*)
:   Return the exponential function *e*^*value*.

`log`(*value*)
:   Return the natural logarithm of *value*.

`sin`(*value*), `cos`(*value*)
:   Trigonometric functions (arguments in radians).

`atan2`(*y*, *x*)
:   Arctangent of *y*/*x* in radians.

`rand()`
:   Return a random number *n* such that 0 <= *n* < 1.

`srand`([*value*])
:   Set the seed for **rand()** to *value* (or the current time).
    Returns the previous seed.

`sprintf`(*fmt*, [*expr*...])
:   Format values according to *fmt* (same syntax as **printf**
    conversion specifiers) and return the resulting string.

`sub`(*r*, *s*[, *t*])
:   Replace the first occurrence of the regex **r** with string **s**
    in *t* (or `$0`). Returns the number of replacements.

`gsub`(*r*, *s*[, *t*])
:   Replace all occurrences of the regex **r** with string **s** in *t*
    (or `$0`). Returns the number of replacements.

`system`(*command*)
:   Execute *command* via **sh** and return the exit status.

`close`(*expression*)
:   Close a file or pipe opened by **print**, **printf**, or
    **getline**. Returns 0 on success.

# CONTROL FLOW

```
if (condition) statement [else statement]
while (condition) statement
for (init; cond; incr) statement
for (var in array) statement
break
continue
next
exit [code]
return [expression]
function name(params) { body }
```

# OUTPUT

`print` [*expr*, ...]
:   Print expressions separated by **OFS**, followed by **ORS**.
    With no arguments, prints `$0`.

`printf` *fmt*, [*expr*, ...]
:   Format and print using the given format string.

Output redirection:

```
print ... > "file"
print ... >> "file"
print ... | "command"
```

# OPERATORS

Arithmetic: `+` `-` `*` `/` `%` `^` (increment/decrement: `++` `--`)
Assignment: `=` `+=` `-=` `*=` `/=` `%=` `^=`
Comparison: `==` `!=` `<` `<=` `>` `>=`
Regex match: `~` `!~`
Logical: `&&` `||` `!`
Concatenation: (juxtaposition, e.g. `"a" "b"`)
Ternary: `cond ? yes : no`
Array membership: `key in array`

# EXAMPLES

```bash
# Print the second field of each line
awk '{print $2}' data.txt

# Print lines matching a regex
awk '/error/ {print}' logfile

# Sum the first column
awk '{sum += $1} END {print sum}' data.txt

# Field count and line number
awk '{print NR, NF, $0}' data.txt

# Custom field separator
awk -F: '{print $1, $3}' /etc/passwd

# Variable assignment
awk -v x=10 'BEGIN {print x}'

# Program from file
awk -f prog.awk input.txt

# Case conversion
awk '{print tolower($0)}' text.txt

# Replace commas with dashes
awk '{gsub(/,/, "-"); print}' data.csv

# Associative array
awk '{cnt[$1]++} END {for (k in cnt) print k, cnt[k]}' words.txt

# Range pattern
awk '/start/,/end/' bigfile.txt

# Printf formatting
awk '{printf "%.2f\n", $1 * 2}' numbers.txt
```

# NOTES

- **Modbox `awk`** implements a POSIX-compatible subset of GNU awk with 21 built-in
functions.
- **NOT IMPLEMENTED**: `BEGINFILE`/`ENDFILE` rules, `delete array` (without
  index) on built-in arrays.
- `for (key in array)` iteration order is unspecified.
- Numeric-to-string coercion uses **CONVFMT**; string-to-number coercion is
  automatic in arithmetic context.
- `-W version` prints a GNU Awk compatibility string, not a modbox-specific
  version.

# EXIT STATUS

`0` on success.

# SEE ALSO

**modbox**(1)
