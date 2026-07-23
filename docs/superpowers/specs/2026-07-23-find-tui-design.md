# Feature Spec: find --tui

## Problem Statement

`find` currently only produces flat text output (one path per line via `-print`). When searching through large result sets, users cannot visually scan, filter, or preview files without piping to a pager or other tool. The modbox roadmap calls for "busybox for the 2025 terminal" — every command should support rich, navigable, optionally-interactive output. `ps --tui` and `cat --tui` already demonstrate this pattern. `find` is the next command in the planned TUI sequence that still lacks it.

## Solution

Add a `--tui` flag to `find` that launches an ftxui-based interactive viewer when stdout is a terminal. The viewer displays matched files as a navigable list with file metadata previews, search/filter, and common action shortcuts (delete, open in cat). When piped (non-TTY), it falls back to the existing plain-text behavior so scripts and pipelines are unaffected.

## User Stories

1. As a developer searching for config files in a large project, I want `find . -name '*.conf' --tui` to show me an interactive list of matches, so that I can scan results without losing them to a pager scroll.
2. As a user who pipes `find` output, I want `find ... --tui` to silently fall back to plain `-print` output, so that my existing shell pipelines continue to work.
3. As a user with many find results, I want to press `/` to filter results by pattern inside the TUI, so that I can narrow down matches without re-running the search.
4. As a user navigating a long result list, I want j/k keys to scroll and Enter to select, so that I can browse results ergonomically.
5. As a user who wants to act on a result, I want a `d` key to delete the selected file and a `o` key to open it in the cat TUI, so that common post-find operations are one keystroke away.
6. As a user who wants to see file details before acting, I want a preview pane showing file type, size, modification time, and owner, so that I can confirm the selected entry is the right one.
7. As a user running find with predicates like `-type f -name '*.log'`, I want the TUI to show the matching type icon and colored file type prefix, so that I can visually distinguish files, directories, and symlinks.
8. As a keyboard-driven user, I want `q` to quit the TUI and return to the shell, so that I can exit cleanly without killing the terminal.
9. As a user on a small terminal, I want the TUI layout to adapt to terminal height, so that I always see the maximum usable information.
10. As a user running `find` without `--tui`, I want zero behavioral change, so that my scripts and aliases are unaffected.
11. As a user who wants to see file contents before opening, I want to press `p` to preview the first N lines of the selected file in the detail pane, so that I can quickly inspect without leaving the TUI.
12. As a user with a multi-line selection, I want `n`/`N` to jump to the next/previous match of the current search query, so that I can hop between relevant results.
13. As a user who wants to see how many results matched, I want a status bar showing total count and current position, so that I understand the scope of the search result set.

## Implementation Decisions

- **Two entry points, one walk engine.** The existing `find_walk` and `find_evaluate` functions are reused unchanged. A new `find_collect_matches` function drives the same walk but populates a `std::vector<FindMatch>` instead of calling `find_exec_file`. This keeps the non-TUI code path completely untouched.
- **`FindMatch` result struct.** A new lightweight struct holds `path`, `display_name`, `file_type` (from the existing `classify()`), `size`, `mtime`, `mtime_str`, and `perm_str`. This mirrors the existing `TuiEntry` pattern used by `ls --tui`. Used solely in `find_tui_main` to feed the list renderer; not persisted beyond the TUI session.
- **`TuiBase` inheritance.** The find TUI component extends the existing `TuiBase` class (which already provides `selected_`, `scroll_offset_`, `search_mode_`, `handle_nav`, `handle_search`, `render_list`, `render_search_bar`). This gives consistent j/k navigation, `/` search, and n/N next-prev behavior across all TUI commands without duplicating logic.
- **TTY guard.** In `find_command`, when `--tui` is present and `isatty(STDOUT_FILENO)` is true, launch `find_tui_main`. When stdout is piped, ignore `--tui` and fall through to the existing `-print` path. This matches the pattern used by `cat --tui`, `cat --less`, and `ps --tui`.
- **Argtable3 `--tui` flag.** A new `arg_lit0(NULL, "tui", "interactive TUI viewer for search results")` is added to `find_command`'s argtable. It is a no-op in non-TTY contexts. Help text is updated.
- **Keymap.** Borrowed from existing TUIs where applicable:
  - `q` — quit
  - `j`/`k` — scroll down/up (from `TuiBase::handle_nav`)
  - `/` — enter search/filter mode (from `TuiBase::handle_search`)
  - `n`/`N` — next/prev search match (from `TuiBase`)
  - `d` — delete selected file (calls `unlink`/`rmdir`, mirrors `find_exec_file`)
  - `o` — open selected file in cat TUI (if stdout is still a TTY)
  - `p` — preview: show first 50 lines of selected file in detail pane
  - The Tab key behavior from `cat --tui` is NOT carried over (find has a different UX model — single-pane list + detail, not multi-tab).
