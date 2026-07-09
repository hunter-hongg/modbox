# modbox Usage Guide

modbox is a small multi-command binary. Invocation:

```bash
./modbox <command> [options] [args...]
```

Run `./modbox help` for a command listing, or `./modbox <command> --help` for per-command help.

Commands:

| Command  | Purpose                                       |
| -------- | --------------------------------------------- |
| `cat`    | Concatenate files and print to standard output |
| `chgrp`  | Change group ownership                         |
| `chmod`  | Change file mode bits                          |
| `chown`  | Change file owner and group                    |
| `cp`     | Copy files and directories                     |
| `find`   | Search for files in a directory hierarchy      |
| `grep`   | Search for patterns in files                   |
| `help`   | List available commands                        |
| `ln`     | Create hard or symbolic links                  |
| `ls`     | List directory contents                        |
| `mv`     | Move (rename) files                            |
| `ptx`    | Generate a permuted index (KWIC index)         |
| `rg`     | Recursive pattern search (ripgrep-style)       |
---

## cat

Concatenate FILE(s) to standard output. With no FILE, or when FILE is `-`, read from standard input.

### Synopsis

```
modbox cat [OPTION]... [FILE]...
```

### Standard options

| Option                       | Description                                                                                 |
| ---------------------------- | ------------------------------------------------------------------------------------------- |
| `-b`, `--number-nonblank`    | Number nonempty output lines                                                                |
| `-E`, `--show-ends`          | Display `$` at end of each line                                                             |
| `-n`, `--number`             | Number all output lines                                                                     |
| `-s`, `--squeeze-blank`      | Suppress repeated empty lines (collapse runs of blank lines to one)                         |
| `-T`, `--show-tabs`          | Display TAB characters as `^I`                                                              |
| `-v`, `--show-nonprinting`   | Use `^` and `M-` notation for non-printing characters (except LFD and TAB)                 |
| `-e`                         | Equivalent to `-vE`                                                                         |
| `-t`                         | Equivalent to `-vT`                                                                         |
| `-A`, `--show-all`           | Equivalent to `-vET`                                                                        |
| `--less`                     | Pager mode (interactive: `j` next, `k` previous, `q` quit)                                  |
| `-h`, `--help`               | Display help and exit                                                                       |

### Dev-tool options

| Option                | Description                                                            |
| --------------------- | ---------------------------------------------------------------------- |
| `--blame`             | Prefix each line with git blame info (`commit author date`)            |
| `--highlight`         | Syntax-highlight output based on file extension (auto, TTY only)       |
| `--header`            | Print a file metadata banner before the content                        |
| `--diff=FILE`         | Print unified diff between the input file and FILE                     |

### Content navigation

| Option                   | Description                                                  |
| ------------------------ | ------------------------------------------------------------ |
| `--range=N-M`            | Show only lines N through M (1-indexed, inclusive)           |
| `--grep=PATTERN`         | Keep only lines matching the extended regex PATTERN           |
| `--context=N`            | With `--grep`, also show N context lines around each match   |
| `--head=N`               | Show only the first N lines                                  |
| `--tail=N`               | Show only the last N lines                                   |
| `--number-format=FMT`    | Line-number format: `decimal` (default), `hex`, or `octal`   |
| `--stats`                | Print line / word / character counts after the output        |

### Examples

```bash
modbox cat file.txt
modbox cat -n file.txt
modbox cat -E file.txt
modbox cat -A file.txt
modbox cat --blame main.c
modbox cat --highlight --range=10-30 main.c
modbox cat --grep='TODO' --context=2 changelog.md
modbox cat --head=20 --tail=10 big.log
modbox cat --less long.txt
modbox cat a.txt b.txt > combined.txt
echo "hello" | modbox cat
```

### Notes

- Short options can be combined: `-vET` is equivalent to `-A`.
- `--grep` and `--range` apply after the file is read; the rest of the pipeline (`--squeeze-blank`, `--head`, `--tail`) chains in order.
- `--diff`, `--grep`, `--range`, `--head`, `--tail` cannot be combined freely — they operate on the line stream and may filter each other out.

