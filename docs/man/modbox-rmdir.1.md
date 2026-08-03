% MODBOX-RMDIR(1) modbox | User Commands
% modbox project
% 2026-08-03

# NAME

modbox-rmdir - remove empty directories

# SYNOPSIS

**modbox rmdir** [*OPTION*]... *DIRECTORY*...

# DESCRIPTION

Remove the DIRECTORY(ies), if they are empty.

# OPTIONS

`-p`, `--parents`
:   Remove DIRECTORY and its ancestors.  For example, `rmdir -p a/b/c` is equivalent to running `rmdir a/b/c a/b a`.  Non-empty intermediate directories are silently skipped.

`-h`, `--help`
:   Display help and exit.

# NOTES

- Only **empty** directories are removed without `-p`.  A non-empty directory produces an error.
- With `-p`, ancestors are removed from deepest to shallowest.  If an ancestor is non-empty, it is silently skipped and processing continues.
- Unlike `rm`, `rmdir` has no trash mode — removed directories are gone permanently.
- `-p` does not remove the final DIRECTORY itself if it is non-empty; it only attempts to remove ancestors.

# EXAMPLES

```bash
modbox rmdir empty_dir/
modbox rmdir -p a/b/c
modbox rmdir dir1 dir2 dir3
```

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-cat**(1), **modbox-ls**(1), **modbox-rm**(1), **modbox**(1)