- **Single-pane layout with detail footer.** Unlike `ls --tui` (two-pane) or `cat --tui` (tabbed), `find --tui` uses a single scrollable list for results plus a footer showing the selected entry's metadata. This matches the "navigable results" model from the roadmap.
- **No piping of TUI output.** All ftxui rendering is confined to `find_tui_main`, which only runs when stdout is a TTY. Tests never need to parse ftxui escape sequences.
- **No new dependencies.** Uses the existing ftxui dependency (already used by `ls_tui`, `ps_tui`, `cat_tui`). No external link dependencies added.

## Testing Decisions

- **Shell-based integration tests only** — consistent with the entire codebase. All existing find tests are in `tests/test_find.sh` using `assert_cmd`, `assert_cmd_pat`, `assert_cmd_not_pat`, `assert_cmd_pat_stderr`. New tests follow the same framework.
- **TTY guard tests.** When `find --tui` is invoked with stdout piped (non-TTY), it must produce identical output to `find -print` for the same arguments. This is tested by piping `find --tui ...` through `cat` and comparing with `find ...` (no `--tui`).
- **Existing predicate tests remain valid.** The `find -print` path is untouched; all 24 `test_find.sh` cases continue to pass without modification.
- **New TUI-specific tests:**
  - `--tui help` test: `find --help` output includes `--tui`
  - `--tui non-TTY fallback`: `find --tui -maxdepth 1 -name 'a.txt'` piped produces same output as without `--tui`
  - `--tui + -maxdepth + -type`: combined predicates still work in TUI mode (verified via fallback path)
- **What is NOT tested:** ftxui UI rendering, keyboard event handling, or search highlighting inside the TUI. These would require terminal automation (screen recorder / expect-style tools) which is not used anywhere in the existing test suite. The TUI rendering logic is exercised indirectly through the TTY guard (it confirms the entry point is reachable and does not crash).

## Out of Scope

- No `--json` output flag (separate roadmap item)
- No tree view for directory results (the `ls --tui` tree view is already implemented; find TUI is flat list for now)
- No `-exec` enabled interactively from inside the TUI (too dangerous; the user can copy the path and run the command manually)
- No syntax highlighting inside find TUI (not applicable — find outputs paths, not file contents)
- No bookmarking or favorites (separate roadmap item)
- Unit tests for `find_collect_matches` as a standalone function — it is a thin wrapper around the same code already covered by integration tests

## Further Notes

- The roadmap sequence (MemPalace diary entry 2026-07-22) was: `ps` (done) → `cat` (done) → `find` (this). Completing `find --tui` advances the project to having three TUI-enhanced commands.
- The `cat --tui` preview pane adapter (`cat_tui.cpp:159-177`) was a prototype that informed this spec's decision to use a single-pane footer detail view rather than a full two-pane browser. Find results are heterogeneous (different file types, sizes, no guaranteed content to show), so a lightweight metadata footer is more appropriate.
- The `p` preview key reads up to the first 50 lines of the selected file using `fopen`/`fgets` into the detail pane. This reuses the existing `classify()` and `format_time()` helpers from `ls.hpp` and `stat.cpp`. No syntax highlighting is applied in the preview pane — that is the job of `cat --tui` which the `o` key launches.