---

## cp

Copy SOURCE to DEST, or multiple SOURCE(s) into DIRECTORY.

### Synopsis

```
modbox cp [OPTION]... SOURCE DEST
modbox cp [OPTION]... SOURCE... DIRECTORY
modbox cp -t DIRECTORY [OPTION]... SOURCE...
```

### Options

| Option                            | Description                                                            |
| --------------------------------- | ---------------------------------------------------------------------- |
| `-r`, `--recursive`               | Copy directories recursively                                           |
| `-v`, `--verbose`                 | Print `'src' -> 'dst'` for every file copied                           |
| `-f`, `--force`                   | Remove existing destination file if open fails                         |
| `-n`, `--no-clobber`               | Never overwrite an existing destination                                |
| `-i`, `--interactive`             | Prompt before overwriting (`y/N`)                                       |
| `-u`, `--update`                  | Copy only when SOURCE is newer than DEST                               |
| `-p`, `--preserve`                | Preserve mode, ownership, timestamps                                   |
| `-t`, `--target-directory=DIR`    | Copy all sources into DIR (DIR must exist and be a directory)          |
| `-h`, `--help`                    | Display help and exit                                                  |

### Examples

```bash
modbox cp file.txt copy.txt
modbox cp -v file.txt /tmp/
modbox cp -r src/ dst/
modbox cp -rp src/ backup/
modbox cp -i file.txt /existing/
modbox cp -n file.txt /existing/
modbox cp -u source.txt dest.txt
modbox cp -t /backup file1 file2 file3
modbox cp -rvi project/ /tmp/backup/
```

### Notes

- `-n` overrides `-f` and `-i` (safer takes precedence).
- For cross-device copies, behavior is the same as within a single device — the kernel handles it transparently. `mv` is the command that needs the rename→copy fallback.
- Non-recursive mode with multiple sources is rejected — use `-r` for directories.
- `--preserve` flushes stdio buffers before setting timestamps to prevent the final `fclose()` from overwriting the mtime via `utimensat`.

---

## grep

Search for PATTERN in each FILE or standard input.

### Synopsis

```
modbox grep [OPTION]... PATTERN [FILE]...
```

### Pattern selection

| Option                       | Description                                                              |
| ---------------------------- | ------------------------------------------------------------------------ |
| `-E`, `--extended-regexp`   | Interpret PATTERN as extended regular expression (ERE)                   |
| `-F`, `--fixed-strings`     | Interpret PATTERN as a set of newline-separated fixed strings            |
| `-e`, `--regexp=PATTERN`     | Use PATTERN as the pattern (use this for patterns starting with `-`)     |

### Matching control

| Option                     | Description                                                |
| -------------------------- | ---------------------------------------------------------- |
| `-i`, `--ignore-case`     | Ignore case distinctions                                   |
| `-v`, `--invert-match`    | Select non-matching lines                                  |
| `-w`, `--word-regexp`     | Match only whole words                                     |
| `-x`, `--line-regexp`     | Match only whole lines                                     |

### Output control

| Option                            | Description                                                                  |
| --------------------------------- | ---------------------------------------------------------------------------- |
| `-c`, `--count`                  | Print only a count of matching lines per file                                |
| `-l`, `--files-with-matches`     | Print only the names of FILEs containing matches                             |
| `-n`, `--line-number`            | Prefix each line with its 1-based line number                                |
| `-o`, `--only-matching`          | Show only the part of a line matching PATTERN (one match per line)           |
| `-H`, `--with-filename`         | Force the file-name prefix on every match                                    |
| `-h`, `--no-filename`           | Suppress the file-name prefix on output                                      |
| `--color=WHEN`                   | Highlight matches: `always`, `auto`, or `never`                              |

### Other

| Option                     | Description                                                              |
| -------------------------- | ------------------------------------------------------------------------ |
| `-r`, `--recursive`       | Read all files under directories recursively                              |
| `-R`                       | Like `-r`, but follow symlinks                                            |
| `-h`, `--help`            | Display help and exit                                                     |

