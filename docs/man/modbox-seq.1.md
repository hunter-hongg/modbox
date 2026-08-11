% MODBOX-SEQ(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-seq - print a sequence of numbers

# SYNOPSIS

**modbox seq** [*OPTION*]... LAST
**modbox seq** [*OPTION*]... FIRST LAST
**modbox seq** [*OPTION*]... FIRST INCREMENT LAST

# DESCRIPTION

Print numbers from FIRST to LAST, in steps of INCREMENT.
If FIRST or INCREMENT is omitted, they default to 1.
Output is written to standard output, one number per line.

# OPTIONS

**-f**, **--format=FORMAT**
:   Use printf-style floating-point FORMAT for output. Default is `%g`.
    Supported specifiers: `%e`, `%f`, `%g`, `%宽.精度` (e.g. `%05.2f`).

**-s**, **--separator=STRING**
:   Use STRING to separate numbers instead of newline.

**-w**, **--equal-width**
:   Pad all numbers with leading zeros to equal width (width of LAST).

**-h**, **--help**
:   Display help and exit.

# FORMAT SPECIFIERS

When **-f** is used, the FORMAT follows the same rules as printf(3):

`%d`
:   Signed integer decimal.

`%f`
:   Fixed-point decimal notation.

`%e`
:   Scientific notation.

`%g`
:   Shorter of `%f` and `%e`.

`%05d`
:   Zero-padded to at least 5 digits.

`%4.2f`
:   Fixed-point with 2 decimal places, total width at least 4.

# EXAMPLES

```bash
# Print 1 to 5
modbox seq 5

# Print 3 to 7
modbox seq 3 7

# Print with custom step
modbox seq 0 2 10

# Zero-padded sequence
modbox seq -w 001 005

# Floating-point with format
modbox seq -f "%.2f" 0 0.5 2

# Pipe to xargs
modbox seq 5 | xargs -I{} echo "item-{}"

# Shell loop
for i in $(modbox seq 3); do echo "round $i"; done
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- FIRST and INCREMENT default to 1 when omitted.
- **-w** pads with leading zeros based on the width of LAST, not FIRST.
- The **--format** option uses the same specifiers as printf; only
  floating-point and integer formats are supported (no strings).
- INCREMENT can be negative to count downward.

# SEE ALSO

**modbox-bc**(1), **modbox-printf**(1), **modbox-xargs**(1), **modbox**(1)
