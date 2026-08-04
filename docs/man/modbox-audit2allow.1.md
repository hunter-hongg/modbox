% MODBOX-AUDIT2ALLOW(1) modbox | User Commands
% modbox project
% 2026-08-04

# NAME

modbox-audit2allow - generate SELinux policy rules from audit log records

# SYNOPSIS

**modbox audit2allow** [*OPTIONS*]

# DESCRIPTION

Read audit log records containing AVC (Access Vector Cache) denial
messages and emit SELinux policy rules that would allow the denied
access.

The default behavior reads from standard input, making it easy to pipe
output from tools such as **ausearch** or **dmesg**.

# OPTIONS

## Input sources

`-i` *INPUT*, `--input`=\ *INPUT*
:   Read input from the specified FILE. Mutually exclusive with `-a`,
    `-b`, `-d`, and `-l`.

`-a`, `--all`
:   Read input from the system audit log (requires **libaudit**).
    Not supported in modbox v1 — produces an error.

`-b`, `--boot`
:   Read audit messages since the last boot (requires **libaudit**).
    Not supported in modbox v1 — produces an error.

`-d`, `--dmesg`
:   Read input from **dmesg** output. Not supported in modbox v1.

`-l`, `--lastreload`
:   Read input since the last policy reload (requires **libaudit**).
    Not supported in modbox v1 — produces an error.

## Output options

`-m` *MODULE*, `--module`=\ *MODULE*
:   Wrap output as a loadable SELinux module with a `module MODULE`
    statement and a `require` block.

`-o` *OUTPUT*, `--output`=\ *OUTPUT*
:   Append output to the specified FILE instead of printing to stdout.
    Conflicts with `-M`.

`-r`, `--requires`
:   Include `require` statements for types and classes encountered in
    the input. Can be used with or without `-m`.

`-D`, `--dontaudit`
:   Generate `dontaudit` rules instead of `allow` rules.

`-M` *MODULE*, `--module-package`=\ *MODULE*
:   Generate a loadable module package. Not supported in modbox v1 —
    produces an error.

## Filtering

`-t` *TYPE*, `--type`=\ *TYPE*
:   Only process messages whose type field matches the given REGEX.
    Useful for narrowing to a specific audit message type.

## Explanation modes

`-w`, `--why`
:   Translate AVC denials into human-readable explanations instead of
    generating policy rules.

`-e`, `--explain`
:   Same as `-w` (full explanation, builds on why mode).

`-v`, `--verbose`
:   Explain generated output. Ignored with a warning in modbox v1.

## Other options

`-R`, `--reference`
:   Generate reference policy style output. Not fully supported —
    emits a warning and falls back to traditional output.

`-C`, `--cil`
:   Generate CIL (Common Intermediate Language) output. Not supported
    in modbox v1 — produces an error.

`-N`, `--noreference`
:   Do not generate refpolicy-style output. This is the default.

`-x`, `--xperms`
:   Generate extended permission rules. Ignored with a warning.

`--perm-map`=\ *PERM_MAP*
:   Permission map file. Ignored with a warning.

`--interface-info`=\ *INTERFACE_INFO*
:   Interface info file. Ignored with a warning.

`-h`, `--help`
:   Display help and exit.

`-V`, `--version`
:   Output version information and exit.

# INPUT FORMAT

The parser accepts two input formats:

**Raw audit log lines** containing `avc:  denied`:

```
type=AVC msg=audit(1680000000.000:123): avc:  denied  { read } for
pid=1234 comm="httpd" srcname="index.html" tclass=file
byuser=unconfined_r ruser=unconfined_r host=example.com
salabel=unconfined_u:unconfined_r:unconfined_t:s0
label=unconfined_u:unconfined_r:unconfined_t:s0
```

**ausearch-style output** with structured key=value pairs.

Lines that do not match an AVC denial pattern are silently skipped.

# OUTPUT FORMATS

## Traditional allow rules (default)

```
#============= httpd ==============
allow httpd_t httpd_sys_content_t:file { read };
```

## Module output (`-m MODULE`)

```
module MODULE 1.0;

require {
    type httpd_t;
    type httpd_sys_content_t;
    class file { read };
}

#============= httpd ==============
allow httpd_t httpd_sys_content_t:file { read };
```

## Require-only output (`-r`)

```
require {
    type httpd_t;
    type httpd_sys_content_t;
    class file { read };
}

allow httpd_t httpd_sys_content_t:file { read };
```

## Dontaudit rules (`-D`)

```
#============= httpd ==============
dontaudit httpd_t httpd_sys_content_t:file { read };
```

## Why/explain mode (`-w`)

```
# avc:  denied  { read } for pid=1234 comm="httpd" ...
    # comm=httpd
    # source unconfined_u:unconfined_r:unconfined_t:s0
    # target unconfined_u:unconfined_r:unconfined_t:s0
```

# EXAMPLES

```bash
# Read from a file
modbox audit2allow -i /var/log/audit/audit.log

# Pipe ausearch output
ausearch -m avc | modbox audit2allow

# Generate module output
echo 'AVC: denied { read } for pid=1234 ...' | modbox audit2allow -m mypolicy

# Generate dontaudit rules
ausearch -m avc | modbox audit2allow -D

# Write to a file
modbox audit2allow -o policy.te -i /var/log/audit/audit.log

# Filter by type
modbox audit2allow -t 'AVC' -i /var/log/audit/audit.log

# Show why a denial occurred
ausearch -m avc | modbox audit2allow -w
```

# NOTES

- Rules are deduplicated: identical (source type, target type, class,
  permissions) entries are merged; the process name (`comm`) is used
  as a grouping comment header.
- The default rule type is `allow`. Use `-D` to generate
  `dontaudit` rules.
- On systems where SELinux is disabled, `audit2allow` still parses AVC
  messages and generates rules — it does not require a running policy.
- Unsupported flags (`-a`, `-b`, `-l`, `-M`, `-C`) return a non-zero
  exit code with an error message.
- Warnings are printed to stderr for ignored or partially-supported
  flags (`-R`, `-x`, `--perm-map`, `--interface-info`, `-v`); the
  command continues normally.

# EXIT STATUS

`0` on success.
`1` on error (unrecognized option, conflicting flags, missing input,
unsupported flag in v1).

# SEE ALSO

**modbox**(1)
