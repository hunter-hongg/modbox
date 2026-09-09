# modbox-watch.1.md

## Name

watch — execute a program periodically, showing output fullscreen

## Synopsis

```
watch [options] command [arg...]
```

## Description

Runs *command* repeatedly, capturing its standard output and displaying it
after a header line. By default the command runs every 2.0 seconds and is
invoked as `sh -c "command arg..."` (arguments joined with single spaces);
with `-x`/`--exec` the command is executed directly instead.

The header shows the interval, the command line, and the hostname and date
(`Every 2.0s: df -h`), right-aligned against the terminal width. `-t`
suppresses the header.

When stdout is a terminal the screen is cleared before every frame and the
cursor is restored on exit. When stdout is not a terminal, no ANSI control
sequences are emitted (a deliberate deviation from GNU watch) so piped
output stays clean.

## Options

- `-b`, `--beep`: beep (BEL) if the command has a non-zero exit.
- `-d`, `--differences[=permanent]`: highlight lines that changed since the
  previous update. `permanent` is accepted; both forms highlight per frame.
  Highlighting is applied only when stdout is a terminal.
- `-e`, `--errexit`: exit if the command has a non-zero exit, propagating the
  command's exit status.
- `-g`, `--chgexit`: exit when the output of the command changes relative to
  the previous run. The first run establishes the baseline.
- `-n`, `--interval <secs>`: seconds to wait between updates (default 2.0).
  Accepts decimal values and `s`/`m`/`h`/`d` suffixes.
- `-t`, `--no-title`: turn off the header.
- `-x`, `--exec`: pass the command to `execvp` directly instead of
  `sh -c`.
- `-h`, `--help`: display help and exit.
- `-v`, `--version`: output version information and exit.

## Exit status

- 0 on clean termination or a `-g` change trigger.
- The command's exit status when `-e` is active and the command fails.
- 2 on usage errors (missing command, invalid option, invalid interval).
- 127 if the command could not be executed (`-x` mode, or `/bin/sh` missing).
- 1 on internal failures (fork/pipe errors).

## Examples

```
watch -n 1 df -h
watch -d ls -la
watch -g -n 0.5 cat /tmp/flag
watch -e false
watch -x git -C /srv status --short
```

## See also

modbox-sleep(1), modbox-timeout(1), modbox-top(1), modbox(1)
