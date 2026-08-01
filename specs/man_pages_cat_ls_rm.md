# Spec: Man Pages for cat, ls, rm Commands

## Problem Statement

modbox currently lacks man pages for its commands. Users expect standard Unix documentation accessible via `man modbox-cat`, `man modbox-ls`, `man modbox-rm` (or similar naming). The project README explicitly lists "man pages" as future work. Without man pages, users cannot access offline documentation through standard system tools, and the project doesn't follow Unix conventions for command-line utilities.

## Solution

Implement man pages for the three core commands: `cat`, `ls`, and `rm`. The man pages will be:

1. **Generated from existing documentation sources** — reuse the detailed option tables and examples from `docs/usage.md` and the in-code `--help` text
2. **Built via Makefile** — add `man` target to generate `.1` roff files and install them to standard location
3. **Named as `modbox-cat.1`, `modbox-ls.1`, `modbox-rm.1`** — prefixed to avoid conflicts with system commands
4. **Installed to `$(PREFIX)/share/man/man1/`** — standard FHS-compliant location

## User Stories

1. As a **modbox user**, I want to run `man modbox-cat` to see the cat command manual, so that I can learn all options without internet access.

2. As a **modbox user**, I want to run `man modbox-ls` to see the ls command manual, so that I can reference display options, sorting, and formatting flags offline.

3. As a **modbox user**, I want to run `man modbox-rm` to see the rm command manual, so that I can understand recursive, force, interactive, and trash options.

4. As a **system administrator**, I want man pages installed to `/usr/local/share/man/man1/` during `make install`, so that they integrate with the system `man` command and `apropos`/`whatis` databases.

5. As a **developer**, I want man pages generated from a single source of truth (markdown), so that documentation stays in sync with code and `--help` output.

6. As a **packager**, I want a `make man` target that builds roff files without installing, so that packaging scripts can stage them appropriately.

7. As a **user**, I want the man pages to document all options shown in `--help`, including modbox-specific extensions (like `--tui`, `--json`, `--trash`), so that the manual is complete.

## Implementation Decisions

### 1. Source Format: Markdown → roff via pandoc
- Author man page content in Markdown (one file per command: `docs/man/modbox-cat.1.md`, etc.)
- Use `pandoc -s -t man` to generate `.1` roff files
- Pandoc is widely available and produces clean roff output
- Markdown source lives in `docs/man/` for easy editing

### 2. Man Page Naming: `modbox-<command>.1`
- Prefix with `modbox-` to avoid conflicts with system `cat(1)`, `ls(1)`, `rm(1)`
- Users access via `man modbox-cat`, `man modbox-ls`, `man modbox-rm`
- Section 1 (user commands) is correct for all three

### 3. Makefile Targets
| Target | Description |
|--------|-------------|
| `man` | Generate all `.1` files from markdown sources to `build/man/` |
| `install-man` | Install generated `.1` files to `$(DESTDIR)$(PREFIX)/share/man/man1/` |
| `uninstall-man` | Remove installed man pages |
| `clean` | Extended to remove `build/man/` |

### 4. Installation Path
- `PREFIX` defaults to `/usr/local` (standard)
- `DESTDIR` supported for staged installs
- Man pages go to `$(DESTDIR)$(PREFIX)/share/man/man1/`
- Compress with `gzip -9` after install (standard practice)

### 5. Content Coverage Per Command

#### modbox-cat.1
- Synopsis: `modbox cat [OPTION]... [FILE]...`
- Standard options: `-b`, `-E`, `-n`, `-s`, `-T`, `-v`, `-e`, `-t`, `-A`, `--less`, `--tui`
- Dev-tool options: `--blame`, `--highlight`, `--header`, `--diff`
- Content navigation: `--range`, `--grep`, `--context`, `--head`, `--tail`, `--number-format`, `--stats`
- Exit status, examples, notes on option combining

#### modbox-ls.1
- Synopsis: `modbox ls [OPTION]... [DIR]...`
- Display options: `-a`, `-A`, `-l`, `--author`, `-b`, `-B`, `-d`, `-C`, `-1`, `-F`, `--colorful`, `--icons`, `--color`
- Sorting: `-r`, `-U`
- Sizing: `--block-size`
- Other: `--tui`, `--json`, `-h`
- Exit status, examples, notes on terminal detection

#### modbox-rm.1
- Synopsis: `modbox rm [OPTION]... FILE...`
- Options: `-d`, `-f`, `-i`, `-r`, `-v`, `--one-file-system`, `--no-preserve-root`, `--preserve-root`, `--trash`, `-h`
- Notes: `-f` overrides `-i`, trash behavior, preserve-root default
- Exit status, examples

### 6. Documentation Source Strategy
- **Primary**: Markdown files in `docs/man/` — authoritative, human-editable
- **Validation**: CI step to verify `--help` output matches man page options (optional future work)
- **Sync**: Manual process initially; developer updates both `--help` strings and markdown when adding options

### 7. Dependencies
- **Build-time**: `pandoc` (for markdown → roff conversion)
- **Runtime**: None (man pages are static files)
- Add `pandoc` to CI/install docs; not required for `make compile` or `make run`

## Testing Decisions

### What Makes a Good Test
- **External behavior only**: Test that `man modbox-cat` displays without error, not internal roff formatting
- **Installation verification**: Verify files exist in correct location with correct permissions after `make install-man`
- **Content sanity**: Grep for key option strings in generated roff to ensure conversion worked
- **No implementation detail tests**: Don't test pandoc internals or roff syntax

### Test Modules
1. **Unit**: `tests/test_man_pages.sh` — new test file
   - `assert_cmd_pat "modbox cat" "modbox cat --help"` (man page contains command name)
   - `assert_cmd_pat "\-n.*number" "man ./build/man/modbox-cat.1 | col -b"` (key option present)
   - Verify all three man pages generate without pandoc errors
   - Verify `make man` produces 3 files in `build/man/`
   - Verify `make install-man DESTDIR=/tmp/stage` places files correctly

2. **Integration**: Existing `run_tests.sh` framework will pick up new `test_man_pages.sh`

### Prior Art
- `tests/test_cat.sh`, `tests/test_ls.sh`, `tests/test_rm.sh` — existing command test patterns
- `tests/framework.sh` — `assert_cmd_pat`, `assert_cmd` helpers
- Makefile already has `compile_commands.json` generation as precedent for generated artifacts

## Out of Scope

- **Man page for `modbox` meta-command** (the multi-call binary entry point) — separate effort
- **Man pages for other 127+ commands** — this spec covers only cat, ls, rm as MVP
- **Automatic sync from `--help` to markdown** — manual maintenance initially; automation can be future work
- **Translation/localization** — English only
- **HTML/PDF output** — only roff/man format
- **`help2man` approach** — rejected; produces lower quality than curated markdown
- **Shell completion files** — separate feature

## Further Notes

- The `docs/usage.md` already has excellent content for cat and ls; rm section needs to be added there as part of this work
- Consider adding a `docs/man/README.md` explaining the markdown→roff workflow for future contributors
- Pandoc's man output uses `.TH` macro with date; set date to build date or release date for reproducibility
- Man page `SEE ALSO` section should reference `modbox(1)` (to be created later) and other related modbox commands
- The `--tui` and `--json` options are modbox-specific extensions; document them clearly as non-POSIX