# Spec: Man Pages for cp, mv, rmdir Commands

## Problem Statement

modbox already has man pages for `cat`, `ls`, and `rm` (generated from markdown in `docs/man/` and tested in `tests/test_man_pages.sh`), but lacks man pages for three other core filesystem commands: `cp`, `mv`, and `rmdir`. Users running `man modbox-cp`, `man modbox-mv`, or `man modbox-rmdir` will find nothing, breaking the expectation of parity across commands and leaving a gap in offline documentation.

## Solution

Add markdown-sourced man pages for `cp`, `mv`, and `rmdir` following the same pattern as the existing `modbox-cat.1.md`, `modbox-ls.1.md`, and `modbox-rm.1.md`. The Makefile and test infrastructure already support this; only the source markdown files and test assertions need to be added.

## User Stories

1. As a **modbox user**, I want to run `man modbox-cp` to see the copy command manual, so that I can learn all recursive, preserve, backup, and update options without internet access.
2. As a **modbox user**, I want to run `man modbox-mv` to see the move/rename command manual, so that I can understand the difference between `rename()` and cross-filesystem copy-fallback behavior.
3. As a **modbox user**, I want to run `man modbox-rmdir` to see the directory removal manual, so that I can use the `-p` parents flag correctly.
4. As a **system administrator**, I want man pages for all core filesystem commands (`cp`, `mv`, `rmdir`) in addition to the already-documented ones (`cat`, `ls`, `rm`), so that the manual coverage feels complete.
5. As a **developer**, I want the new man pages to follow the same markdown→roff pipeline (pandoc, Makefile targets) as the existing ones, so that maintenance burden stays low.
6. As a **packager**, I want `make install-man` to install the three new `.gz` files to `$(PREFIX)/share/man/man1/` alongside the existing three, so that `apropos` and `man -k` return complete results.
7. As a **user**, I want the man pages to document modbox-specific extensions such as `--backup`, `--update`, `--no-target-directory`, and `--parents`, so that the manual is a complete reference.
8. As a **tester**, I want the existing `tests/test_man_pages.sh` to verify the three new man pages alongside the existing ones, so that regressions in generation or install are caught.

## Implementation Decisions

### 1. Source Files: Three New Markdown Files

| File | Command |
|------|---------|
| `docs/man/modbox-cp.1.md` | `modbox cp` |
| `docs/man/modbox-mv.1.md` | `modbox mv` |
| `docs/man/modbox-rmdir.1.md` | `modbox rmdir` |

These follow the same structure as the existing `docs/man/modbox-cat.1.md`, `docs/man/modbox-ls.1.md`, and `docs/man/modbox-rm.1.md`.

### 2. Man Page Content — `modbox-cp.1.md`

- **Synopsis**: `modbox cp [OPTION]... SOURCE... DEST` (with `-t` variant)
- **Options**:
  - `-r`, `--recursive` — copy directories recursively
  - `-v`, `--verbose` — explain what is being done
  - `-f`, `--force` — remove existing destination file
  - `-n`, `--no-clobber` — do not overwrite existing files
  - `-i`, `--interactive` — prompt before overwrite
  - `-u`, `--update` — copy only when source is newer than destination
  - `-p`, `--preserve` — preserve mode, ownership, timestamps
  - `-t`, `--target-directory=DIRECTORY` — copy all sources into DIRECTORY
  - `-h`, `--help` — display help and exit
- **Notes**: `-n` overrides `-f` and `-i`; short options can be combined
- **Examples**: basic copy, recursive, preserve, verbose, target-directory, update
- **Exit status**: `0` on success, `1` on error

### 3. Man Page Content — `modbox-mv.1.md`

- **Synopsis**: `modbox mv [OPTION]... SOURCE DEST` (with `-t` and multi-source variants)
- **Options**:
  - `-i`, `--interactive` — prompt before overwrite
  - `-n`, `--no-clobber` — do not overwrite existing files
  - `-f`, `--force` — remove existing destination, never prompt
  - `-v`, `--verbose` — explain what is being done
  - `-u`, `--update` — move only when SOURCE is newer than DEST
  - `-b`, `--backup` — back up existing destination files (append `~`)
  - `-t`, `--target-directory=DIRECTORY` — move all sources into DIRECTORY
  - `-T`, `--no-target-directory` — treat DEST as a normal file, not a directory
  - `-h`, `--help` — display help and exit
