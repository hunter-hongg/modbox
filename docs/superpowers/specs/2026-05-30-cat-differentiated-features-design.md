# Cat Command: Differentiated Features Design

Date: 2026-05-30

## Overview

Add dev-tool and content-navigation features to modbox `cat`, transforming it from a standard GNU cat clone into a terminal code-review tool. Zero new package dependencies.

## New CLI Options

```
Dev tools:
      --blame          Show git blame (commit, author, date) per line
      --highlight      Syntax highlight output by file extension
      --header         Show file metadata banner before content
      --diff=FILE      Unified diff between file and FILE

Content navigation:
      --range=N-M      Show only lines N through M (1-indexed)
      --grep=PATTERN   Keep lines matching extended regex
      --context=N      Show N context lines around --grep matches
      --head=N         Show first N lines only
      --tail=N         Show last N lines only
      --number-format=FMT  Line number format: decimal|hex|octal
      --stats          Show line/word/char count at end
```

## CatOptions Struct Additions

```c
typedef struct {
    // Existing fields
    int show_line_numbers;
    int show_nonempty_line_numbers;
    int show_ends;
    int squeeze_blank;
    int show_tabs;
    int show_nonprinting;
    int less_mode;

    // Dev tools
    int blame_mode;
    int highlight_mode;
    int header_mode;
    char* diff_file;

    // Content navigation
    int range_start;       // 0 = no limit
    int range_end;         // 0 = no limit
    char* grep_pattern;
    int context_lines;
    int head_lines;        // 0 = no limit
    int tail_lines;        // 0 = no limit
    int show_stats;
    int number_format;     // 0=decimal, 1=hex, 2=octal
} CatOptions;
```

## File Layout

```
include/commands/cat.h                  — CatOptions struct
src/commands/cat.c                      — cat_command() + pipeline orchestrator

src/commands/cat/
  helpers.{c,h}          — buffer mode, range/head/tail/grep/context/stats
  blame.{c,h}            — git blame parser (pipe git blame --porcelain)
  highlight.{c,h}        — syntax highlighting engine (pure C, zero deps)
  diff.{c,h}             — diff runner (spawn diff -u)
```

Headers in `include/commands/cat/` mirror this structure.

## Architecture

### Two Processing Modes

| Mode | Trigger | Behavior |
|---|---|---|
| Stream (existing) | No dev/nav options active | Line-by-line via fgets, on-the-fly output |
| Buffer | Any dev/nav option active | Read file into GPtrArray of lines, then pipeline stages |

### Pipeline Stages (buffer mode)

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
[--number-format]  format line number display
     │
     ▼
[--highlight]      ANSI color tokens per language
     │
     ▼
output lines[]
```

### Feature Interaction Rules

| Combo | Behavior |
|---|---|
| --range + --grep | Range filter first, then grep within range |
| --head + --tail | Head applied first; tail after head = no-op |
| --head + --range | Range first, then head |
| --blame + --highlight | Blame column left, highlighted code right |
| --diff | Ignores all other display options, pure diff output |
| --context without --grep | No-op |
| --stats | Counts from final filtered set |

## Blame

### BlameInfo struct

```c
typedef struct {
    char commit[9];      // short SHA (7 chars + null)
    char author[32];
    char date[16];       // YYYY-MM-DD
} BlameInfo;
```

### Implementation

Spawn `git blame --porcelain FILE`, parse per-line output. Porcelain format is stable, machine-parseable. Each blame hunk starts with a commit SHA line, followed by header lines, then the file content prefixed with a tab.

### Output layout

```
abc1234  Alice  2026-05-30  │  #include <stdio.h>
aef5678  Bob    2026-05-29  │  int main(void) {
```

Truncate: commit=7 chars, author=12 chars, date=10 chars. Pipe-safe (no color unless `--color=always`).

## Highlight

### Approach

Pure C keyword-based highlighting. No external parser library.

- File extension → language map
- Language → token types: keywords, strings, comments, numbers, types
- Token type → ANSI color code

### Supported languages

C, Python, Rust, Go, JavaScript, JSON, YAML, TOML, Markdown, Shell.

Approximately 200 lines C.

### Color table

| Token | ANSI Code |
|---|---|
| Keyword | Blue (34) |
| String | Green (32) |
| Comment | Gray (90) |
| Number | Yellow (33) |
| Type | Cyan (36) |
| Preprocessor | Magenta (35) |
| Operator | Default |
| Punctuation | Default |

Auto-disable color when stdout is not a TTY.

## Header

Use `stat()` syscall. Print format:

```
── FILE ──────────────────────────────────────────────
  Mode: -rw-r--r--   Size: 1243 B   Modified: 2026-05-30 10:30:00
──────────────────────────────────────────────────────
```

## Diff

Spawn `diff -u FILE1 FILE2`, capture output, print to stdout. Reuses system `diff`. No inline diff algorithm needed.

## Dependencies

| Feature | Dependency |
|---|---|
| --grep | POSIX regex.h (system) |
| --header | POSIX sys/stat.h (system) |
| --blame | git binary on PATH |
| --diff | diff binary on PATH |
| --highlight | None |

Zero new package dependencies. Zero vcpkg additions.

## Implementation Order

Each step produces a buildable, testable increment:

1. Add new fields to CatOptions, argtable3 options, no-op stubs
2. Buffer mode: read file into GPtrArray when dev/nav option active
3. --range, --head, --tail (slice ops)
4. --grep + --context (POSIX regex.h)
5. --number-format (hex/octal)
6. --header (stat syscall)
7. --blame (git blame --porcelain pipe)
8. --highlight (keyword-based color engine)
9. --stats (line/word/char count)
10. --diff (spawn diff -u)
11. Color refine, pipe detection, edge cases

## Testing

All features testable via `tests/run_tests.sh`:

- --head/--tail/--range: known-length files with assert_cmd
- --grep/--context: pattern match on known content
- --header: pattern match "Mode:", "Size:"
- --number-format: verify hex prefix "0x" or octal prefix "0"
- --stats: pattern match count labels
- --blame: create temporary git repo, commit file, assert SHA pattern
- --diff: two known-different files, assert unified diff markers
- --highlight: pipe output to file, assert no escape codes in file

Tests per implementation step, TDD-style.
