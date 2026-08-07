% MODBOX-TEE(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-tee - read from standard input and write to standard output and files

# SYNOPSIS

**modbox tee** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Copy standard input to each FILE, and also to standard output.
This is useful for splitting a pipe so that data flows both to a file
and further down the pipeline.

With no FILE, or when only `-` is given, tee behaves like `cat`
(copying stdin to stdout).

# OPTIONS

**-a**, **--append**
:   Append to the given FILEs instead of overwriting them.
    Without this option, each FILE is truncated before writing.

**-i**, **--ignore-interrupts**
:   Ignore interrupt signals (SIGINT, SIGQUIT).  This allows tee to
    continue writing to files even if the upstream process is killed
    with Ctrl+C.

**-p**, **--error-action=MODE**
:   Specify what to do on write errors.  MODE can be:
    `warn` (default: print an error message to stderr),
    `warn-nopipe` (warn unless the error is EPIPE / broken pipe),
    `ignore` (silently ignore write errors).

**-h**, **--help**
:   Display help and exit.

# BEHAVIOUR

- Each FILE is opened in write mode (`"w"`) by default, or append
  mode (`"a"`) when **-a** is used.
- If a FILE argument is `-`, it is treated as standard output again
  (a no-op, since stdout is always included).
- Input is read in chunks and written to all outputs simultaneously.
- If any write fails, a message is sent to stderr and the process
  continues writing to the remaining outputs.

# EXAMPLES

```bash
# Split a log into a file and the screen
modbox some-command | modbox tee /var/log/output.log

# Append to an existing log
modbox some-command | modbox tee -a /var/log/output.log

# Multiple output files
modbox some-command | modbox tee file1.txt file2.txt file3.txt

# Pipe further after tee
modbox some-command | modbox tee process.log | modbox grep "error"

# Ignore interrupts for long-running pipeline
modbox long-task | modbox tee -i output.log | modbox final-process
```

# NOTES

- **tee** does not buffer input; it reads and writes as data arrives.
- The **--ignore-interrupts** option is particularly useful in
  scripts where an intermediate Ctrl+C should not abort file writing.
- If a file cannot be opened, tee prints an error to stderr and
  continues with the remaining files.

## Differences from GNU tee

Not implemented: `--output-errors=MODE` (more granular than
`--error-action`), `-q`/`--quiet`, `--sequential`.

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   A write error occurred (depending on **--error-action**).

# SEE ALSO

**modbox-cat**(1), **modbox-sort**(1), **modbox**(1)
