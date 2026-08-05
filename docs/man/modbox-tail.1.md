% MODBOX-TAIL(1) modbox | User Commands
% modbox project
% 2026-08-05

# NAME

modbox-tail - output the last part of files

# SYNOPSIS

**modbox tail** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print the last 10 lines of each FILE to standard output.
With no FILE, or when FILE is `-`, read from standard input.

# OPTIONS

`-n`, `--lines=N`
:   Output the last N lines. Default is 10.
    Use a leading `+` to output from line N to the end
    (e.g., `-n +100` prints from line 100 to the end).

`-c`, `--bytes=N`
:   Output the last N bytes.
    Use a negative value (e.g., `-c -100`) to omit the first N bytes.
    When both `-n` and `-c` are given, `-c` takes precedence.

`-f`, `--follow`
:   Append data to the output as the file grows.
    Polls the file every `--sleep-interval` seconds.
    With no file argument (stdin mode), `-f` is a no-op.

`-F`
:   Like `-f`, but reopen the file if it is deleted or renamed
    (useful for log rotation). Implies `-f`.

`-q`, `--quiet`
:   Never print headers showing the names of files being processed.

`-v`, `--verbose`
:   Always print headers showing the names of files being processed.

`-z`, `--zero-terminated`
:   Line delimiter is NUL (`\0`) instead of newline.
    Useful with `find -print0` and `xargs -0`.

`-s`, `--sleep-interval=N`
:   Sleep interval for `-f` and `-F`. Default is 1 second.
    Minimum value is 1.

`-h`, `--help`
:   Display help and exit.

# EXAMPLES

```bash
modbox tail file.txt
modbox tail -n 20 file.txt
modbox tail -n +100 file.txt
modbox tail -c 100 file.txt
modbox tail -f app.log
modbox tail -F app.log
modbox tail -f -s 5 app.log
modbox tail -z <(find /path -print0)
echo "hello" | modbox tail -n 1
modbox tail -c -100 big.log
```

# NOTES

- `-c` takes precedence over `-n` when both are given.
- `-F` implies `-f`.
- Short options can be combined: `-vf` is equivalent to `-v -f`.
- With `-f` and no file argument, input is read once from stdin and the program exits.
- A leading `+` in `-n +N` means "output from line N to the end" rather than "output the last N lines".

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-head**(1), **modbox-sort**(1), **modbox**(1)
