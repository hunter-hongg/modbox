# modbox `cat` Command

Concatenate files and print on standard output. A full POSIX-style `cat` with additional dev-tool and content-navigation features for terminal code review.

## Synopsis

```
cat [OPTION]... [FILE]...
```

With no `FILE` or when `FILE` is `-`, read standard input.

## Standard Options

| Option | Description |
|--------|-------------|
| `-n`, `--number` | Number all output lines |
| `-b`, `--number-nonblank` | Number nonempty output lines only |
| `-E`, `--show-ends` | Display `$` at end of each line |
| `-T`, `--show-tabs` | Display TAB characters as `^I` |
| `-s`, `--squeeze-blank` | Suppress repeated empty output lines |
| `-v`, `--show-nonprinting` | Use `^` and `M-` notation for non-printing characters (except LFD and TAB) |
| `-A`, `--show-all` | Equivalent to `-vET` |
| `-e` | Equivalent to `-vE` |
| `-t` | Equivalent to `-vT` |

### Behavior

`-n` and `-b` are mutually exclusive; `-b` takes precedence. Line numbers are printed right-aligned in a 6-character field followed by two spaces. Files are concatenated in argument order.

The `-s` flag suppresses consecutive blank lines down to at most one. When combined with `-n` or `-b`, line numbers reset per file — new files restart at 1.

## Dev Tools

| Option | Description |
|--------|-------------|
| `--blame` | Show git blame (commit, author, date) per line |
| `--highlight` | Syntax highlight output by file extension |
| `--header` | Show file metadata banner (mode, size, mtime) before content |
| `--diff=FILE` | Unified diff between input file and FILE |

### `--blame`

Annotate each line with its git blame information. Spawns `git blame --porcelain <FILE>` and parses the porcelain output. Each line is prefixed with:

```
abc1234  Alice  2026-05-30  │  #include <stdio.h>
aef5678  Bob    2026-05-29  │  int main(void) {
```

Columns are truncated to: commit = 7 chars, author = 12 chars, date = 10 chars. Does not color the blame prefix (pipe-safe).

Only works in git repositories. Returns nothing if the file is not tracked by git.

### `--highlight`

Apply keyword-based syntax highlighting to output. Uses ANSI escape codes (colors). Auto-disabled when stdout is not a TTY.

| Token | ANSI Color |
|-------|------------|
| Keyword | Blue (34) |
| String | Green (32) |
| Comment | Gray (90) |
| Number | Yellow (33) |
| Type | Cyan (36) |
| Preprocessor | Magenta (35) |

Supported languages and file extensions:

| Language | Extensions |
|----------|-----------|
| C | `.c`, `.h` |
| Python | `.py` |
| Rust | `.rs` |
| Go | `.go` |
| JavaScript/TypeScript | `.js`, `.ts` |
| JSON | `.json` |
| YAML | `.yaml`, `.yml` |
| TOML | `.toml` |
| Markdown | `.md` |
| Shell | `.sh`, `.bash` |

The highlighting engine is pure C — no external parser dependency. It tokenizes each line using keyword lists and pattern matching.

### `--header`

Print a metadata banner before each file's content using `stat()`:

```
-- Makefile ──────────────────────────────────────────────
  Mode: -rw-r--r--   Size: 2.3 KiB   Modified: 2026-05-30 10:30:00
──────────────────────────────────────────────────────────
```

Shows file mode (rwx-style), human-readable size (B/KiB/MiB), and last modification timestamp.

### `--diff=FILE`

Compute a unified diff between the input file and the specified FILE. Spawns system `diff -u` and captures output. Ignores all other display options — pure diff output.

```
$ cat main.c --diff=main.c.bak
```

## Content Navigation

| Option | Description |
|--------|-------------|
| `--range=N-M` | Show only lines N through M (1-indexed) |
| `--grep=PATTERN` | Keep lines matching extended regular expression |
| `--context=N` | Show N context lines around `--grep` matches |
| `--head=N` | Show first N lines only |
| `--tail=N` | Show last N lines only |
| `--number-format=FMT` | Line number format: `decimal` (default), `hex`, `octal` |
| `--stats` | Show line/word/char count at end |

### `--range=N-M`

Slice lines by 1-indexed range. Both N and M are inclusive. Omit M to slice to end:

