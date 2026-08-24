% MODBOX-GETFACL(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-getfacl - display Access Control Lists

# SYNOPSIS

**modbox getfacl** [*OPTION*]... *FILE*...

# DESCRIPTION

Display Access Control Lists (ACLs) for each FILE.

Without options, prints both access and default ACLs with a comment header.

# OPTIONS

## ACL selection

`-a`, `--access`
:   Display access ACL only.

`-d`, `--default`
:   Display default ACL only.

`-e`, `--all-effective`
:   Print all effective rights comments.

`-E`, `--no-effective`
:   Print no effective rights comments.

## Formatting

`-t`, `--tabular`
:   Use tabular output format.

`-n`, `--numeric`
:   Print numeric user and group identifiers.

`-c`, `--omit-header`
:   Omit leading file name and ownership lines.

`-p`, `--absolute-names`
:   Do not strip leading `/` from path names.

## Operation

`-R`, `--recursive`
:   Process directories recursively.

`-L`, `--logical`
:   Logical walk; follow symbolic links.

`-P`, `--physical`
:   Physical walk; do not follow symbolic links (default).

`--one-file-system`
:   Stay within one file system.

`-s`, `--skip-base`
:   Skip files that only have base entries.

## Help

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox getfacl file.txt
modbox getfacl -R -n /var/www
modbox getfacl -d -c dir1
modbox getfacl --tabular file.txt | awk -F'\t' '{print $1}'
```

# NOTES

* Output format follows GNU `getfacl`.
* Requires libacl.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-setfacl**(1), **modbox-chmod**(1), **modbox**(1)
