# modbox

## Development

### Building

- `make` or `make compile` - build project
- `make run` - build and run `./target/modbox`
- `make clean` - remove `build` and `target` directories
- `make refresh` - 完全重建（clean + rebuild）

### Code Convention

- C++20 standard
- All source files are `.cpp`, all headers are `.hpp`
- Options structs use default member initializers (C++20)
- Config structs passed as `const XxxOptions*`
- No GLib dependency — uses C++ STL exclusively
  - `std::vector` for dynamic arrays (replaces `GPtrArray`, `GArray`, `GByteArray`)
  - `std::unordered_map` for hash maps (replaces `GHashTable`)
  - `std::string` for strings (replaces `gchar*` + manual memory management)
  - `std::regex` for regex (replaces `GRegex`)
  - `std::filesystem` for path manipulation (replaces `g_path_get_basename`, `g_build_filename`)
- Standard C types: `int` for `gint`, `size_t` for `gsize`, `int64_t` for `gint64`, `uint64_t` for `guint64`, `bool` for `gboolean`
- Memory management: `new`/`delete` or RAII containers (no `g_malloc`/`g_free`)
- Uses argtable3 for argument parsing — the API is implemented in-tree
  (`include/argtable3.h` + `src/argtable3.cpp`), not linked from a system library
- POSIX APIs (fopen, stat, readdir, etc.) used directly

### Adding Commands

1. Create header: `include/commands/<cmd>.hpp`
2. Implement: `src/commands/<cmd>.cpp`
3. Register in `src/commands/<cmd>.cpp` using the `REGISTER_COMMAND` macro:
   ```cpp
   #include "commands/command_macros.hpp"

   int <cmd>_command(int argc, char** argv) {
       // ...
   }

   REGISTER_COMMAND("<cmd>", <cmd>_command, "Description");
   ```

### Command Interface

- Signature: `int command(int argc, char** argv)`
- Uses argtable3 for argument parsing (in-tree; see Code Convention)
- Command lookup via `CommandRegistry` singleton in `src/main.cpp`

### Dependencies

- ftxui, openssl, libselinux, libacl (via pkg-config)
- argtable3 is in-tree, not an external dependency
- No GLib dependency
- Pure Makefile build — no CMake or vcpkg required

### Static Analysis (clang-tidy)

- Configured via `.clang-tidy` at project root
- The `lint` make target and build-time analysis both pass `--system-headers=false`

### Running Tests

- `make test` / `bash tests/run_tests.sh` — test suite frontend
- `tests/run_tests.sh` delegates by default to `tests/run_tests.py` (a Python
  orchestrator that runs the `tests/test_*.sh` files in parallel across all
  CPU cores). Pass `SERIAL=1` for serial execution, or `USE_BASH=1` to use the
  original pure-bash runner. Requires `python3`.
- Exit code 0 = all pass, exit code 1 = some fail
- Test helpers: `assert_cmd`, `assert_cmd_pat`, `assert_cmd_not_pat`, `assert_cmd_pat_stderr`

### Special Notes

- `tests/test_sh.sh` has been removed intentionally — do NOT add a `sh` command test back.

## Agent skills

### Issue tracker

Issues are tracked as GitHub issues. See `docs/agents/issue-tracker.md`.

### Triage labels

Five canonical triage labels: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context repo with ADRs in `docs/adr/`. Read relevant ADRs before exploring the area you're working in. See `docs/agents/domain.md`.
