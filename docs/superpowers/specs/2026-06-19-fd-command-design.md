# fd Command — Design Spec

## Overview

Add an `fd` command to modbox that implements the file-search interface of
[fd-find](https://github.com/sharkdp/fd), the popular Rust-based file finder.
Follows the same implementation pattern as `rg.c` (ripgrep reimplementation).

## CLI Interface

```
fd [OPTIONS] PATTERN [PATH]
```

Mirrors fd-find's argument order: pattern first, optional path (defaults to `.`).

## Options

| Short | Long | Default | Description |
|-------|------|---------|-------------|
| `-H` | `--hidden` | off | Search hidden files/directories |
| `-I` | `--no-ignore` | off | Don't respect .gitignore (stub — no real .gitignore parsing since we never ignore by default) |
| `-s` | `--case-sensitive` | off | Force case-sensitive matching |
| `-i` | `--ignore-case` | off | Force case-insensitive matching |
| | `--smart-case` | on | Case-insensitive if pattern is all-lowercase |
| `-g` | `--glob` | off | Use glob matching instead of regex |
| `-p` | `--full-path` | off | Search full path instead of basename only |
| `-L` | `--follow` | off | Follow symbolic links |
| `-d` | `--max-depth` | unlimited | Maximum recursion depth |
| `-t` | `--type` | any | Filter by type: `f` file, `d` dir, `l` symlink, `x` executable, `e` empty, `s` socket |
| `-e` | `--extension` | — | Filter by file extension (repeatable, e.g. `-e rs -e c`) |
| `-E` | `--exclude` | — | Exclude files matching glob pattern (repeatable) |
| `-0` | `--print0` | off | Separate results by NUL character |
| | `--max-results` | unlimited | Stop after N results |
| `-x` | `--exec` | — | Execute command for each matching file (`{}` replaced) |
| `-X` | `--exec-batch` | — | Execute command once with all matching files |
| | `--color` | auto | When to use colors: `never`, `auto` (default), `always` |
| `-h` | `--help` | — | Show help and exit |

## Architecture

### Files

| File | Action | Description |
|------|--------|-------------|
| `include/commands/fd.h` | Create | FdOptions struct, fd_color_t enum, fd_command() decl |
| `src/commands/fd.c` | Create | Full implementation: arg parsing, dir walk, pattern matching, output, exec |
| `src/main.c` | Edit | Register "fd" -> fd_command in hash table |
| `tests/run_tests.sh` | Edit | Add fd test section |

### Data structures

```c
typedef enum {
    FD_COLOR_NEVER,
    FD_COLOR_ALWAYS,
    FD_COLOR_AUTO
} fd_color_t;

typedef struct {
    int hidden;              // -H
    int no_ignore;           // -I (no-op today)
    int case_sensitive;      // -s
    int ignore_case;         // -i
    int smart_case;          // --smart-case (default: on)
    int glob_mode;           // -g
    int full_path;           // -p
    int follow;              // -L
    int print0;              // -0
    int max_depth;           // -d (-1 = unlimited)
    int max_results;         // --max-results (0 = unlimited)
    fd_color_t color_mode;   // --color
    char type_filter;        // -t (f, d, l, x, e, s, or 0 = any)
    gchar *pattern;          // search pattern
    GPtrArray *extensions;   // -e filters
    GPtrArray *exclude;      // -E globs
    int has_exec;            // -x
    int exec_batch;          // -X
    GPtrArray *exec_args;    // exec command args
    GPtrArray *exec_paths;   // accumulated paths for -X batch mode
} FdOptions;
```

### Implementation flow

1. **Arg parsing** — argtable3, same setup as `rg.c`. Pattern is the first
   positional arg (or the second if a PATH is given). Uses `arg_filen` for
   both PATTERN (min 1) and PATH (min 0).

2. **Directory walk** — `fd_walk()` is recursive via `opendir`/`readdir`.
   Skips `.` and `..`. Skips hidden entries unless `-H` given. Skips entries
   matching `-E` excludes. Recurses into all directories encountered
   (regardless of whether they match the pattern), up to `-d` max depth.

3. **Pattern matching** — Two modes:
   - **Regex (default):** Uses `GRegex` compiled with appropriate flags
     (caseless if smart-case decides so).
   - **Glob (`-g`):** Uses `GPatternSpec`.
   - Target is basename only by default, full path with `-p`.

4. **Output** — One result per line (or NUL-delimited with `-0`). Color
   coding when `--color=always` or `--color=auto` + TTY: directories in
   blue (`\033[01;34m`), symlinks in cyan (`\033[01;36m`), executables in
   green (`\033[01;32m`), regular files in default.

5. **Exec (`-x` / `-X`)** — Same fork/exec pattern as `find.c`. Per-file
   exec forks for each match, batched exec accumulates paths and runs once.
   Uses `execvp` with `{}` substitution.

### Filtering chain

For each directory entry, filters are applied in order:

1. Skip `.` and `..`
2. Skip hidden if `!opts->hidden`
3. Skip if matches `-E` exclude pattern (checks both basename and full path)
4. If not a directory, check `-t` type filter
5. If `-t e` (empty), check file is empty
6. Check `-e` extension filter (skipped for directories)
7. Check pattern match (regex or glob)
8. If matches, add to results count and print (or exec)

### Edge Cases

- **No pattern given** — exit with error (exit code 2)
- **Pattern matches nothing** — exit code 1 (same as rg/grep)
- **Hidden dirs without `-H`** — skipped entirely (not recursed into)
- **Symlink loops** — not detected; shallow directories only (standard
  limitation of recursive walk)
- **Binary files** — not filtered; printed as-is (fd-find also doesn't
  filter binaries by default)
- **`-x` with `{}` not in args** — appends matching path as last arg

### .gitignore note

Real fd parses `.gitignore`. We skip this — `--no-ignore`/`-I` is a no-op
since we don't ignore anything by default. The help text notes this.

### Testing

Test section follows the same helper pattern (`assert_cmd`, `assert_cmd_pat`,
etc.). Tests cover:

- Basic pattern search (regex)
- Glob mode (`-g`)
- Case-sensitive (`-s`) and case-insensitive (`-i`)
- Smart-case default behavior
- Hidden files (`-H`)
- File type filter (`-t f`, `-t d`, `-t l`, `-t x`, `-t e`)
- Extension filter (`-e`)
- Exclude pattern (`-E`)
- Max depth (`-d`)
- Print0 (`-0`)
- Full path search (`-p`)
- Max results (`--max-results`)
- Color modes (`--color=always`, `--color=never`)
- Follow symlinks (`-L`)
- Exec (`-x`, `-X`)
- Multiple paths
- No match exit code
- Error: no pattern
- Help output
