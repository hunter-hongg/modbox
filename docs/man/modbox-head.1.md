% MODBOX-HEAD(1) modbox | User Commands
% modbox project
% 2026-08-05

# NAME

modbox-head - output the first part of files

# SYNOPSIS

**modbox head** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print the first 10 lines of each FILE to standard output.
With no FILE, or when FILE is `-`, read from standard input.

# OPTIONS

`-n`, `--lines=N`
:   Print the first N lines. Default is 10.
    Use a leading `+` to skip the first N-1 lines and print the rest
    (e.g., `-n +20` prints from line 20 to the end).

`-c`, `--bytes=N`
:   Print the first N bytes.
    Use a negative value (e.g., `-c -100`) to print all but the last N bytes.
    When both `-n` and `-c` are given, `-c` takes precedence.

`-q`, `--quiet`
:   Never print headers showing the names of files being processed.

`-v`, `--verbose`
:   Always print headers showing the names of files being processed.

`-z`, `--zero-terminated`
:   Line delimiter is NUL (`\0`) instead of newline.
    Useful with `find -print0` and `xargs -0`.

`-h`, `--help`
:   Display help and exit.

# EXAMPLES

```bash
modbox head file.txt
modbox head -n 20 file.txt
modbox head -n +20 file.txt
modbox head -c 100 file.txt
modbox head -v file1.txt file2.txt
modbox head -z <(find /path -print0)
echo "hello" | modbox head -n 1
modbox head -c -100 big.log
```

# NOTES

- `-c` takes precedence over `-n` when both are given.
- Short options can be combined: `-vn` is equivalent to `-v -n`.
- With no FILE argument, input is read from standard input.
- A leading `+` in `-n +N` means "skip the first N-1 lines" rather than "print the first N lines".

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-tail**(1), **modbox-sort**(1), **modbox**(1)
