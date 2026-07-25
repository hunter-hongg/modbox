# Spec: wc --json

## Problem Statement

`wc` is one of the most fundamental coreutils commands, used constantly in scripts and pipelines. Like `stat`, `du`, `find`, `ps`, `ls`, and `df`, it currently produces only human-readable, fixed-width text output. There is no `--json` flag, so script authors cannot programmatically access line counts, word counts, byte counts, or character counts without fragile regex parsing. This is the last high-value command from the original JSON output roadmap that has not been implemented.

## Solution

Add a `--json` flag to `wc` that outputs machine-readable JSON containing per-file counts (lines, words, bytes, characters) plus an optional total entry when multiple files are provided. The implementation follows the established pattern used by the other 6 commands that already have `--json`.

## User Stories

1. As a shell scripter, I want `wc --json file.txt` to produce valid JSON with line/word/byte/char counts, so that I can extract specific fields with jq without regex hacks.

2. As a data engineer, I want `wc --json *.log | jq '[.[] | select(.words > 1000)]'` to filter large log files, so that I can quickly identify which files need attention.

3. As a developer, I want `wc --json` to include all four count types (lines, words, bytes, characters) as separate numeric fields, so that my scripts can use whichever metric they need.

4. As a script author, I want `wc --json file1 file2` to output a JSON array with a "total" entry when multiple files are given, so that aggregate counts are accessible programmatically just like the text mode.

5. As a user, I want `wc --json -` to accept stdin and produce JSON with `"name": "-"`, so that piping works consistently in JSON mode.

6. As a power user, I want `wc --json -l file.txt` (with a specific flag like `-l`) to still emit all available count fields in JSON (lines, words, bytes, chars), so that JSON output is always complete and structured regardless of which text-mode flags are combined.

7. As a CI/CD engineer, I want `wc --json` output to be deterministic (stable key ordering, consistent types), so that my pipeline tests are reproducible across runs.

8. As a user, I want error cases (missing file, permission denied) to produce a JSON error entry like `{"error": "...", "path": "..."}`, so that my scripts can handle failures structurally instead of crashing on malformed text.

9. As a script author, I want `wc --json` to document the flag in `--help` output, so that discoverability is consistent with the other `--json` commands.

10. As a user, I want `wc --json` to work with `-c`/`-m`/`-l`/`-w` flag combinations, so that the JSON always includes all four count fields regardless of which text-mode flags were specified.

## Implementation Decisions

### Architecture

- Add `--json` flag to `wc.cpp` using the existing argtable3 pattern (`arg_lit0(NULL, "json", ...)`)
- Include `commands/json_stringifier.hpp` (already exists, already included by 6 other commands)
- Use `bool json_mode = (json_opt->count > 0)` following the same pattern as `du.cpp:361`, `ls.cpp:773`, `ps.cpp:358`
- When `--json` is active, collect all results into a vector first (like `stat.cpp:743-788`), then emit a single JSON array
- Always emit all four count fields (`lines`, `words`, `bytes`, `chars`) in JSON regardless of which `-l`/`-w`/`-c`/`-m` flags are passed — JSON is for programmatic access, so completeness is preferred over flag-parity

### JSON Schema

Each file entry in the JSON array:

```json
{
  "bytes": <int64>,
  "chars": <int64>,
  "lines": <int64>,
  "name": <string>,
  "words": <int64>
}
```

Total entry (when multiple files and `--total` behavior applies):

```json
{
  "bytes": <int64>,
  "chars": <int64>,
  "lines": <int64>,
  "name": "total",
  "words": <int64>
}
```

Error entry:

```json
{
  "error": <string>,
  "name": <string>
}
```

Key ordering: alphabetical (consistent with the existing `--json` spec convention).

### Output format

- JSON array `[...]` wrapping all entries
- Pretty-printed with 2-space indent (consistent with existing `--json` implementations)
- Trailing comma handling: comma after each entry except the last
- Single newline at end of output

### Behavior with existing flags

- `-l`, `-w`, `-c`, `-m` flags: accepted but ignored in JSON mode (all four fields always emitted)
- Combined short flags like `-cwl`: accepted, no effect on JSON output structure
- No file operand (stdin only): output a single-entry array with `"name": ""` (empty string, matching how text mode prints nothing for name)
- `-` as filename (explicit stdin): `"name": "-"`
- Multiple files with total: last entry is `"name": "total"` with summed counts
- Mixed valid/invalid files: valid files produce count entries, invalid files produce error entries in the same array

### Module changes

- **wc.cpp**: add `--json` argtable, parse `json_mode` bool, refactor output loop to collect results then branch on `json_mode` vs text output
- **wc.hpp**: no changes needed (command signature unchanged)
- No new files required — reuses existing `json_stringifier.hpp`

## Testing Decisions

### What to test

Add tests to `tests/test_wc.sh` covering:

1. **Single file, default flags**: `wc --json file.txt` → JSON array with one object containing all four count fields
2. **Single file, specific flags**: `wc --json -l file.txt` → still emits all four fields
3. **Multiple files with total**: `wc --json file1 file2` → JSON array with two file entries + one "total" entry
4. **Stdin**: `echo "hello world" | wc --json` → single entry with `"name": ""`
5. **Explicit stdin dash**: `echo "hello" | wc --json -` → entry with `"name": "-"`
6. **Empty file**: `wc --json empty.txt` → all counts are 0
7. **Missing file**: `wc --json nonexistent.txt` → JSON error entry `{"error": "...", "name": "nonexistent.txt"}`
8. **Mixed valid/invalid**: `wc --json file.txt nonexistent.txt` → array with both a count entry and an error entry
9. **Valid JSON**: pipe output through `python3 -m json.tool` or `jq .` to verify parseability
10. **Key ordering**: verify keys appear in alphabetical order (bytes, chars, lines, name, words)

### Test helpers

Use existing `assert_cmd` and `assert_cmd_pat` from `framework.sh`. For JSON validation, use `jq .` if available, or `python3 -m json.tool` as fallback.

### Seam

- **Highest seam**: modify `wc.cpp` only. No new modules, no new headers, no new interfaces.
- Reuses existing `json_stringifier.hpp` — zero new dependencies.
- Tests go in existing `tests/test_wc.sh` — no new test infrastructure.

## Out of Scope

- `--human-readable` style values in JSON (JSON always uses raw integer counts; human-readable is a display concern)
- Streaming/incremental JSON output (collect-then-emit, consistent with other `--json` implementations)
- `--json` auto-detection based on TTY (the existing implementations do not auto-switch; user must explicitly pass `--json`)
- Character encoding awareness (counts bytes for `-c` and characters for `-m` as the current text mode does; no UTF-8 grapheme cluster support)

## Further Notes

This is the final ticket in the original `--json` roadmap (Ticket 08 — wc --json). After this, all 7 planned high-value commands will have `--json` output: stat, du, find, ps, ls, df, wc. The JSON stringifier utility (Ticket 01) is already in place and proven across 6 commands.

The implementation is straightforward — `wc` is one of the simpler commands at ~136 lines. The main structural change is refactoring `wc_stream` to return counts as a struct rather than printing directly, then branching on `json_mode` for output.
