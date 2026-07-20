# `ls --tui` / `lf`: interactive file browser design

**Date:** 2026-07-20  
**Author:** opencode  
**Status:** approved

## Context

modbox has implemented ~95 coreutils commands. The next phase is differentiation (angle #2: Rich TUI everything). `ls`/`lsc` are the most-run commands and already have colorful/icons foundations. Adding an interactive, navigable TUI to `ls` is the highest-impact first move — it demonstrates the "rich + navigable" spine and sets the pattern for other commands.

`mtop.cpp` already uses **ftxui**, so no new dependency is introduced.

## Goals

- `ls --tui` opens an interactive, two-pane file browser.
- `lf` is a thin alias wrapper (`lsc` pattern) that calls `ls --tui`.
- On quit, the browser changes the user's actual CWD (ranger/lf style) via a shell wrapper.
- Plain `ls` output is completely unchanged — backward compatible.

## Files

- **New:** `src/commands/ls_tui.hpp` — TUI options struct.
- **New:** `src/commands/ls_tui.cpp` — ftxui browser app (entry collection, rendering, event loop).
- **New:** `src/commands/lf.cpp` — alias wrapper, injects `--tui` and delegates to `ls_command`.
- **Refactor:** `src/commands/ls.cpp` — extract entry-collection logic (readdir + filters + lstat) into a shared helper `collect_entries()` (static in `ls.hpp` or a small `ls_common.cpp`). `ls_command` and `ls_tui` both use it. No behavior change.
- **New:** `modbox lf init <shell>` subcommand — installs a shell function that captures stdout from `modbox lf --tui` and `cd`s to it. Mirrors `prompts init` pattern.
- **New:** test cases in `tests/run_tests.sh`.

## TUI layout

Two-pane `Container::Horizontal`:
- **Left:** file list (`Menu`) showing icon + name + classify indicator, with current path header and count. Colors/icons reuse `ls.cpp` logic.
- **Right:** preview pane (smart-by-type, see §Preview).
- **Bottom:** status bar with keybind hints + selected file size/type.

## Keybindings

| Key | Action |
|-----|--------|
| `Up` / `Down` / `j` / `k` | Move selection |
| `Enter` | If dir: cd into it (re-collect, reset selection to top). If file: no special action. |
| `/` | Fuzzy filter input — narrows list live. |
| `o` | Open selected in `$EDITOR` → `$PAGER` → `cat` via `system()`; TUI suspends and returns after. |
| `c` | Copy absolute path to clipboard (`xclip`/`pbcopy`/OSC 52). Status-bar toast on success. |
| `d` | Delete selected. Shows confirm dialog, then `unlink` (file) or `rmdir` (dir). |
| `q` | Quit. Writes final CWD to `$XDG_RUNTIME_DIR/modbox-lf-cwd` (fallback `/tmp/modbox-lf-cwd`). |

## Preview pane (smart-by-type)

On selection change, stat/lstat the entry and render:
- **Directory** → child listing (first ~N entries, mini `ls -1`). Show `.`/`..` nav hint.
- **Text / regular file** → contents preview with syntax highlighting (reuse `cat`'s highlighter if separable; fallback plain text). First ~200 lines, scrollable.
- **Symlink** → `readlink` target + arrow; preview target's type.
- **Binary / device / socket / fifo** → metadata block: type, size, perms, owner, mtime, inode.
- **Empty file** → "(empty)".

Truncated previews show "[… truncated]". Rendered via ftxui `Paragraph`.

## CWD-on-quit

Flow:
1. TUI prints last directory to stdout when user hits `q`.
2. `modbox lf` is normally invoked through a shell function, not bare binary. The function runs:
   ```bash
   __dir=$(command modbox lf --tui "$@" 2>/dev/null)
   if [ -n "$__dir" ] && [ "$__dir" != "$(pwd)" ]; then
       cd "$__dir"
   fi
   ```
3. `modbox lf init <shell>` prints this function.

Implementation detail: `ls_tui_command` prints the final CWD to stdout after the screen loop exits. No temp file or hidden flag needed. The shell wrapper captures stdout and `cd`s there.

## Non-interactive safety

When `ls --tui` detects stdout is not a TTY (piped, redirected), it must **fall back to plain `ls` output** (no ftxui screen initialization). This preserves script/CI compatibility.

## Error handling

- Unreadable directory → error entry in list (grayed), not crash.
- Filter with no matches → "no matches" placeholder state.
- `o` with missing editor → fallback chain (`$EDITOR` → `$PAGER` → `cat`), status-bar error if none available.
- `d` on permission-denied → status-bar error, no crash.
- Terminal too small → minimum 2 columns enforced; if too small, show error and quit gracefully.

## Testing

- **Regression:** all existing `ls`/`lsc` tests still pass after collection refactor.
- **TTY guard:** `ls --tui` with stdout piped produces plain `ls` output (not a TUI crash).
- **`lf` alias:** `lf` invokes `ls --tui` (flag injection parity; behavioral tests for non-TTY).
- **CWD write:** unit test or shell-level test that the temp file is written on quit.
- **Keybind smoke tests:** (deferred to impl) test collection + filtering + basic nav via simulated input.

## Out of scope

Tabs, bookmarks, bulk multi-select, rename/move/mkdir, git integration, sort-by-metadata-columns, recursive directory view. Save for later phases.

## Implementation order

1. Refactor `ls.cpp` — extract `collect_entries()`.
2. Build `ls_tui.cpp` — ftxui two-pane with nav + preview (dir + text + symlink + binary).
3. Add keybindings: filter, open, copy, delete, quit + CWD stdout.
4. `lf.cpp` alias + `lf init` shell function.
5. Non-interactive fallback + error handling polish.
6. Tests.
