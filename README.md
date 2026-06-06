# modbox

A minimalist multi-call binary providing concise core Unix tool alternatives, inspired by BusyBox.

## Overview

modbox is a lightweight, self-contained utility that combines multiple Unix commands into a single executable. This approach reduces disk space and memory footprint while providing essential functionality for system administration and development tasks.

Unlike traditional single-purpose Unix tools, modbox uses a dispatch mechanism to handle different commands within the same binary. When invoked as `modbox cat`, it behaves like the `cat` command; when invoked as `modbox help`, it displays help information. Additionally, modbox can create symlinks to individual command names, allowing direct invocation like `/path/to/cat` or `help`.

## Features

- **Multi-call binary architecture**: All commands contained in a single executable
- **Command dispatch via hash table**: O(1) command lookup using GLib's GHashTable
- **GNU-style argument parsing**: Uses argtable3 for robust CLI option handling
- **Minimal dependencies**: Only requires GLib and argtable3
- **Static analysis support**: Built-in clang-tidy integration
- **Comprehensive test suite**: Bash-based testing framework

## Available Commands

### help

Displays usage information for modbox and its available commands.

```
modbox help
```

### cat

Concatenates and displays files. Supports reading from multiple files or stdin.

```
modbox cat [OPTIONS] [FILE...]
```

Options:
- `-n`, `--number` - Number all output lines
- `-b`, `--number-nonblank` - Number nonempty output lines
- `-E`, `--show-ends` - Display `$` at end of each line
- `-T`, `--show-tabs` - Display TAB characters as `^I`
- `-s`, `--squeeze-blank` - Never more than one single blank line
- `-v`, `--show-nonprinting` - Use `^` and `M-` notation for non-printing characters

### ls

List directory contents.

```
modbox ls [OPTIONS] [FILE...]
```

Options:
- `-a`, `--all` - Do not ignore entries starting with `.`
- `-A`, `--almost-all` - Do not list implied `.` and `..`
- `-l`, `--long` - Use a long listing format
- `--author` - With -l, print the author of each file
- `-b`, `--escape` - Print octal escapes for non-graphic characters
- `-B`, `--ignore-backups` - Do not list entries ending with `~`
- `--color=WHEN` - Colorize the output (always, auto, never)
- `--block-size=SIZE` - Scale sizes by SIZE when printing them

### grep

Search for patterns in files. Similar to GNU grep with basic regex (GRegex/PCRE-like), ERE, and fixed-string modes.

```
modbox grep [OPTIONS] PATTERN [FILE...]
```

Options:
- `-E`, `--extended-regexp` - Interpret pattern as extended regex (ERE)
- `-F`, `--fixed-strings` - Interpret pattern as fixed strings (literal)
- `-i`, `--ignore-case` - Ignore case distinctions
- `-v`, `--invert-match` - Select non-matching lines
- `-n`, `--line-number` - Print line number with output lines
- `-c`, `--count` - Print only a count of matching lines per file
- `-l`, `--files-with-matches` - Print only FILE names containing matches
- `-w`, `--word-regexp` - Match only whole words
- `-x`, `--line-regexp` - Match only whole lines
- `-o`, `--only-matching` - Show only the matched part of a line
- `-H`, `--with-filename` - Print the file name for each match
- `-h`, `--no-filename` - Suppress the file name prefix on output
- `-r`, `--recursive` - Search directories recursively
- `--color=WHEN` - Highlight matching text (always, auto, never)
- `-e`, `--regexp=PATTERN` - Use PATTERN as the pattern

Not implemented (relative to GNU grep):
`-A`/`-B`/`-C` (context), `-P` (PCRE), `-z` (null-data), `-b` (byte-offset),
`-q` (quiet), `-s` (suppress errors), `-d` (directory action),
`--include`/`--exclude`, `-m` (max-count), `--label`, `-Z` (null output).

### cp

Copy files and directories.

```
modbox cp [OPTIONS] SOURCE... DEST
```

Options:
- `-r`, `-R`, `--recursive` - Copy directories recursively
- `-i`, `--interactive` - Prompt before overwrite
- `-v`, `--verbose` - Explain what is being done
- `-f`, `--force` - Remove existing destination files

### mv

Move or rename files and directories.

```
modbox mv [OPTIONS] SOURCE... DEST
```

Options:
- `-i`, `--interactive` - Prompt before overwrite
- `-v`, `--verbose` - Explain what is being done
- `-f`, `--force` - Remove existing destination files

### ln

Create links between files.

```
modbox ln [OPTIONS] TARGET LINK_NAME
```

Options:
- `-f`, `--force` - Remove existing destination file before linking
- `-s`, `--symbolic` - Make symbolic links instead of hard links
- `-v`, `--verbose` - Explain what is being done

