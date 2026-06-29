# modbox

## Development

### Building

- `make` or `make compile` - build project
- `make run` - build and run `./target/modbox`
- `make clean` - remove `build` and `target` directories
- `make refresh` - full rebuild (clean + regenerate CMake)

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
- Uses argtable3 for argument parsing
- POSIX APIs (fopen, stat, readdir, etc.) used directly

### Adding Commands

1. Create header: `include/commands/<cmd>.hpp`
2. Implement: `src/commands/<cmd>.cpp`
3. Register in `src/main.cpp`:
   ```cpp
   g_hash_table_insert(commands, "<cmd>", <cmd>_command);
   ```
   Replace with unordered_map:
   ```cpp
   commands["<cmd>"] = <cmd>_command;
   ```

### Command Interface

- Signature: `void command(int argc, char** argv)`
- Uses argtable3 for argument parsing
- Command lookup via `std::unordered_map` in `src/main.cpp`

### Dependencies

- vcpkg (manages argtable3)
- No glib dependency
- CMake toolchain hardcoded to `$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake`

### Static Analysis (clang-tidy)

- Configured via `.clang-tidy` at project root
- The `lint` make target and build-time analysis both pass `--system-headers=false`

### Running Tests

- `bash tests/run_tests.sh` — self-contained Bash test script
- Exit code 0 = all pass, exit code 1 = some fail
- Test helpers: `assert_cmd`, `assert_cmd_pat`, `assert_cmd_not_pat`, `assert_cmd_pat_stderr`
