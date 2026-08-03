% MODBOX-MV(1) modbox | User Commands
% modbox project
% 2026-08-03

# NAME

modbox-mv - move (rename) files

# SYNOPSIS

**modbox mv** [*OPTION*]... *SOURCE* *DEST*

**modbox mv** [*OPTION*]... *SOURCE*... *DIRECTORY*

**modbox mv** [*OPTION*]... **-t** *DIRECTORY* *SOURCE*...

# DESCRIPTION

Move (rename) SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.

When moving across filesystems, modbox falls back to a copy-then-remove strategy.

# OPTIONS

`-i`, `--interactive`
:   Prompt before overwrite.

`-n`, `--no-clobber`
:   Do not overwrite an existing file.

`-f`, `--force`
:   Remove existing destination, never prompt.

`-v`, `--verbose`
:   Explain what is being done.

`-u`, `--update`
:   Move only when SOURCE is newer than DEST or when DEST is missing.

`-b`, `--backup`
:   Back up existing destination files by appending `~`.

`-t`, `--target-directory=DIRECTORY`
:   Move all SOURCE(s) into DIRECTORY.

`-T`, `--no-target-directory`
:   Treat DEST as a normal file, even if it appears to be a directory.

`-h`, `--help`
:   Display help and exit.

# NOTES

- **`--no-clobber` overrides `--interactive` and `--force`**: if `-n` is given, `-i` and `-f` are ignored.
- **`--force` overrides `--interactive`**: if both `-f` and `-i` are given, `-f` wins.
- `--backup` renames the existing destination to `DEST~` before overwriting.
- Without `-T`, if DEST is an existing directory, SOURCE(s) are moved *into* it.
- With `-T`, DEST is always treated as a file path; an error occurs if multiple sources are given.
- Cross-filesystem moves use a copy-then-remove fallback (same-filesystem moves use the fast `rename(2)` path).
- Short options can be combined (e.g., `-iv` is equivalent to `-i -v`).

# EXAMPLES

```bash
modbox mv src.txt dst.txt
modbox mv file1.txt file2.txt /tmp/
modbox mv -v src.txt dst.txt
modbox mv -i src.txt dst.txt
modbox mv -f src.txt dst.txt
modbox mv -u src.txt dst.txt
modbox mv -b src.txt dst.txt
modbox mv -t /tmp src1.txt src2.txt
modbox mv -T src.txt existing_dir/
```

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-cat**(1), **modbox-ls**(1), **modbox-rm**(1), **modbox-cp**(1), **modbox**(1)
