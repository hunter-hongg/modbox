# 03 — Wire --tui flag + find_tui_main entry point

**What to build:** The final integration layer. Registers the `--tui` flag in find's argtable, adds the TTY guard in `find_command` so piped stdout falls back to plain `-print`, updates `--help` text, and creates the `find_tui_main` entry function that collects matches, instantiates `FindTuiComponent`, and runs the ftxui loop.

**Blocked by:** 01 — Extract find_collect_matches collector
**Blocked by:** 02 — Build FindTuiComponent

**Status:** ready-for-agent

- [ ] `--tui` arg_lit flag added to `find_command` argtable
- [ ] TTY guard: when `--tui` is present and `isatty(STDOUT_FILENO)` is true, `find_tui_main` is called; when piped, flag is silently ignored and normal `-print` path executes
- [ ] `--help` output includes `--tui` description
- [ ] `find_tui_main` entry function collects matches via `find_collect_matches`, configures and runs `FindTuiComponent` via `App::FitComponent`
- [ ] `find --help` and all existing `test_find.sh` cases pass without modification
