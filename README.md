# modbox

A minimalist multi-call binary providing concise core Unix tool alternatives, inspired by BusyBox.

## Overview

modbox is a lightweight, self-contained utility that combines multiple Unix commands into a single C++20 executable. This approach reduces disk space and memory footprint while providing essential functionality for system administration and development tasks.

Unlike traditional single-purpose Unix tools, modbox uses a dispatch mechanism to handle different commands within the same binary. When invoked as `modbox cat`, it behaves like the `cat` command; when invoked as `modbox help`, it displays help information. Additionally, modbox can be invoked through symlinks to individual command names, allowing direct invocation like `/path/to/cat` or `help`.

## Features

- **Multi-call binary architecture**: All commands contained in a single executable
- **Command dispatch via unordered_map**: O(1) command lookup using `std::unordered_map`
- **Modern C++20**: Uses the C++ STL exclusively — no GLib dependency
- **GNU-style argument parsing**: Uses argtable3 for robust CLI option handling
- **Minimal dependencies**: Only requires argtable3 and the C++ standard library
- **Static analysis support**: Built-in clang-tidy integration
- **Comprehensive test suite**: Bash-based testing framework

## Available Commands

modbox currently provides the following commands (91 in total):

`arch`, `awk`, `base32`, `base64`, `basename`, `cat`, `chgrp`, `chmod`, `chown`, `comm`, `cp`, `csplit`, `cut`, `date`, `dd`, `diff`, `dir`, `dirname`, `du`, `dust`, `echo`, `env`, `expand`, `expr`, `factor`, `false`, `fd`, `find`, `grep`, `head`, `help`, `htop`, `id`, `install`, `link`, `ln`, `ls`, `lsc`, `md5sum`, `mkdir`, `mkfifo`, `mknod`, `mtop`, `mv`, `nice`, `nl`, `nohup`, `numfmt`, `pager`, `paste`, `printf`, `prompts`, `ps`, `ptx`, `pwd`, `rev`, `rg`, `rm`, `sed`, `seq`, `sh`, `sha1sum`, `sha256sum`, `shuf`, `sleep`, `sort`, `split`, `stat`, `stty`, `sync`, `tac`, `tail`, `tee`, `test`, `time`, `timeout`, `top`, `touch`, `tr`, `true`, `tsort`, `tty`, `uname`, `unexpand`, `uniq`, `unlink`, `vdir`, `wc`, `whoami`, `yes`, `zoxide`

> Note: `[` is aliased to `test`.

Run `modbox help <command>` for usage of a specific command.

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

Search for patterns in files. Supports basic regex, extended regex (ERE), and fixed-string modes.

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

### ptx

Generate a permuted index (KWIC - Key Word In Context) for files.

```
modbox ptx [OPTIONS] [FILE...]
```

Options:
- `-A`, `--auto-reference` - Generate automatic references (filename:lineno)
- `-R`, `--right-side-refs` - Put references on right side of output
- `-G`, `--traditional` - Traditional mode (System V compatibility)
- `-t`, `--typeset-mode` - Typeset mode
- `-r`, `--references` - Use input references
- `-w`, `--width=N` - Output width (default 72)
- `-g`, `--gap-size=N` - Gap size between fields (default 3)
- `-S`, `--sentence-regexp=REGEXP` - Sentence regular expression
- `-b`, `--break-file=FILE` - Break file
- `-i`, `--ignore-file=FILE` - Ignore file
- `-o`, `--only-file=FILE` - Only file

### stat

Display file or file system status, following GNU coreutils `stat` behavior.

```
modbox stat [OPTIONS] [FILE...]
```

Options:
- `-L`, `--dereference` - Follow symbolic links (report the target)
- `-f`, `--file-system` - Display file system status instead of file status
- `-t`, `--terse` - Print the information in terse form
- `-c`, `--format=FORMAT` - Use the specified FORMAT instead of the default
- `--printf=FORMAT` - Like `--format`, but interpret backslash escapes and omit the trailing newline
- `--version` - Output version information and exit

With no `-c`/`--printf`/`--format`, `stat` uses the default verbose format.
Common format directives include `%n` (name), `%s` (size), `%a` (octal access
rights), `%A` (human-readable permissions), `%F` (file type), `%i` (inode),
`%h` (link count), `%U`/`%G` (owner/group names), and `%x`/`%y`/`%z` (access,
modify, change times).