## Building

### Prerequisites

- CMake 3.10+
- GLib 2.0
- argtable3 (via vcpkg)
- C compiler (GCC or Clang)
- vcpkg (for dependency management)

### Build Steps

```bash
# Standard build
make

# Build and run
make run

# Full rebuild (clean + regenerate CMake)
make refresh

# Clean build artifacts
make clean

# Run static analysis
make lint
```

### Build Output

The compiled binary is placed at `./target/modbox`.

### Dependencies

- vcpkg (manages argtable3)
- System glib-2.0
- clang-tidy (optional, for static analysis)
- CMake toolchain hardcoded to `$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake`

## Testing

Run the comprehensive test suite:

```bash
bash tests/run_tests.sh
```

The test suite includes:
- `assert_cmd EXPECTED_OUTPUT args...` - Compares stdout exactly
- `assert_cmd_pat PATTERN args...` - Checks stdout for regex pattern
- `assert_cmd_not_pat PATTERN args...` - Checks stdout lacks regex pattern
- `assert_cmd_pat_stderr PATTERN args...` - Checks stderr for regex pattern

## Static Analysis

Built-in clang-tidy integration with comprehensive checks:

```bash
# Run static analysis on all source files
make lint

# Or enable build-time analysis
cmake -DENABLE_CLANG_TIDY=ON ..
make
```

Enabled checks include:
- `clang-analyzer-*`, `bugprone-*`, `cert-*`
- `misc-*`, `readability-*`, `modernize-*`, `portability-*`

## Code Conventions

### Struct Parameters

Configuration parameters must be passed via structs:

```c
typedef struct {
    int flag_a;
    int flag_b;
    unsigned long block_size;
} XxxOptions;

// Usage
XxxOptions opts = {0};
opts.flag_a = (arg_opt->count > 0);
some_helper(src, dst, &opts);
```

### Include Cleanups

Add direct includes for functions used:

```c
#include <string.h>  // for strcmp
#include <time.h>    // for strftime
```

## Building

### Prerequisites

- CMake 3.10+
- GLib 2.0
- argtable3
- C compiler (GCC or Clang)

### Build Steps

```bash
# Standard build
make

# Or manually with CMake
mkdir build && cd build
cmake ..
make

# Run the binary
./target/modbox

# Full rebuild
make refresh
```

### Build Output

The compiled binary is placed at `./target/modbox`.

## Architecture

### Command Registration

Commands are registered in a hash table at runtime:

```c
GHashTable* commands = g_hash_table_new(g_str_hash, g_str_equal);
g_hash_table_insert(commands, "help", help_command);
g_hash_table_insert(commands, "cat", cat_command);
g_hash_table_insert(commands, "ls", ls_command);
g_hash_table_insert(commands, "cp", cp_command);
g_hash_table_insert(commands, "mv", mv_command);
g_hash_table_insert(commands, "ln", ln_command);
```

### Command Interface

Each command follows a standard function signature:

```c
typedef void (*command_t)(gint argc, gchar** argv);
```

### Argument Parsing

Uses argtable3 for POSIX/GNU-compatible argument parsing:

```c
struct arg_lit* number_opt = arg_lit0("n", "number", "number all output lines");
struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file to read");
```

## Extending modbox

To add a new command:

1. Create a header file in `include/commands/`:
   ```c
   #ifndef NEWCMD_H
   #define NEWCMD_H
   
   #include <glib.h>
   
   void newcmd_command(gint argc, gchar** argv);
   
   #endif
   ```

2. Create the implementation in `src/commands/newcmd.c`

3. Register the command in `src/main.c`:
   ```c
   g_hash_table_insert(commands, "newcmd", newcmd_command);
   ```

## Design Philosophy

modbox follows these principles:

- **Simplicity**: Minimal code, straightforward implementation
- **Composability**: Multiple commands in one binary
- **Standards compliance**: GNU-style options where appropriate
- **Lean dependencies**: Only essential libraries required
- **Code quality**: Static analysis and comprehensive testing

## Current Status

- ✅ Implemented commands: help, cat, ls, cp, mv, ln, grep
- ✅ Comprehensive test suite
- ✅ Static analysis integration
- ✅ GNU-style argument parsing
- ✅ Multi-call binary architecture
- 🔄 Future work: man pages, additional commands

## See Also

- [BusyBox](https://www.busybox.net/) - The original multi-call binary
- [GLib](https://developer.gnome.org/glib/) - Utility library
- [argtable3](https://argtable.org/) - Command-line parser
- [vcpkg](https://vcpkg.io/) - C++ package manager

## License

MIT License