### Exit status

- `0` — a match was found
- `1` — no match was found
- `2` — an error occurred

### Examples

```bash
modbox grep "TODO" src/
modbox grep -i "error" logfile.txt
modbox grep -n "main" *.c
modbox grep -r "deprecated" include/
modbox grep -E "^[A-Z][a-z]+$" names.txt
modbox grep -F "literal.dot" file.txt
modbox grep -c -r "pattern" project/
modbox grep -l "config" *.yaml
modbox grep -v "^\s*#" config.ini
modbox grep -w "test" file.c
modbox grep --color=always "func" main.go | less -R
echo "hello world" | modbox grep "world"
```

### Notes

- Default regex syntax is GRegex (PCRE-like), not strict POSIX BRE. Use `-E` for explicit ERE mode.
- Directories passed as FILEs require `-r`; otherwise grep reports `Is a directory` and skips them.
- For `-r`, hidden files are not skipped — the recursion descends into everything readable.
- Some GNU options are not implemented: `-A/-B/-C` context, `-P` PCRE, `-z`, `-b`, `-q`, `-s`, `-d`, `--include/--exclude`, `-m`, `--label`, `-Z`. See `./modbox grep --help` for the full list.

---

## help

Print the modbox command listing.

### Synopsis

```
modbox help
```

### Examples

```bash
modbox help
modbox cat --help
modbox ls --help
```

### Notes

- `help` ignores any arguments and always prints the modbox command summary.
- For per-command usage, prefer `modbox <command> --help`.

---

## ln

Create a link from SOURCE to DEST.

### Synopsis

```
modbox ln [OPTION]... SOURCE DEST
```

### Options

| Option                                | Description                                                                          |
| ------------------------------------- | ------------------------------------------------------------------------------------ |
| `-v`, `--verbose`                     | Print `'link' -> 'target'` after successful link                                     |
| `-f`, `--force`                       | Remove existing destination files first                                              |
| `-s`, `--symbolic`                    | Make symbolic links instead of hard links                                            |
| `-i`, `--interactive`                 | Prompt before removing destination (`y/N`)                                          |
| `-n`, `--no-dereference`              | Do not follow DEST if it is a symlink (treat it as the link target itself)           |
| `-L`, `--logical`                     | For hard links, dereference SOURCE if it is a symbolic link                          |
| `-h`, `--help`                        | Display help and exit                                                                |

### Examples

```bash
modbox ln source.txt link.txt
modbox ln -s /usr/local/bin/foo ~/bin/foo
modbox ln -sf old.txt new.txt
modbox ln -s ~/projects/myrepo repo
modbox ln -v important.cfg backup.cfg
modbox ln -L symlinked.txt hardlink.txt
```

### Notes

- **Hard links (default):** SOURCE must exist and be a regular file. Hard links to directories are not allowed.
- **Symbolic links (`-s`):** SOURCE may be any path — it does not need to exist. The link is just a string.
- If DEST is an existing directory, a link with the same basename as SOURCE is created inside it.
- `-L` resolves SOURCE through any intermediate symlinks before creating the hard link. Without `-L`, hard-linking a symlink typically fails because `link(2)` targets the symlink itself.
- `-n` prevents accidentally following a symlink at DEST (useful in scripts).

---

## ls

List directory contents. With no DIR, list the current directory.

### Synopsis

```
modbox ls [OPTION]... [DIR]...
```

### Display options

| Option                       | Description                                                            |
| ---------------------------- | ---------------------------------------------------------------------- |
| `-a`, `--all`                | Do not ignore entries starting with `.`                                |
| `-A`, `--almost-all`         | Do not list implied `.` and `..`                                       |
| `-l`, `--long`               | Use the long listing format                                            |
| `--author`                   | With `-l`, also print the author of each file                          |
| `-b`, `--escape`             | Print C-style escapes (`\ooo`) for non-graphic characters              |
| `-B`, `--ignore-backups`     | Do not list entries ending with `~`                                    |
| `-d`, `--directory`          | List directories themselves, not their contents                        |
| `-C`                         | List entries by columns (default for terminal output)                  |
| `-1`                         | List one file per line                                                 |
| `-F`, `--classify`           | Append indicator: `*` executable, `/` dir, `@` symlink                |
| `--colorful`                 | Multi-color output (eza/lsd style)                                     |
| `--icons`                    | Display icons before file names (lsd style)                            |

