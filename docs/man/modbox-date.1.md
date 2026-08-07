% MODBOX-DATE(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-date - print or set the system date and time

# SYNOPSIS

**modbox date** [*OPTION*]... [+*FORMAT*]

# DESCRIPTION

Display the current time in the given *FORMAT*, or display the time
described by a *STRING* or the modification time of a *FILE*.
With no options and no format, the default human-readable format is
used.

# OPTIONS

**-d**, **--date=STRING**
:   Display the time described by STRING instead of the current time.
    Supported string formats:
    `YYYY-MM-DD HH:MM:SS`, `YYYY-MM-DD`,
    `YYYY/MM/DD HH:MM:SS`, `YYYY/MM/DD`,
    `MM/DD/YYYY HH:MM:SS`, `MM/DD/YYYY`.

**-r**, **--reference=FILE**
:   Display the last modification time of FILE.

**-u**, **--utc**, **--universal**
:   Print or interpret times in Coordinated Universal Time (UTC).

**-R**, **--rfc-email**
:   Output the date and time in RFC 5322 format
    (e.g., `Mon, 07 Aug 2026 12:00:00 +0000`).

**-I**[*TIMESPEC*], **--iso-8601**[=*TIMESPEC*]
:   Output the date/time in ISO 8601 format.
    Optional TIMESPEC controls precision:
    `hours` → `YYYY-MM-DDTHH`,
    `minutes` → `YYYY-MM-DDTHH:MM`,
    `seconds` (default) → `YYYY-MM-DDTHH:MM:SS`,
    `ns` → `YYYY-MM-DDTHH:MM:SS` (nanoseconds not shown).

**-h**, **--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# FORMAT SPECIFIERS

When a *FORMAT* string is supplied (starting with `+`), the following
strftime-style conversion specifiers are supported:

`%Y`
:   Year with century (e.g., 2026).

`%m`
:   Month as a decimal number (01–12).

`%d`
:   Day of the month (01–31).

`%H`
:   Hour in 24-hour format (00–23).

`%M`
:   Minute (00–59).

`%S`
:   Second (00–60, allowing for leap seconds).

`%F`
:   Full date, equivalent to `%Y-%m-%d`.

`%T`
:   Full time, equivalent to `%H:%M:%S`.

`%a`
:   Abbreviated weekday name (locale-dependent).

`%b`
:   Abbreviated month name (locale-dependent).

`%z`
:   UTC offset in the form `±HHMM` (e.g., `+0800`).

`%Z`
:   Time zone name or abbreviation.

`%N`
:   Nanoseconds (always 9 digits, zero-padded).

`%%`
:   Literal `%` character.

Additional specifiers supported by the system `strftime`:
`%c`, `%x`, `%X`, `%j`, `%U`, `%W`, `%w`, `%G`, `%V`, etc.

# EXAMPLES

```bash
# Default human-readable output
modbox date

# Custom format
modbox date "+%Y-%m-%d %H:%M:%S"

# RFC 5322 format
modbox date -R

# ISO 8601 with seconds precision
modbox date -Iseconds

# ISO 8601 with hours precision
modbox date -Ihours

# Show modification time of a file
modbox date -r /etc/passwd "+%Y-%m-%d"

# UTC time
modbox date -u "+%Y-%m-%d %H:%M:%S UTC"

# Nanoseconds
modbox date "+%Y-%m-%d %H:%M:%S.%N"

# Parse a date string
modbox date -d "2026-01-01"
```

# NOTES

- When no format is given and no option specifies a time source,
  the default format is `%a %b %e %H:%M:%S %Z %Y` (local time).
- The `-d` option accepts a limited set of date string formats;
  arbitrary natural language parsing is not supported.
- `%N` (nanoseconds) always outputs 9 digits even if the underlying
  clock resolution is coarser.
- The `--version` option is a modbox extension.

## Differences from GNU date

Not implemented: `--set` (requires root), `--file` (read dates from
file), `--date=STRING` with relative expressions like `next monday`
or `2 hours ago`, `%n` (newline), `%t` (tab), `%s` (seconds since
Epoch), custom locale with `--locale`.

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred (e.g., invalid date string, file not found).

# SEE ALSO

**modbox-cal**(1), **modbox**(1)
