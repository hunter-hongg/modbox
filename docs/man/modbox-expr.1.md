% MODBOX-EXPR(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-expr - evaluate expressions

# SYNOPSIS

**modbox expr** [*OPTION*]... *EXPRESSION*
**modbox expr** *OPTION*

# DESCRIPTION

Print the value of EXPRESSION to standard output. EXPRESSION may be arithmetic, relational, string comparison, or match operation.

# OPTIONS

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# OPERATORS

Precedence from lowest to highest:

**ARG1 | ARG2**
:   ARG1 if neither null nor 0, else ARG2.

**ARG1 & ARG2**
:   ARG1 if neither argument is null or 0, else 0.

**ARG1 < ARG2**, **ARG1 <= ARG2**, **ARG1 = ARG2**, **ARG1 != ARG2**, **ARG1 >= ARG2**, **ARG1 > ARG2**
:   Relational comparisons.

**ARG1 + ARG2**, **ARG1 - ARG2**
:   Arithmetic addition/subtraction.

**ARG1 * ARG2**, **ARG1 / ARG2**, **ARG1 % ARG2**
:   Arithmetic multiplication/division/modulo.

**STRING : REGEXP**
:   Anchored pattern match.

**match STRING REGEXP**
:   Same as `STRING : REGEXP`.

**substr STRING POS LENGTH**
:   Substring.

**index STRING CHARS**
:   Index of first occurrence, or 0.

**length STRING**
:   Length of STRING.

# EXAMPLES

```bash
modbox expr 1 + 2
modbox expr 5 > 3
modbox expr match 'hello' 'h.*o'
modbox expr substr 'abcdef' 2 3
modbox expr length 'abc'
```

# EXIT STATUS

`0` if EXPRESSION is neither null nor 0, `1` if EXPRESSION is null or 0, `2` if EXPRESSION is invalid.

# SEE ALSO

**modbox-test**(1), **modbox-bc**(1), **modbox**(1)
