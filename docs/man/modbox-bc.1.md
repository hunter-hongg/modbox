% MODBOX-BC(1) modbox | User Commands
% modbox project
% 2026-09-11

# NAME

modbox-bc - arbitrary-precision calculator language

# SYNOPSIS

**modbox bc** [*OPTION*]... [*FILE*]...

# DESCRIPTION

**bc** is an arbitrary-precision calculator language. It reads statements from
the given *FILE* arguments, or from standard input when no files are named or
when a file is `-`, evaluates them, and prints the result of each expression.

Arithmetic is performed on integers and decimals of arbitrary size (not limited
to 64 bits). Division precision is controlled by the global `scale` variable,
which defaults to `0` (integer division).

The language supports:

- Integer and floating-point arithmetic with a configurable `scale`
- The `+`, `-`, `*`, `/`, `%` and `^` (integer exponentiation) operators
- Parentheses, variables, and arrays
- The `ibase` and `obase` special variables for input/output base conversion
- Built-in math functions (`s`, `c`, `a`, `e`, `sqrt`) when `-l` is given
- User-defined functions via `define`
- `if`/`else`, `while`, `for`, `break`, `return`, and `halt` statements
- Comments beginning with `#` or `//`

# OPTIONS

**-q**, **--quiet**
:   Suppress the welcome banner so output is clean for piping.

**-l**, **--mathlib**
:   Load the standard math library and set `scale` to 20. This makes the
    functions `s` (sine), `c` (cosine), `a` (arctangent), `e` (exponential),
    and `sqrt` available.

**-s**, **--standard**
:   Treat the input as strict POSIX `bc` and ignore insignificant whitespace.

**-w**, **--warn**
:   Warn about the use of GNU extensions / POSIX incompatibilities.

**-i**, **--interactive**
:   Force interactive mode.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Basic arithmetic
modbox bc <<< "2 + 3"

# Floating-point precision via scale
modbox bc <<< "scale=10; 10/3"

# Base conversion
modbox bc <<< "obase=16; 255"

# Math library functions
modbox bc -l <<< "sqrt(2)"

# Comments and user-defined functions
modbox bc <<< "define f(x) { return (x*x) }; f(5)"
```

# EXIT STATUS

`0`
:   A valid expression was evaluated and produced output.

`1`
:   No output was produced, or a syntax/runtime error occurred.

# SEE ALSO

**modbox-expr**(1), **modbox-factor**(1), **modbox**(1)
