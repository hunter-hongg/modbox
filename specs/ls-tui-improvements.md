# Spec: ls --tui / lf Interactive File Browser Improvements

## Problem Statement

`ls --tui` (alias `lf`) gives modbox users an interactive file browser inside the terminal, but the current implementation is missing core navigation, sorting, and safety features that users expect from any file manager. Getting stuck in a subdirectory requires quitting entirely, deleting a file has no confirmation, and large directories render all entries at once with no scroll offset. Meanwhile, `ps_tui` already solved scroll, search, and live-refresh — but `ls_tui` got none of it because the scaffolding was copy-pasted instead of shared.

## Solution

Refactor `ls_tui` onto a shared `TuiBase` component class (per ADR 0001), then layer on navigation history, sort cycling, delete confirmation, richer file-type icons, and a safe open action. Expose `lf` as a first-class shell-integrated alias that writes its final CWD to `~/.cache/lf/cwd` (per ADR 0002) instead of relying on stdout capture.

## User Stories

1. As a user browsing a deep directory tree, I want to press `h` or `Backspace` to go to the parent directory, so that I don't have to quit and restart.
2. As a user, I want `Enter` to open a subdirectory and update the current view, so that I can drill into nested folders.
3. As a user, I want a history stack with `u` / `U` to jump back and forward, so that I can retrace my steps without losing place.
4. As a user, I want `s` to cycle through sort modes (name, size, mtime, type), so that I can reorganize the listing without leaving the TUI.
5. As a user, I want `S` to reverse the current sort direction, so that I can see largest/newest first.
6. As a user, I want the sort mode to persist when I `cd` into a subdirectory, so that I don't have to re-sort at every level.
7. As a user, I want `d` to show a `y/N` confirmation prompt before deleting, so that I don't accidentally remove the wrong file.
8. As a user, I want the confirmation prompt to disappear if I press any key other than `y`, so that a stray keystroke doesn't confirm deletion.
9. As a user, I want to see more precise file-type icons (socket, fifo, block-device, character-device) in the entry list, so that I can identify special files at a glance.
10. As a user, I want `o` on a directory to `cd` into it, and `o` on a regular file to open it with `$EDITOR`, so that the key behaves intuitively for both cases.
11. As a user, I want `o` to use `execlp` instead of `system()`, so that paths with spaces or special characters are handled safely.
12. As a user, I want the file list to refresh automatically after I close the editor, so that I see any changes the editor made.
13. As a user, I want `j`/`k` and arrow keys to scroll the entry list with proper offset management, so that I can navigate large directories without the render loop choking.
14. As a user, I want alternating row background colors, so that I can track my place across long listings.
15. As a user, I want `q` to return me to the shell in the directory I was last browsing, so that my shell prompt updates automatically.
16. As a user, I want `lf init <shell>` to generate shell wrapper code that reads the CWD file and `cd`s me there, so that `lf` integrates cleanly with zsh, bash, or fish.
17. As a developer adding future TUI commands (e.g. `cat --tui`, `find --tui`), I want a shared `TuiBase` class that handles scroll, search, nav keys, and chrome, so that every new TUI gets consistent UX without copy-pasting scaffolding.
18. As a maintainer, I want `ps_tui` and `ls_tui` to share the same base class, so that bug fixes to scroll or search behaviour apply to both at once.
19. As a user, I want the preview pane to show a directory's children, symlink target, or file contents depending on the selected entry type, so that I get context without opening the file.
20. As a user, I want `/` to filter the entry list in real time, so that I can quickly narrow down to the file I'm looking for.

## Implementation Decisions