```
$ cat file.txt --range=10-20    # lines 10 through 20
$ cat file.txt --range=50-      # lines 50 to end
```

### `--grep=PATTERN`

Filter lines matching an extended regular expression (POSIX `regex.h`). Works on the buffer after `--range`, `--head`, `--tail` have been applied:

```
$ cat file.txt --grep='error\|warning'
$ cat file.txt --range=50-100 --grep='FIXME'
```

### `--context=N`

Show N lines of context around `--grep` matches. Adjacent context ranges are merged. No-op without `--grep`.

```
$ cat file.txt --grep='TODO' --context=2
```

### `--head=N`

Show first N lines. Applied after `--range` if both are specified.

### `--tail=N`

Show last N lines. Applied after `--head` if both are specified.

### `--number-format=FMT`

Controls line number formatting when `-n` or `-b` is active:

| Format | Example |
|--------|---------|
| `decimal` | `     1  ` |
| `hex` | `0x0001  ` |
| `octal` | `000001  ` |

### `--stats`

Print a summary line after all content showing line, word, and character counts for the (possibly filtered) output:

```
  42 lines  187 words  1024 characters
```

## Pager Mode

| Option | Description |
|--------|-------------|
| `--less` | Pager mode with j/k/q navigation |

When `--less` is passed and stdout is a TTY, output is captured into a buffer and presented in an interactive pager. Supports standard vi-style navigation (j = down, k = up, q = quit). Useful when browsing long files.

```
$ cat longfile.c --less
```

## Architecture

### Two Processing Modes

| Mode | Trigger | Behavior |
|------|---------|----------|
| Stream | No dev/nav options active | Line-by-line via `fgets`, on-the-fly output |
| Buffer | Any dev/nav option active | Read file into `GPtrArray` of lines, then pipeline stages |

When no advanced option is active, `cat` processes files in streaming mode — reading each line and writing it immediately. This is memory-efficient for large files.

When any dev-tool or content-navigation option (or `--less`) is active, the entire file (or stdin) is read into memory as `PipelineLine` entries, then processed through the pipeline:

```
read entire file → lines[]
     │
     ▼
[--header]    print metadata banner (stat: mode, size, mtime)
     │
     ▼
[--blame]     spawn "git blame --porcelain FILE", parse per-line BlameInfo
     │
     ▼
[--diff]      compute unified diff, print, done
     │
     ▼
[--squeeze]   collapse consecutive blank lines
     │
     ▼
[--range]     slice lines[start-1 : end]
     │
     ▼
[--head]      slice lines[0 : N]
     │
     ▼
[--tail]      slice lines[-N : ]
     │
     ▼
[--grep]      regex filter lines, collect match indices
     │
     ▼
[--context]   expand indices → adjacent lines
     │
     ▼
[--stats]     count lines/words/chars from filtered set
     │
     ▼
output lines[]
```

### Feature Interaction Rules

| Combo | Behavior |
|-------|----------|
| `--range` + `--grep` | Range filter first, then grep within range |
| `--head` + `--tail` | Head applied first; tail after head |
| `--head` + `--range` | Range first, then head |
| `--blame` + `--highlight` | Blame column left, highlighted code right |
| `--diff` | Ignores all other display options |
| `--context` without `--grep` | No-op |
| `--stats` | Counts from final filtered set |

## Internal Data Structures

### `CatOptions` (include/commands/cat.h)

All configuration options packed into a single struct, passed as `const CatOptions*` through the pipeline:

```c
typedef struct {
    int show_line_numbers;
    int show_nonempty_line_numbers;
    int show_ends;
    int squeeze_blank;
    int show_tabs;
    int show_nonprinting;
    int less_mode;

    int blame_mode;
    int highlight_mode;
    int header_mode;
    char* diff_file;

    int range_start;
    int range_end;
    char* grep_pattern;
    int context_lines;
    int head_lines;
    int tail_lines;
    int show_stats;
    int number_format;  // 0=decimal, 1=hex, 2=octal
} CatOptions;
```

### `PipelineLine` (include/commands/cat/helpers.h)

Each line in the buffer pipeline:

```c
typedef struct {
    char* text;
    int orig_index;     // original line index (used for --blame mapping)
} PipelineLine;
```

### `BlameInfo` (include/commands/cat/blame.h)

Per-line blame data parsed from `git blame --porcelain`:

