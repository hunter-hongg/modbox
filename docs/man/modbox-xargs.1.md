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

**-I** *REPLACESTR*
:   Replace every command argument equal to REPLACESTR with an input item,
    running COMMAND once per item. (Short form only; the `--replace=`
    spelling shown in `--help` is not yet parsed.)

**-n** *MAX_ARGS*
:   Use at most MAX_ARGS arguments per command line. (Short form only;
    the `--max-args=` spelling shown in `--help` is not yet parsed.)

**-s** *MAX_CHARS*
:   Use at most MAX_CHARS characters per command line. (Short form only;
    the `--max-chars=` spelling shown in `--help` is not yet parsed.)

**-P** *MAX_PROCS*
:   Accept up to MAX_PROCS as the requested process limit. (Short form
    only; the `--max-procs=` spelling shown in `--help` is not yet
    parsed. Note: actual parallel execution is not yet implemented;
    commands currently run sequentially.)

**-t**
:   Read items from standard input (equivalent to treating stdin as a
    single-item source).

**--show-limits**
:   Print the command-line length and argument-count limits and exit.

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

# Show limits and exit
modbox xargs --show-limits
```

# EXIT STATUS

`0`
:   Success (also returned when a command is missing but no items were
    read, or when a recognized error such as a bad option occurs).

`1`
:   COMMAND returned non-zero (propagated from the last invocation).

# NOTES

- By default items are whitespace-delimited and quotes/backslashes are not
  interpreted. Use **-0** for untrusted or arbitrary filenames.
- With **-I**, COMMAND runs once per input item; only arguments that
  exactly match REPLACESTR are replaced.
- Long option spellings (`--replace=`, `--max-args=`, `--max-chars=`,
  `--max-procs=`) are printed by `--help` but are not yet accepted by the
  parser; use the short forms.

# SEE ALSO

**modbox-find**(1), **modbox-awk**(1), **modbox**(1)

