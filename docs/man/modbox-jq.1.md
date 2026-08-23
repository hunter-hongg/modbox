% MODBOX-JQ(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-jq - process JSON inputs using jq-like filters

# SYNOPSIS

**modbox jq** [*OPTIONS*] [*filter*] [*file*]...
**modbox jq** [*OPTIONS*] **-f** *file* [*file*]...

# DESCRIPTION

Process JSON inputs using a small jq-compatible filter language. Reads from files or standard input and writes filtered results to standard output.

# OPTIONS

**-r**, **--raw-output**
:   Output raw strings instead of JSON strings.

**-c**, **--compact-output**
:   Compact output, single line per value.

**-s**, **--slurp**
:   Read all inputs into an array before filtering.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# FILTERS

Simple dot path: `.name`, `.a.b[0]`

Functions: `length`, `keys`, `has`, `type`

Operators: `==`, `!=`, `<`, `>`, `<=`, `>=`, `and`, `or`, `not`

Pipeline: `|`

Selector: `select(expr)`

# EXAMPLES

```bash
modbox jq '.name' file.json
modbox jq -r '.[] | select(.size>100)' data.json
modbox df --json | modbox jq '.[].mount_point'
echo '{"a":1}' | modbox jq '.a'
```

# EXIT STATUS

0 on success, non-zero on error.

# SEE ALSO

**modbox**(1)
