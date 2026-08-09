% MODBOX-XARGS(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-xargs - build and execute command lines from standard input

# SYNOPSIS

**modbox xargs** [*OPTION*]... *COMMAND* [*INITIAL-ARGS*]...

# DESCRIPTION

Run COMMAND with arguments INITIAL-ARGS, followed by items read from
standard input. Items are normally whitespace-delimited tokens; long lines
are broken across multiple invocations to stay within system limits.

# OPTIONS

**-0**, **--null**
:   Items are separated by NUL characters, not whitespace. Use this when
    input items may contain spaces, quotes, or newlines (e.g. output of
    `find -print0`).

**-I**, **--replace=***REPLACESTR*
:   Replace every occurrence of REPLACESTR in COMMAND (and INITIAL-ARGS)
    with an input item, running COMMAND once per item.

**-n**, **--max-args=***MAX_ARGS*
:   Use at most MAX_ARGS arguments per command line.

**-s**, **--max-chars=***MAX_CHARS*
:   Use at most MAX_CHARS characters per command line.

**-P**, **--max-procs=***MAX_PROCS*
:   Run up to MAX_PROCS processes at once.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Remove all .tmp files found
modbox find . -name '*.tmp' | modbox xargs rm

# Safely handle filenames with spaces (NUL-delimited)
modbox find . -name '*.log' -print0 | modbox xargs -0 rm

# Run one command per item, substituting into the argument list
modbox ls | modbox xargs -I {} cp {} /backup/{}

# Limit arguments per invocation
modbox seq 1 100 | modbox xargs -n 10 echo

# Parallel grep across many files
modbox find . -name '*.c' -print0 | modbox xargs -0 -P 4 grep -l main
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid option, or COMMAND returned non-zero
    and xargs was unable to proceed).

# NOTES

- By default items are whitespace-delimited and quotes/backslashes are not
  interpreted (unlike GNU xargs in some modes). Use **-0** for untrusted
  or arbitrary filenames.
- With **-I**, batching options (**-n**, **-s**) are ignored; COMMAND runs
  once per input item.

# SEE ALSO

**modbox-find**(1), **modbox-sh**(1), **modbox**(1)