- **Shared `TuiBase` component class.** Extract a non-template abstract `TuiBase` from `PsTuiComponent` and `LsfComponent`. It owns: `scroll_offset_`, `max_rows_`, `selected_idx_`, `search_mode_`, `search_input_`, `search_query_`, total entries vector, header/footer rendering, and default key bindings (`j`/`k`, `/`, `q`, arrow keys, Home/End/PageUp/PageDown). Subclasses override `fill_entries()`, `render_row(idx)`, and `on_command_key(event)`.
- **`ls_tui` migrates to `TuiBase`.** `LsfComponent` inherits `TuiBase<TuiEntry>`. `fill_entries()` calls `tui_collect_entries()` and applies the current sort mode. `render_row(idx)` maps a `TuiEntry` to an ftxui `Element` with the appropriate icon, permission string, owner, size, and mtime.
- **Directory history stack.** `TuiCtx` gains `std::vector<std::string> history_` and `int history_pos_`. `Enter` and `h`/`Backspace` push/pop the stack. `u` decrements `history_pos_` and reloads; `U` increments it. Going `cd ..` always pops.
- **Sort engine.** `TuiCtx` gains `SortMode` (name, size, mtime, type) and `bool sort_reverse_`. `s` cycles forward through modes; `S` toggles reverse. Sort is applied after every `fill_entries()` call, including after `cd`.
- **Delete confirmation state machine.** `TuiCtx` gains `enum class ConfirmMode { None, Delete } confirm_mode_`, `std::string confirm_path_`, and `std::string confirm_label_`. When `d` is pressed, if `confirm_mode_ == None` the TUI enters confirm mode and shows the prompt; `y` executes the delete, any other key cancels.
- **Richer file-type detection.** `TuiEntry` gains a `FileType` enum (regular, directory, symlink, socket, fifo, block-device, char-device, unknown). `ls_entry_to_tui` sets it via `S_ISREG/S_ISDIR/S_ISLNK/S_ISSOCK/S_ISFIFO/S_ISBLK/S_ISCHR`. The renderer maps each type to a distinct icon.
- **Safe open action.** Replace `system(editor + " " + path)` with `execlp(editor, editor, path, nullptr)`. After the child exits, `tui_collect_entries()` refreshes the entries vector so edits appear immediately. If the selected entry is a directory, `o` behaves identical to `Enter` (`cd` into it).
- **`lf` CWD file protocol.** On quit, `ls_tui` writes the final `current_dir` to `~/.cache/lf/cwd` (creating the directory if needed). It no longer prints CWD to stdout. A new `lf` command module implements `lf init <shell>` which prints a shell function that reads the cwd file and `cd`s before exec'ing `modbox ls --tui`.
- **Footer keybinding hint updates.** The footer shows context-aware hints: sort mode indicator, confirm-mode prompt when active, history position badge.
- **Scroll management.** `TuiBase` owns the scroll offset math. `OnRender` renders only `max_rows_` rows starting at `scroll_offset_`. `vscroll_indicator` stays in the layout for visual affordance, but the actual offset is owned by the base class.

## Testing Decisions

- **Test seam:** The highest seam is the `TuiBase` interface contract. Each TUI command is tested by injecting a mock or fixed entry vector and simulating ftxui events, then asserting on the rendered output or state transitions.
- **Unit-testable seams:**
  - `ls_entry_to_tui` conversion (file-type detection, permission string generation).
  - `filter_entries` with various query strings.
  - Sort comparator functions for each `SortMode`.
  - History stack push/pop/navigation logic.
  - Delete confirmation state transitions (enter confirm → confirm → cancel).
  - `lf init` shell output for each supported shell.
- **Integration seams:**
  - `LsfComponent` event simulation: j/k nav, `/` filter, `s`/`S` sort, `d` delete confirm, `h`/`u`/`U` history, `o` open, `q` quit → CWD file write.
  - `TuiBase` scroll math with N > max_rows_ entries.
- **Prior art:** Existing bash test helpers (`assert_cmd`, `assert_cmd_pat`) are used for shell-level integration tests (e.g. `lf init zsh` output format). TUI event simulation tests follow the same pattern as other command unit tests in `tests/`.
- **What not to test:** Internal ftxui `Element` tree shape is an implementation detail. Tests should assert on observable state (`TuiCtx` fields) and on stdout/stderr output where applicable.

## Out of Scope

- Syntax-highlighted preview pane (bat/delta style). That requires integrating a syntax-highlighter library and is deferred.
- Tree view (parent/child process-style indentation) for directories. The current flat list with `cd ..` is sufficient.
- Multi-select / bulk operations (bulk delete, bulk copy). The spec covers single-entry operations only.
- Bookmark / favorite directory system. History is transient per session only.
- Remote filesystem support (SSH/SFTP). Only local `readdir` paths are in scope.
- `cat --tui`, `find --tui`, or any future TUI commands beyond the refactor of the existing two. The `TuiBase` foundation is in scope; new commands built on it are not.

## Further Notes

- ADR 0001 (shared TUI base class) and ADR 0002 (lf CWD file protocol) are already accepted and constrain this spec.
- The `lf` name is the human-facing brand; `ls --tui` is the underlying modbox invocation. Both must remain supported.
- `ps_tui` will be migrated to `TuiBase` in the same refactor pass so the two TUIs do not diverge again.