### Sorting

| Option                       | Description                                                |
| ---------------------------- | ---------------------------------------------------------- |
| `-r`, `--reverse`            | Reverse order when sorting                                 |
| `-U`                         | Do not sort; list entries in directory order               |

### Sizing

| Option                       | Description                                                                  |
| ---------------------------- | ---------------------------------------------------------------------------- |
| `--block-size=SIZE`          | Scale sizes by SIZE (e.g., `K` for KiB, `M` for MiB)                          |
| `--color=WHEN`               | Colorize output: `always`, `auto`, or `never`                                |

### Other

| Option                       | Description                                                |
| ---------------------------- | ---------------------------------------------------------- |
| `-h`, `--help`               | Display help and exit                                       |

### Examples

```bash
modbox ls
modbox ls -l
modbox ls -la
modbox ls -lh
modbox ls -lA
modbox ls -F
modbox ls -1
modbox ls -r
modbox ls -t -r
modbox ls -lh --block-size=K
modbox ls --color=always
modbox ls src/ include/
modbox ls -d */
modbox ls --icons --colorful
```

### Notes

- `-a` implies `-A` is ignored — both list hidden files.
- When output is not a terminal (e.g., piped), color is disabled unless `--color=always`.
- `--color` (with no value) is treated as `--color=always` for convenience.
- Multiple directory arguments are supported; each is listed in turn.

---

## mv

Move (rename) SOURCE to DEST, or multiple SOURCE(s) into DIRECTORY.

### Synopsis

```
modbox mv [OPTION]... SOURCE DEST
modbox mv [OPTION]... SOURCE... DIRECTORY
```

### Options

`mv` currently takes no flags beyond `-h, --help`. All options are positional.

| Option     | Description          |
| ---------- | -------------------- |
| `-h`       | Display help and exit |

### Examples

```bash
modbox mv old.txt new.txt
modbox mv file.txt /tmp/
modbox mv a.txt b.txt destdir/
modbox mv project/ /tmp/projects/
modbox mv *.log logs/
```

### Notes

- **Same filesystem:** `rename(2)` is used (atomic, instant).
- **Cross-filesystem:** Falls back to copy-then-unlink (`EXDEV` handling). Slower; preserves content but does not preserve hard-link count, sparse regions, or special files.
- Multiple SOURCEs require DEST to be an existing directory.
- Moving a directory into itself is rejected with: `mv: cannot move '...' to '...': Invalid argument`.
- `-i`, `-n`, `-f`, `-v` flags are not yet implemented.

---

## ptx

Generate a permuted index (KWIC - Key Word In Context) for files.

### Synopsis

```
modbox ptx [OPTION]... [FILE]...
```

### Options

| Option | Description |
| ------ | ----------- |
| `-A`, `--auto-reference` | Generate automatic references (filename:lineno) |
| `-R`, `--right-side-refs` | Put references on right side of output |
| `-G`, `--traditional` | Traditional mode (System V compatibility) |
| `-t`, `--typeset-mode` | Typeset mode |
| `-r`, `--references` | Use input references |
| `-w`, `--width=N` | Output width (default 72) |
| `-g`, `--gap-size=N` | Gap size between fields (default 3) |
| `-S`, `--sentence-regexp=REGEXP` | Sentence regular expression |
| `-b`, `--break-file=FILE` | Break file |
| `-i`, `--ignore-file=FILE` | Ignore file |
| `-o`, `--only-file=FILE` | Only file |
| `-h`, `--help` | Display help and exit |

### Examples

```bash
modbox ptx file.txt
modbox ptx -A file.txt
modbox ptx -w 80 file.txt
modbox ptx -R file.txt
echo "The quick brown fox" | modbox ptx
```

