% MODBOX-CP(1) modbox | User Commands
% modbox project
% 2026-08-03

# NAME

modbox-cp - copy files and directories

# SYNOPSIS

**modbox cp** [*OPTION*]... *SOURCE*... *DEST*

**modbox cp** [*OPTION*]... **-t** *DIRECTORY* *SOURCE*...

# DESCRIPTION

Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.

# OPTIONS

`-r`, `--recursive`
:   Copy directories recursively.

`-v`, `--verbose`
:   Explain what is being done.

`-f`, `--force`
:   Remove existing destination file(s).

`-n`, `--no-clobber`
:   Do not overwrite an existing file.

`-i`, `--interactive`
:   Prompt before overwrite.

`-u`, `--update`
:   Copy only when the SOURCE file is newer than the DEST file or when the DEST file is missing.

`-p`, `--preserve`
:   Preserve mode, ownership, and timestamps.

`-t`, `--target-directory=DIRECTORY`
:   Copy all sources into DIRECTORY.

`-h`, `--help`
:   Display help and exit.

# NOTES

- **`--no-clobber` overrides `--force` and `--interactive`**: if `-n` is given, `-f` and `-i` are ignored.
- **`--force` overrides `--interactive`**: if both `-f` and `-i` are given, `-f` wins.
- Without `-r`, only regular files are copied; directories require `-r`.
- Short options can be combined (e.g., `-rv` is equivalent to `-r -v`).
- When copying into an existing directory, the source basename is used (e.g., `cp a b/` copies `a` as `b/a`).
- The `--update` option skips the copy if the destination already exists and is newer than or equal to the source.

# EXAMPLES

```bash
modbox cp src.txt dst.txt
modbox cp -r src_dir/ dst_dir/
modbox cp -v src.txt dst.txt
modbox cp -p src.txt dst.txt
modbox cp -u src.txt dst.txt
modbox cp -i src.txt dst.txt
modbox cp -n src.txt dst.txt
modbox cp -rf src_dir/ dst_dir/
modbox cp -t /tmp dst1.txt dst2.txt
modbox cp --preserve --verbose a.txt b.txt
```

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-cat**(1), **modbox-ls**(1), **modbox-rm**(1), **modbox-mv**(1), **modbox**(1)