```c
typedef struct {
    char commit[9];     // short SHA (7 chars + null)
    char author[32];
    char date[16];      // YYYY-MM-DD
} BlameInfo;
```

## File Layout

```
include/commands/cat.h                  — CatOptions struct, cat_command() entry
include/commands/cat/
  helpers.h                             — PipelineLine, buffer ops, stats, header, formatting
  blame.h                               — BlameInfo, parse_blame, free_blame
  highlight.h                           — get_file_extension, print_highlighted
  diff.h                                — run_diff

src/commands/cat.c                      — cat_command() + pipeline orchestrator + expand_short_options
src/commands/cat/
  helpers.c                             — buffer mode: read/read_stdin, slice_range/head/tail,
                                          grep/context, stats, header banner, visual output
  blame.c                               — git blame --porcelain parser
  highlight.c                           — keyword-based syntax highlighting engine
  diff.c                                — spawn diff -u, capture output
```

## Short Option Expansion

The `cat` command supports combined short options (e.g., `-vTE` instead of `-v -T -E`). The `expand_short_options()` function splits combined flags into individual `-X` tokens before argtable3 parsing. This runs before any argument parsing and is transparent to the rest of the pipeline.

## Dependencies

| Feature | Dependency |
|---------|------------|
| Argument parsing | argtable3 (via vcpkg) |
| Data structures | glib-2.0 (GPtrArray, GArray, GString) |
| grep | POSIX `regex.h` (system) |
| header | POSIX `sys/stat.h` (system) |
| blame | `git` binary on PATH |
| diff | `diff` binary on PATH |
| highlight | None (pure C) |

Zero new package dependencies beyond what modbox already uses.

## Examples

### Basic Usage

```bash
# Concatenate files
$ cat file1.txt file2.txt

# Read from stdin
$ cat < file.txt

# Use dash for stdin explicitly
$ echo "hello" | cat -
```

### Standard Options

```bash
# Number all lines
$ cat -n file.txt

# Number non-blank lines
$ cat -b file.txt

# Show line endings
$ cat -E file.txt

# Show tabs
$ cat -T file.txt

# Squeeze blank lines
$ cat -s file.txt

# Show all non-printing characters
$ cat -A file.txt

# Combined options
$ cat -vTE file.txt
```

### Content Navigation

```bash
# Show lines 10-20
$ cat file.txt --range=10-20

# Show lines 50 to end
$ cat file.txt --range=50-

# Show first 10 lines
$ cat file.txt --head=10

# Show last 5 lines
$ cat file.txt --tail=5

# Grep for matches
$ cat file.txt --grep='TODO|FIXME'

# Grep with context
$ cat file.txt --grep='function' --context=3

# Combined: range then grep
$ cat file.txt --range=100-200 --grep='error'

# Combined: range then head
$ cat file.txt --range=10-50 --head=5

# Hex line numbers
$ cat -n --number-format=hex file.txt

# Stats
$ cat --stats file.txt
```

### Dev Tools

```bash
# Git blame per line
$ cat main.c --blame

# Syntax highlight
$ cat main.c --highlight

# Header banner
$ cat main.c --header

# Diff against another version
$ cat main.c --diff=main.c.bak

# Combined: header + blame + highlight
$ cat main.c --header --blame --highlight
```

### Pager

```bash
# Interactive pager
$ cat longfile.c --less
```

## Tests

The cat command is tested through `tests/run_tests.sh`. Tests cover:

- Standard output concatenation (single file, multiple files, stdin)
- All standard options: `-n`, `-b`, `-E`, `-T`, `-s`, `-v`, `-A`, `-e`, `-t`
- Option combinations: `-vTE`, `-nb`, `-n -s`, `-vT`
- Edge cases: empty files, DEL character (0x7f), high bytes (0x80–0xff)
- Content navigation: `--head`, `--tail`, `--range` (with open-ended ranges)
- Grep and context: `--grep`, `--grep --context`
- Number formats: `--number-format=hex`, `--number-format=octal`
- Header: `--header` with Mode/Size/Modified patterns
- Stats: `--stats` output pattern
- Diff: `--diff` with unified diff markers
- Blame: `--blame` inside a temporary git repository
- Combined features: `--range --head`, `--range --grep`
- Error handling: nonexistent files (stderr message)
- Pager: `--less` mode