### Notes

- With no FILE, reads from standard input.
- Each word in the input becomes a keyword in the output, surrounded by its left and right context.
- Keywords are sorted alphabetically in the output.
- The `-A` option adds filename and line number references to each entry.

---

## rg

Recursively search files for PATTERN (ripgrep-style). Auto-recurses into directories.

### Synopsis

```
modbox rg [OPTION]... PATTERN [PATH]...
```

### Pattern selection

| Option                       | Description                                                          |
| ---------------------------- | -------------------------------------------------------------------- |
| `-e`, `--regexp=PATTERN`     | Use PATTERN as the pattern (use this for patterns starting with `-`) |
| `-F`, `--fixed-strings`      | Treat PATTERN as a literal string                                    |
| `-E`, `--extended-regexp`   | Treat PATTERN as extended regex                                      |

### Matching control

| Option                     | Description                                                                       |
| -------------------------- | --------------------------------------------------------------------------------- |
| `-i`, `--ignore-case`     | Case-insensitive matching                                                         |
| `-s`, `--case-sensitive`  | Force case-sensitive matching (overrides smart-case)                              |
| `-S`, `--smart-case`      | Case-insensitive when pattern is all lowercase (default)                          |
| `-v`, `--invert-match`    | Select non-matching lines                                                         |
| `-w`, `--word-regexp`     | Match only whole words                                                            |
| `-x`, `--line-regexp`     | Match only whole lines                                                            |

### Output control

| Option                              | Description                                                       |
| ----------------------------------- | ----------------------------------------------------------------- |
| `-n`, `--line-number`               | Show line numbers (default)                                       |
| `-N`, `--no-line-number`            | Suppress line numbers                                             |
| `-c`, `--count`                     | Print match count per file (with filename prefix)                 |
| `-l`, `--files-with-matches`        | Print only names of files containing matches                      |
| `-o`, `--only-matching`             | Print only the matched text (one match per output line)           |
| `--color=WHEN`                      | Highlight matches: `always`, `auto`, or `never`                   |

### Context

| Option                                | Description                                                    |
| ------------------------------------- | -------------------------------------------------------------- |
| `-C`, `--context=NUM`                 | Show NUM lines before and after each match                    |
| `-A`, `--after-context=NUM`           | Show NUM lines after each match                               |
| `-B`, `--before-context=NUM`          | Show NUM lines before each match                              |

`-A`/`-B` override `-C` on the respective side.

### Filtering

| Option                        | Description                                                              |
| ----------------------------- | ------------------------------------------------------------------------ |
| `-g`, `--glob=GLOB`           | Glob pattern to include or exclude files (prefix with `!` to exclude)    |
| `--hidden`                    | Search hidden files and directories (starting with `.`)                 |
| `--max-depth=NUM`             | Descend at most NUM directories deep (default: unlimited)               |
| `-m`, `--max-count=NUM`       | Stop after NUM matches per file                                          |

### Other

| Option     | Description          |
| ---------- | -------------------- |
| `-h`       | Display help and exit |

### Exit status

- `0` — a match was found
- `1` — no match was found
- `2` — an error occurred

### Examples

```bash
modbox rg "TODO" src/
modbox rg -i "error" project/
modbox rg -n "main" --max-depth=2 .
modbox rg -F "literal.dot" file.txt
modbox rg -E "^[A-Z][a-z]+$" names.txt
modbox rg -w "test" *.c
modbox rg -C 2 "function" src/main.c
modbox rg -A 1 -B 1 "TODO" changelog.md
modbox rg -c "import" src/
modbox rg -l "config" docs/
modbox rg -o '\$\{[^}]+\}' template.txt
modbox rg --hidden "secret" home/
modbox rg -g '!*.min.js' "function" web/
modbox rg -g '*.md' -g '!README*' "guide" docs/
modbox rg -m 5 "FIXME" *.c
modbox rg --color=always "pattern" file.txt | less -R
```

### Notes

