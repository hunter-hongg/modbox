# modbox

## Development

### Building
- `make` or `make compile` - build project
- `make run` - build and run `./target/modbox`
- `make clean` - remove `build` and `target` directories
- `make refresh` - full rebuild (clean + regenerate CMake)

### Adding Commands
1. Create header: `include/commands/<cmd>.h`
   ```c
   #ifndef <CMD>_H
   #define <CMD>_H
   #include <glib.h>
   void <cmd>_command(gint argc, gchar** argv);
   #endif
   ```
2. Implement: `src/commands/<cmd>.c`
3. Register in `src/main.c`:
   ```c
   g_hash_table_insert(commands, "<cmd>", <cmd>_command);
   ```

### Command Interface
- Signature: `void command(gint argc, gchar** argv)`
- Uses argtable3 for argument parsing (see `src/commands/cat.c`)
- Command lookup via GHashTable in `src/main.c`

### Dependencies
- vcpkg (manages argtable3)
- System glib-2.0
- CMake toolchain hardcoded to `$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake`

### Notes
- Build output: `./target/modbox`
- No tests, lint, or typecheck configured
- Entry point: `src/main.c` (command dispatch)
- Current commands: help, cat

### cat Command Options
- `-n`, `--number` — number all output lines
- `-b`, `--number-nonblank` — number nonempty output lines
- `-E`, `--show-ends` — display `$` at end of each line
- `-T`, `--show-tabs` — display TAB characters as `^I`
- `-s`, `--squeeze-blank` — never more than one single blank line
- `-v`, `--show-nonprinting` — use `^` and `M-` notation for non-printing characters (except LFD and TAB)
- Options can be combined (e.g., `-vTE`), via `expand_short_options` in `src/commands/cat.c`
- Visual character output helper: `output_char_visual()` in `src/commands/cat.c`
