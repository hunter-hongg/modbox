# modbox

C CLI tool with command dispatch (help, cat).

## Build Commands

```bash
make        # or make compile - build the project
make run   # build and run ./target/modbox
make clean # rm -rf build target
make refresh # full rebuild: clean + cmake regenerate
```

## Key Files

- `src/main.c` - entry point, command dispatch via GHashTable
- `CMakeLists.txt` - build config
- `vcpkg.json` - dependencies (argtable3)

## Dependencies

- vcpkg for argtable3
- glib-2.0 (system)

CMake toolchain is hardcoded to `$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake`.

## Notes

- No tests in this repo
- No lint/typecheck (C project)
- Build output goes to `./target/modbox`