- Recursion is automatic when a PATH argument is a directory. No `-r` flag is required (and none is accepted).
- Smart-case is on by default: lowercase patterns match case-insensitively, mixed-case patterns match case-sensitively. `-S` makes this explicit; `-i` forces insensitive; `-s` forces sensitive.
- `--color` (no value) is treated as `--color=always` for convenience.
- Glob patterns use GPatternSpec (fnmatch-style). `!` prefix excludes; if only exclude patterns are given, all non-matching files are included.
- Context lines are marked with `-` after the line number; match lines have no suffix.
- `-c` and `-l` are mutually exclusive in spirit: `-l` prints a file at most once and short-circuits scanning after the first match.
---

## find

Search for files in a directory hierarchy. With no starting point, searches the current directory.

### Synopsis

```
modbox find [starting-point...] [expression]
```

### Predicates

| Option               | Description                                               |
| -------------------- | --------------------------------------------------------- |
| `-name PATTERN`      | Shell glob pattern match on filename                      |
| `-iname PATTERN`     | Case-insensitive `-name`                                  |
| `-type [fdl]`        | File is of type `f` (regular), `d` (directory), `l` (symlink) |
| `-empty`             | File is empty (zero-byte regular file or empty directory)  |

### Numeric options

| Option               | Description                                               |
| -------------------- | --------------------------------------------------------- |
| `-maxdepth N`        | Descend at most N levels below starting points            |
| `-mindepth N`        | Do not apply tests/actions at levels less than N          |

### Actions

| Option               | Description                                               |
| -------------------- | --------------------------------------------------------- |
| `-print`             | Print the file path (default if no action specified)      |
| `-delete`            | Delete file or empty directory                            |
| `-exec cmd {} ;`     | Execute command once per matching file (`{}` replaced)     |
| `-exec cmd {} +`     | Execute command with all matching files at once            |

### Examples

```bash
modbox find -name "*.c"
modbox find . -type f -name "*.txt"
modbox find /tmp -empty -delete
modbox find . -type f -exec wc -l {} \;
modbox find . -name "*.log" -exec rm {} +
modbox find -maxdepth 2 -type d -name "test*"
```

### Notes

- Predicates are ANDed together (all must match). No `-o`/`-or` operator is implemented yet.
- If no expression is given, `-print` is assumed for all entries.
- If no starting point is given, the current directory `.` is used.
- `-delete` uses `unlink()` for files and `rmdir()` for directories (only empty dirs).

---

## Quick reference

| Task                                | Command                                                        |
| ----------------------------------- | -------------------------------------------------------------- |
| Print a file                        | `modbox cat file`                                              |
| Number lines                        | `modbox cat -n file`                                           |
| Show non-printing chars             | `modbox cat -A file`                                           |
| View long file with paging          | `modbox cat --less big.log`                                    |
| Search files by name/type           | `modbox find -name "*.c"`                                     |
| Search recursively by type          | `modbox find . -type f -name "*.txt"`                         |
| Delete empty files/directories      | `modbox find . -empty -delete`                                |
| Execute command on found files      | `modbox find . -type f -exec wc -l {} ;`                      |
| Copy file                           | `modbox cp src dst`                                            |
| Copy a directory                    | `modbox cp -r src/ dst/`                                       |
| Move/rename                         | `modbox mv src dst`                                            |
| Hard link                           | `modbox ln src link`                                           |
| Symlink                             | `modbox ln -s target link`                                     |
| List directory                      | `modbox ls`                                                    |
| Long listing                        | `modbox ls -l`                                                 |
| List including hidden               | `modbox ls -a`                                                 |
| Sorted by time, newest first        | `modbox ls -lt` (combined with `-r` if needed)                 |
| Find a string in a file             | `modbox grep PATTERN file`                                     |
| Recursive grep                      | `modbox grep -r PATTERN dir/`                                  |
| Recursive search w/ context         | `modbox rg -C 3 PATTERN dir/`                                  |
| Recursive search with glob filter   | `modbox rg -g '*.c' PATTERN dir/`                              |
| Show help for a command             | `modbox <command> --help`                                      |
