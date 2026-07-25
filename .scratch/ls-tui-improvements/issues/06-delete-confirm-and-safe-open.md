# 06 — Add delete confirmation and safe open action

**What to build:** Two safety/UX improvements. First, `d` enters a confirm mode showing "Delete <name>? y/N"; `y` executes the delete, any other key cancels. Second, `o` uses `execlp` instead of `system()` for safe path handling, refreshes the entry list after the editor exits, and `cd`s into directories instead of trying to open them as files.

**Blocked by:** 03

**Status:** ready-for-agent

- [ ] TuiCtx gains confirm_mode, confirm_path, confirm_label fields
- [ ] `d` when not in confirm mode: set confirm state, show "Delete <name>? y/N" in status bar / footer
- [ ] `y` in confirm mode: execute unlink/rmdir, clear confirm state, refresh entries, show "Deleted: <name>"
- [ ] Any other key in confirm mode: cancel, clear confirm state
- [ ] `o` on a directory: cd into it (same as Enter)
- [ ] `o` on a regular file: use execlp(editor, editor, path, nullptr) instead of system()
- [ ] After editor exits: re-run tui_collect_entries and update the entry list so edits appear immediately
- [ ] File names with spaces, quotes, and special characters are handled without shell interpretation
