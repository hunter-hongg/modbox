% MODBOX-SLEEP(1) modbox | User Commands
% modbox project
% 2026-08-10

# NAME

modbox-sleep - pause for a specified duration

# SYNOPSIS

**modbox sleep** NUMBER[SUFFIX]...

# DESCRIPTION

Pause execution for the total duration specified by the given
NUMBER arguments. Each argument is an integer or floating-point
number optionally followed by a suffix:

**s**
:   seconds (the default)

**m**
:   minutes

**h**
:   hours

**d**
:   days

When multiple arguments are given, the total sleep time is the sum
of all durations. The command does **not** run each duration
sequentially — all values are added together and a single sleep of
that total is performed.

# OPTIONS

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Sleep for 5 seconds (default suffix)
modbox sleep 5

# Sleep for 2.5 seconds using a floating-point value
modbox sleep 2.5

# Sleep for 3 minutes
modbox sleep 3m

# Sleep for 1 hour and 30 seconds
modbox sleep 1h 30s

# Sum of multiple arguments — sleeps for 6 seconds total
modbox sleep 1 2 3
# → equivalent to 'modbox sleep 6'

# Sleep for half a day
modbox sleep 0.5d
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid duration format or interruption by a signal).

# NOTES

- Numbers may be integer or floating-point (e.g. `0.1`, `1.5`).
- The **--sleep-interval** option (GNU coreutils extension for
  periodic progress output) is not supported.
- Signals such as **SIGINT** or **SIGTERM** interrupt the sleep and
  cause a non-zero exit status.
- The total sleep time is computed as a single floating-point sum;
  very large totals are subject to the limits of the underlying
  platform timer.

# SEE ALSO

**modbox-time**(1), **modbox-timeout**(1), **modbox**(1)