- **Notes**: `-n` overrides `-i` and `-f`; `-f` overrides `-i`; cross-filesystem fallback uses copy+remove
- **Examples**: rename, move into directory, backup, verbose, update, target-directory, no-target-directory
- **Exit status**: `0` on success, `1` on error

### 4. Man Page Content — `modbox-rmdir.1.md`

- **Synopsis**: `modbox rmdir [OPTION]... DIRECTORY...`
- **Options**:
  - `-p`, `--parents` — remove DIRECTORY and its ancestors (e.g., `rmdir -p a/b/c` is like `rmdir a/b/c a/b a`)
  - `-h`, `--help` — display help and exit
- **Notes**: only empty directories are removed (unless `-p` is used, which silently skips non-empty intermediate dirs); does not remove non-directories
- **Examples**: remove single empty dir, remove with parents, chained parents
- **Exit status**: `0` on success, `1` on error

### 5. Makefile Extension

Add the three new sources to `MAN_SOURCES` in the Makefile:

```
MAN_SOURCES := $(MAN_SRC_DIR)/modbox-cat.1.md \
               $(MAN_SRC_DIR)/modbox-ls.1.md \
               $(MAN_SRC_DIR)/modbox-rm.1.md \
               $(MAN_SRC_DIR)/modbox-cp.1.md \
               $(MAN_SRC_DIR)/modbox-mv.1.md \
               $(MAN_SRC_DIR)/modbox-rmdir.1.md
```

No other Makefile changes are needed — the wildcard patterns for `MAN_PAGES`, `MAN_INSTALLED`, and the `man`/`install-man`/`uninstall-man` targets already iterate over `MAN_SOURCES`.

### 6. Test Extension

Extend `tests/test_man_pages.sh` to:
- Assert existence of the three new `.md` source files
- Assert that `make man` generates `build/man/modbox-cp.1`, `build/man/modbox-mv.1`, `build/man/modbox-rmdir.1`
- Assert content coverage (grep for key option strings in each generated man page)
- Assert installation to the correct `DESTDIR` path for all six man pages
- Assert gzip compression for all six installed files

## Testing Decisions

### What Makes a Good Test
- **External behavior only**: Test that `make man` produces the expected `.1` files and that `man` can render them, not internal pandoc behavior
- **File existence**: Verify source `.md` files and generated `.1` files exist
- **Content sanity**: Grep for key option names (e.g., `--recursive` in cp, `--backup` in mv, `--parents` in rmdir) in the rendered output
- **Install verification**: Verify all six `.gz` files land in the correct `DESTDIR` path
- **No implementation detail tests**: Don't test roff macro syntax or pandoc flags

### Test Modules
1. **Unit**: Extend `tests/test_man_pages.sh` — add assertions for the three new commands
2. **Integration**: The existing `tests/run_tests.sh` framework picks up the extended test automatically

### Prior Art
- `tests/test_man_pages.sh` — existing test file for cat/ls/rm man pages; extend it, don't create a new file
- `tests/test_cp.sh`, `tests/test_mv.sh`, `tests/test_rmdir.sh` — existing command tests for behavioral reference
- Makefile `MAN_SOURCES` variable — the single point of truth for which man pages exist

## Out of Scope

- **Man page for the `modbox` meta-command** — separate effort (noted in the existing spec)
- **Man pages for other commands** (e.g., `mkdir`, `touch`, `ln`) — this spec covers only `cp`, `mv`, `rmdir` as the next batch
- **Automatic sync from `--help` to markdown** — manual maintenance; the existing process
- **HTML/PDF output formats** — only roff/man format
- **Shell completion files** — separate feature
- **Reformatting existing man pages** — the cat/ls/rm pages are already complete; this spec does not touch them

## Further Notes

- The `--help` output in each command's source file is the ground truth for option names and descriptions. The markdown should match it closely.
- `SEE ALSO` sections should reference the other modbox commands (e.g., `modbox-cp(1)`, `modbox-mv(1)`, `modbox-rmdir(1)`, `modbox-rm(1)`, `modbox(1)`).
- The date in the `%` header line should use the build date (pandoc's `\%d %B %Y` will handle this automatically when regenerated).
- The existing `docs/man/modbox-rm.1.md` already documents `--trash`; the new rmdir page should note that `rmdir` has no trash equivalent (it permanently removes empty dirs).
- `modbox-rmdir`'s `-p` behavior silently succeeds on non-empty intermediate directories (matching GNU coreutils); this should be documented in a NOTE.
