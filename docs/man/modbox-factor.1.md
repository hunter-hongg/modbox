% MODBOX-FACTOR(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-factor - print prime factors

# SYNOPSIS

**modbox factor** [*OPTION*]... [*NUMBER*]...

# DESCRIPTION

Print the prime factors of each specified integer NUMBER. If no NUMBER is given, read them from standard input. Supports large integers via Pollard Rho factorization.

# OPTIONS

**-h**, **--exponents**
:   Print repeated factors in the form p^e unless e==1.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
modbox factor 123456
modbox factor 100 1000 1234567891011
modbox factor --exponents 64
echo 42 | modbox factor
```

# NOTES

- Input numbers must be non-negative integers.
- Factorization of very large numbers may take time.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-cksum**(1), **modbox**(1)
