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

Examples:
```
modbox cat file.txt              # Display contents of file.txt
modbox cat -n file.txt           # Display with line numbers
modbox cat file1.txt file2.txt   # Concatenate multiple files
modbox cat -                    # Read from stdin
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

## Limitations

- No tests currently included
- Limited command set (currently: help, cat)
- No man pages yet

## See Also

- [BusyBox](https://www.busybox.net/) - The original multi-call binary
- [GLib](https://developer.gnome.org/glib/) - Utility library
- [argtable3](https://argtable.org/) - Command-line parser

## License

MIT License