## Building

### Prerequisites

- C++20 compiler (GCC or Clang)
- argtable3, ftxui, openssl (resolved automatically via `pkg-config`)
- clang-tidy (optional, for static analysis)

The build uses a pure `Makefile` (no CMake). Dependencies are discovered
through `pkg-config`; the linuxbrew pkg-config path is added automatically
so the bundled linuxbrew libraries are found.

### Build Steps

```bash
# Standard build
make

# Build and run
make run

# Full rebuild (clean + rebuild)
make refresh

# Clean build artifacts
make clean

# Run static analysis
make lint
```

Build options:

```bash
# Debug build (adds -g -O0)
make DEBUG=1

# Build with clang-tidy analysis on every translation unit
make TIDY=1
# or: make refresh-tidy
```

### Build Output

The compiled binary is placed at `./target/modbox`. Object files and
per-source `.d` dependency files live under `./build`; `compile_commands.json`
is generated at the project root for clang-tidy and editors.

### Dependencies

- argtable3, ftxui, openssl via `pkg-config`
- C++ standard library (no GLib dependency)
- clang-tidy (optional, for static analysis)
- No CMake / vcpkg required

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

# Build-time analysis (clang-tidy runs during compilation)
make TIDY=1
# or: make refresh-tidy
```

Enabled checks include:
- `clang-analyzer-*`, `bugprone-*`, `cert-*`
- `misc-*`, `readability-*`, `modernize-*`, `portability-*`

## Code Conventions

### Language and Standard Library

- C++20 standard
- All source files are `.cpp`, all headers are `.hpp`
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

### Options Structs

Options structs use default member initializers (C++20). Config structs are passed by pointer (`const XxxOptions*`):

```cpp
struct XxxOptions {
    bool flag_a = false;
    bool flag_b = false;
    unsigned long block_size = 0;
};

// Usage
XxxOptions opts;
opts.flag_a = (arg_opt->count > 0);
some_helper(src, dst, &opts);
```

### Include Cleanups

Add direct includes for functions used:

```cpp
#include <cstring>   // for std::strcmp
#include <ctime>     // for std::strftime
```

## Architecture

### Command Registration

Commands are registered in a `std::unordered_map` at runtime:

```cpp
using CommandFunc = void (*)(int, char**);

static const std::unordered_map<std::string, CommandFunc> commands = {
    {"help", help_command},
    {"cat", cat_command},
    {"ls", ls_command},
    {"cp", cp_command},
    {"mv", mv_command},
    {"ln", ln_command},
    // ...
};
```

### Command Interface

Each command follows a standard function signature:

```cpp
void command(int argc, char** argv);
```

### Argument Parsing

Uses argtable3 for POSIX/GNU-compatible argument parsing:

```cpp
struct arg_lit* number_opt = arg_lit0("n", "number", "number all output lines");
struct arg_file* file_arg = arg_filen(nullptr, nullptr, "FILE", 0, 100, "file to read");
```

## Extending modbox

To add a new command:

1. Create a header file in `include/commands/`:
   ```cpp
   #ifndef NEWCMD_HPP
   #define NEWCMD_HPP

   void newcmd_command(int argc, char** argv);

   #endif
   ```

2. Create the implementation in `src/commands/newcmd.cpp`

3. Register the command in `src/main.cpp`:
   ```cpp
   commands["newcmd"] = newcmd_command;
   ```

## Design Philosophy

modbox follows these principles:

- **Simplicity**: Minimal code, straightforward implementation
- **Composability**: Multiple commands in one binary
- **Standards compliance**: GNU-style options where appropriate
- **Lean dependencies**: Only essential libraries required (C++ STL + argtable3)
- **Code quality**: Static analysis and comprehensive testing

## Current Status

- ✅ Implemented commands: 91 commands including help, cat, ls, cp, mv, ln, grep, ptx, stat, and more
- ✅ Comprehensive test suite
- ✅ Static analysis integration
- ✅ GNU-style argument parsing
- ✅ Multi-call binary architecture
- 🔄 Future work: man pages, additional commands

## See Also

- [BusyBox](https://www.busybox.net/) - The original multi-call binary
- [argtable3](https://argtable.org/) - Command-line parser
- [vcpkg](https://vcpkg.io/) - C++ package manager

## License

MIT License
