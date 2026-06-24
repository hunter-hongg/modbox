# fd Command Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an `fd` command to modbox that implements the fd-find file search interface.

**Architecture:** Same pattern as `rg.c` — argtable3 for option parsing, recursive `opendir`/`readdir` walk, `GRegex`/`GPatternSpec` for pattern matching, fork/exec for `-x`/`-X`.

**Tech Stack:** C, GLib, argtable3, POSIX APIs (opendir, readdir, stat, fork, execvp)

---

### Task 1: Create header file `include/commands/fd.h`

**Files:**
- Create: `include/commands/fd.h`

- [ ] **Step 1: Write the header**

```c
#ifndef FD_H
#define FD_H

#include <glib.h>

typedef enum {
    FD_COLOR_NEVER,
    FD_COLOR_ALWAYS,
    FD_COLOR_AUTO
} fd_color_t;

typedef struct {
    int hidden;
    int no_ignore;
    int case_sensitive;
    int ignore_case;
    int smart_case;
    int glob_mode;
    int full_path;
    int follow;
    int print0;
    int max_depth;
    int max_results;
    fd_color_t color_mode;
    char type_filter;
    gchar *pattern;
    GPtrArray *extensions;
    GPtrArray *exclude;
    int has_exec;
    int exec_batch;
    GPtrArray *exec_args;
    GPtrArray *exec_paths;
} FdOptions;

void fd_command(gint argc, gchar **argv);

#endif
```

- [ ] **Step 2: Verify it compiles (header-only check, no .c yet)**

Run: `gcc -fsyntax-only -Iinclude include/commands/fd.h`
Expected: no errors

- [ ] **Step 3: Commit**

```bash
git add include/commands/fd.h
git commit -m "feat: add fd command header"
```

---

### Task 2: Register fd command in `src/main.c`

**Files:**
- Modify: `src/main.c`

- [ ] **Step 1: Add include and hash table entry**

Insert `#include "commands/fd.h"` at line 15 (after `#include "commands/find.h"`):
```c
#include "commands/fd.h"
```

Insert `g_hash_table_insert(commands, "fd", (gpointer)fd_command);` after the `find` entry at line 44:
```c
    g_hash_table_insert(commands, "fd", (gpointer)fd_command);
```

- [ ] **Step 2: Verify build still works**

Run: `make compile 2>&1 | head -5`
Expected: link fails because `fd_command` is undefined (no .c yet) — that's fine, next task provides it

- [ ] **Step 3: Commit**

```bash
git add src/main.c
git commit -m "feat: register fd command in main dispatch"
```

---

### Task 3: Implement core fd.c — helpers, walk, filtering, output

**Files:**
- Create: `src/commands/fd.c`

This is the main implementation file. All helper functions, directory walking, pattern matching, filtering, and output go here. Exec support comes in Task 4.

- [ ] **Step 1: Write fd.c with includes, constants, and helper functions**

```c
#include <argtable3.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/fd.h"

#define FD_MAX_ARGS 200

static int is_pattern_lowercase(const gchar *pattern) {
    for (const gchar *p = pattern; *p; p++) {
        if (g_ascii_isupper((gchar)*p)) {
            return 0;
        }
    }
    return 1;
}

static int is_hidden(const gchar *name) {
    return name[0] == '.';
}

static int match_type(mode_t mode, char type_filter) {
    switch (type_filter) {
    case 'f': return S_ISREG(mode);
    case 'd': return S_ISDIR(mode);
    case 'l': return S_ISLNK(mode);
    case 'x': return (mode & 0111) != 0;
    case 's': return S_ISSOCK(mode);
    default:  return 1;
    }
}

static int is_empty_file(const gchar *path, mode_t mode) {
    if (S_ISREG(mode)) {
        struct stat st;
        if (stat(path, &st) != 0) return 0;
        return st.st_size == 0;
    }
    if (S_ISDIR(mode)) {
        DIR *dir = opendir(path);
        if (dir == NULL) return 0;
        int empty = 1;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                empty = 0;
                break;
            }
        }
        (void)closedir(dir);
        return empty;
    }
    return 0;
}

static int match_extension(const gchar *name, GPtrArray *exts) {
    if (exts == NULL || exts->len == 0) return 1;
    const gchar *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    const gchar *ext = dot + 1;
    for (guint i = 0; i < exts->len; i++) {
        if (strcmp(ext, (const gchar *)g_ptr_array_index(exts, i)) == 0) {
            return 1;
        }
    }
    return 0;
}

static int match_exclude(const gchar *path, const gchar *name, GPtrArray *excludes) {
    if (excludes == NULL || excludes->len == 0) return 0;
    for (guint i = 0; i < excludes->len; i++) {
        const gchar *pat = (const gchar *)g_ptr_array_index(excludes, i);
        GPatternSpec *spec = g_pattern_spec_new(pat);
        int m = (int)g_pattern_spec_match_string(spec, path) ||
                (int)g_pattern_spec_match_string(spec, name);
        g_pattern_spec_free(spec);
        if (m) return 1;
    }
    return 0;
}

static int fd_should_ci(const FdOptions *opts) {
    if (opts->case_sensitive) return 0;
    if (opts->ignore_case) return 1;
    if (opts->smart_case) return is_pattern_lowercase(opts->pattern);
    return 0;
}
```

- [ ] **Step 2: Add fd_walk() function**

```c
// NOLINTNEXTLINE(misc-no-recursion)
static int fd_walk(const gchar *dirpath, FdOptions *opts, GRegex *re,
                    int is_ci, int is_glob, int depth) {
    if (opts->max_depth >= 0 && depth > opts->max_depth) return 0;

    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        (void)fprintf(stderr, "fd: %s: %s\n", dirpath, strerror(errno));
        return 0;
    }

    int match_count = 0;
    int use_color = 0;
    if (opts->color_mode == FD_COLOR_ALWAYS ||
        (opts->color_mode == FD_COLOR_AUTO && isatty(STDOUT_FILENO))) {
        use_color = 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (opts->max_results > 0 && match_count >= opts->max_results) break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (!opts->hidden && is_hidden(entry->d_name)) continue;

        gchar *full_path = g_build_filename(dirpath, entry->d_name, NULL);

        struct stat st;
        int rc = opts->follow ? stat(full_path, &st) : lstat(full_path, &st);
        if (rc != 0) {
            g_free(full_path);
            continue;
        }

        if (match_exclude(full_path, entry->d_name, opts->exclude)) {
            g_free(full_path);
            continue;
        }

        int matches_type = 1;
        if (opts->type_filter != 0 && !S_ISDIR(st.st_mode)) {
            matches_type = match_type(st.st_mode, opts->type_filter);
        }
        if (opts->type_filter == 'e') {
            matches_type = is_empty_file(full_path, st.st_mode);
        }

        int matches_ext = 1;
        if (!S_ISDIR(st.st_mode)) {
            matches_ext = match_extension(entry->d_name, opts->extensions);
        }

        int pattern_match = 0;
        if (matches_type && matches_ext) {
            const gchar *target = opts->full_path ? full_path : entry->d_name;
            if (is_glob) {
                if (is_ci) {
                    gchar *lower_target = g_utf8_strdown(target, -1);
                    gchar *lower_pat = g_utf8_strdown(opts->pattern, -1);
                    GPatternSpec *spec = g_pattern_spec_new(lower_pat);
                    pattern_match = (int)g_pattern_spec_match_string(spec, lower_target);
                    g_pattern_spec_free(spec);
                    g_free(lower_target);
                    g_free(lower_pat);
                } else {
                    GPatternSpec *spec = g_pattern_spec_new(opts->pattern);
                    pattern_match = (int)g_pattern_spec_match_string(spec, target);
                    g_pattern_spec_free(spec);
                }
            } else {
                pattern_match = (int)g_regex_match(re, target, (GRegexMatchFlags)0, NULL);
            }
        }

        if (pattern_match) {
            match_count++;
            /* Output handled in caller (print or exec) */
            if (!opts->has_exec) {
                if (opts->print0) {
                    printf("%s%c", full_path, '\0');
                } else if (use_color) {
                    if (S_ISDIR(st.st_mode)) {
                        printf("\033[01;34m%s\033[0m\n", full_path);
                    } else if (S_ISLNK(st.st_mode)) {
                        printf("\033[01;36m%s\033[0m\n", full_path);
                    } else if (st.st_mode & 0111) {
                        printf("\033[01;32m%s\033[0m\n", full_path);
                    } else {
                        printf("%s\n", full_path);
                    }
                } else {
                    printf("%s\n", full_path);
                }
            }
        }

        if (S_ISDIR(st.st_mode)) {
            match_count += fd_walk(full_path, opts, re, is_ci, is_glob, depth + 1);
        }

        g_free(full_path);
    }

    (void)closedir(dir);
    return match_count;
}
```

- [ ] **Step 3: Add fd_command() entry point (arg parsing + setup)**

```c
// NOLINTNEXTLINE(readability-function-size)
void fd_command(gint argc, gchar **argv) {
    FdOptions opts = {0};
    opts.smart_case = 1;
    opts.max_depth = -1;
    opts.color_mode = FD_COLOR_AUTO;

    struct arg_lit *hidden_opt = arg_lit0("H", "hidden", "search hidden files and directories");
    struct arg_lit *no_ignore_opt = arg_lit0("I", "no-ignore", "do not respect .gitignore files");
    struct arg_lit *case_sensitive_opt = arg_lit0("s", "case-sensitive", "case-sensitive search (default: smart-case)");
    struct arg_lit *ignore_case_opt = arg_lit0("i", "ignore-case", "case-insensitive search");
    struct arg_lit *glob_opt = arg_lit0("g", "glob", "glob-based search (default: regex)");
    struct arg_lit *full_path_opt = arg_lit0("p", "full-path", "search full path (default: basename only)");
    struct arg_lit *follow_opt = arg_lit0("L", "follow", "follow symbolic links");
    struct arg_lit *print0_opt = arg_lit0("0", "print0", "separate results by NUL character");
    struct arg_int *max_depth_opt = arg_int0("d", "max-depth", "DEPTH", "maximum search depth");
    struct arg_int *max_results_opt = arg_int0(NULL, "max-results", "NUM", "limit number of search results");
    struct arg_str *type_opt = arg_str0("t", "type", "TYPE", "filter by type: f(ile), d(irectory), l(symlink), x(executable), e(mpty), s(socket)");
    struct arg_str *extension_opt = arg_strn("e", "extension", "EXT", 0, FD_MAX_ARGS, "filter by file extension");
    struct arg_str *exclude_opt = arg_strn("E", "exclude", "PATTERN", 0, FD_MAX_ARGS, "exclude entries matching glob pattern");
    struct arg_str *exec_opt = arg_strn("x", "exec", "CMD", 0, FD_MAX_ARGS, "execute command for each search result");
    struct arg_str *exec_batch_opt = arg_strn("X", "exec-batch", "CMD", 0, FD_MAX_ARGS, "execute command with all search results at once");
    struct arg_str *color_opt = arg_str0(NULL, "color", "WHEN", "when to use colors: never, auto, always");
    struct arg_lit *help_opt = arg_lit0("h", "help", "print help");
    struct arg_file *pattern_arg = arg_filen(NULL, NULL, "PATTERN", 1, 1, "search pattern");
    struct arg_file *path_arg = arg_filen(NULL, NULL, "PATH", 0, FD_MAX_ARGS, "root directory for search");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        hidden_opt, no_ignore_opt,
        case_sensitive_opt, ignore_case_opt,
        glob_opt, full_path_opt, follow_opt,
        print0_opt, max_depth_opt, max_results_opt,
        type_opt, extension_opt, exclude_opt,
        exec_opt, exec_batch_opt,
        color_opt, help_opt, pattern_arg, path_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTIONS] PATTERN [PATH...]\n", argv[0]);
        printf("Search for files in a directory hierarchy (like fd-find).\n");
        printf("\n");
        printf("Pattern selection:\n");
        printf("  PATTERN            search pattern (regex, unless -g is given)\n");
        printf("\n");
        printf("Matching control:\n");
        printf("  -s, --case-sensitive      case-sensitive (default: smart-case)\n");
        printf("  -i, --ignore-case         case-insensitive\n");
        printf("  -g, --glob                glob-based search (default: regex)\n");
        printf("  -p, --full-path           search full path, not just basename\n");
        printf("\n");
        printf("Filtering:\n");
        printf("  -t, --type TYPE           filter by type: f,d,l,x,e,s\n");
        printf("  -e, --extension EXT       filter by file extension\n");
        printf("  -E, --exclude PATTERN     exclude entries matching glob pattern\n");
        printf("  -H, --hidden              search hidden files and directories\n");
        printf("  -I, --no-ignore           do not respect .gitignore (no-op)\n");
        printf("  -L, --follow              follow symbolic links\n");
        printf("  -d, --max-depth DEPTH     maximum search depth\n");
        printf("      --max-results NUM     limit number of search results\n");
        printf("\n");
        printf("Output control:\n");
        printf("  -0, --print0              separate results by NUL character\n");
        printf("      --color WHEN          when to use colors: never, auto, always\n");
        printf("\n");
        printf("Execution:\n");
        printf("  -x, --exec CMD            execute command for each result\n");
        printf("  -X, --exec-batch CMD      execute command with all results at once\n");
        printf("\n");
        printf("Exit status:\n");
        printf("  0  if a match is found\n");
        printf("  1  if no match was found\n");
        printf("  2  if an error occurred\n");
        printf("\n");
        printf("Notes:\n");
        printf("  .gitignore files are not parsed; -I is a no-op.\n");
        printf("  Hidden files/dirs are excluded by default; use -H to include.\n");
        printf("  For exec (-x), use {} to substitute the file path.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        exit(2);
    }

    opts.hidden = (hidden_opt->count > 0);
    opts.no_ignore = (no_ignore_opt->count > 0);
    opts.case_sensitive = (case_sensitive_opt->count > 0);
    opts.ignore_case = (ignore_case_opt->count > 0);
    opts.glob_mode = (glob_opt->count > 0);
    opts.full_path = (full_path_opt->count > 0);
    opts.follow = (follow_opt->count > 0);
    opts.print0 = (print0_opt->count > 0);

    if (opts.case_sensitive || opts.ignore_case) {
        opts.smart_case = 0;
    }

    if (max_depth_opt->count > 0) {
        opts.max_depth = max_depth_opt->ival[0];
    }
    if (max_results_opt->count > 0) {
        opts.max_results = max_results_opt->ival[0];
    }

    if (type_opt->count > 0) {
        const char *tv = type_opt->sval[0];
        if (tv[0] && tv[1] == '\0' &&
            strchr("fdlxes", tv[0])) {
            opts.type_filter = tv[0];
        } else {
            (void)fprintf(stderr, "fd: invalid type '%s' (valid: f,d,l,x,e,s)\n", tv);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            exit(2);
        }
    }

    if (extension_opt->count > 0) {
        opts.extensions = g_ptr_array_new();
        for (int i = 0; i < extension_opt->count; i++) {
            g_ptr_array_add(opts.extensions, (gpointer)g_strdup(extension_opt->sval[i]));
        }
    }

    if (exclude_opt->count > 0) {
        opts.exclude = g_ptr_array_new();
        for (int i = 0; i < exclude_opt->count; i++) {
            g_ptr_array_add(opts.exclude, (gpointer)g_strdup(exclude_opt->sval[i]));
        }
    }

    if (exec_opt->count > 0) {
        opts.has_exec = 1;
        opts.exec_args = g_ptr_array_new();
        for (int i = 0; i < exec_opt->count; i++) {
            g_ptr_array_add(opts.exec_args, (gpointer)g_strdup(exec_opt->sval[i]));
        }
        opts.exec_paths = g_ptr_array_new_with_free_func(g_free);
    }

    if (exec_batch_opt->count > 0) {
        opts.has_exec = 1;
        opts.exec_batch = 1;
        opts.exec_args = g_ptr_array_new();
        for (int i = 0; i < exec_batch_opt->count; i++) {
            g_ptr_array_add(opts.exec_args, (gpointer)g_strdup(exec_batch_opt->sval[i]));
        }
        opts.exec_paths = g_ptr_array_new_with_free_func(g_free);
    }

    if (color_opt->count > 0) {
        const char *cv = color_opt->sval[0];
        if (strcmp(cv, "never") == 0) {
            opts.color_mode = FD_COLOR_NEVER;
        } else if (strcmp(cv, "always") == 0) {
            opts.color_mode = FD_COLOR_ALWAYS;
        } else if (strcmp(cv, "auto") == 0) {
            opts.color_mode = FD_COLOR_AUTO;
        } else {
            (void)fprintf(stderr, "fd: invalid color '%s' (valid: never, auto, always)\n", cv);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            exit(2);
        }
    }

    opts.pattern = g_strdup(pattern_arg->filename[0]);

    int is_ci = fd_should_ci(&opts);
    int is_glob = opts.glob_mode;

    GRegex *re = NULL;
    GError *re_error = NULL;
    if (!is_glob) {
        GRegexCompileFlags flags = G_REGEX_OPTIMIZE;
        if (is_ci) {
            flags |= G_REGEX_CASELESS;
        }
        re = g_regex_new(opts.pattern, flags, (GRegexMatchFlags)0, &re_error);
        if (re == NULL) {
            (void)fprintf(stderr, "fd: invalid pattern '%s': %s\n",
                          opts.pattern,
                          re_error ? re_error->message : "unknown error");
            if (re_error) g_error_free(re_error);
            g_free(opts.pattern);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            exit(2);
        }
    }

    int total_matches = 0;
    int has_paths = (path_arg->count > 0);

    if (!has_paths) {
        total_matches = fd_walk(".", &opts, re, is_ci, is_glob, 0);
    } else {
        for (int i = 0; i < path_arg->count; i++) {
            const gchar *p = path_arg->filename[i];
            struct stat st;
            if (stat(p, &st) != 0) {
                (void)fprintf(stderr, "fd: %s: %s\n", p, strerror(errno));
                continue;
            }
            if (S_ISDIR(st.st_mode)) {
                total_matches += fd_walk(p, &opts, re, is_ci, is_glob, 0);
            } else {
                (void)fprintf(stderr, "fd: %s: is not a directory\n", p);
            }
        }
    }

    if (re) g_regex_unref(re);
    g_free(opts.pattern);
    if (opts.extensions) g_ptr_array_free(opts.extensions, TRUE);
    if (opts.exclude) g_ptr_array_free(opts.exclude, TRUE);
    if (opts.exec_args) g_ptr_array_free(opts.exec_args, TRUE);
    if (opts.exec_paths) g_ptr_array_free(opts.exec_paths, TRUE);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (total_matches >= 0) {
        exit(total_matches > 0 ? 0 : 1);
    }
    exit(2);
}
```

- [ ] **Step 4: Build and verify compilation**

Run: `make compile 2>&1 | tail -10`
Expected: `cmake --build build` succeeds, `target/modbox` exists

- [ ] **Step 5: Quick smoke test**

Run: `./target/modbox fd '[a-z]+' 2>&1 || true`
Expected: some output from current directory, no crash

- [ ] **Step 6: Commit**

```bash
git add src/commands/fd.c
git commit -m "feat: implement fd command core (walk, filter, output)"
```

---

### Task 4: Implement exec support in fd.c

**Files:**
- Modify: `src/commands/fd.c`

The walk function currently skips exec. Need to add `fd_exec_file` and `fd_exec_finalize` and wire them in.

- [ ] **Step 1: Add exec helper functions before fd_command()**

```c
static void fd_exec_file(const gchar *fullpath, FdOptions *opts) {
    if (opts->exec_batch) {
        g_ptr_array_add(opts->exec_paths, g_strdup(fullpath));
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        GPtrArray *args = g_ptr_array_new();
        int has_subst = 0;
        for (guint i = 0; i < opts->exec_args->len; i++) {
            const gchar *a = (const gchar *)g_ptr_array_index(opts->exec_args, i);
            if (strcmp(a, "{}") == 0) {
                g_ptr_array_add(args, (gpointer)fullpath);
                has_subst = 1;
            } else {
                g_ptr_array_add(args, (gpointer)a);
            }
        }
        if (!has_subst) {
            g_ptr_array_add(args, (gpointer)fullpath);
        }
        g_ptr_array_add(args, NULL);
        (void)execvp((const gchar *)g_ptr_array_index(args, 0),
                      (char *const *)args->pdata);
        (void)fprintf(stderr, "fd: %s: %s\n",
                      (const gchar *)g_ptr_array_index(args, 0), strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        int status;
        (void)waitpid(pid, &status, 0);
    } else {
        (void)fprintf(stderr, "fd: fork failed: %s\n", strerror(errno));
    }
}

static void fd_exec_finalize(FdOptions *opts) {
    if (!opts->has_exec || !opts->exec_batch || opts->exec_paths->len == 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        GPtrArray *args = g_ptr_array_new();
        int has_subst = 0;
        for (guint i = 0; i < opts->exec_args->len; i++) {
            const gchar *a = (const gchar *)g_ptr_array_index(opts->exec_args, i);
            if (strcmp(a, "{}") == 0) {
                for (guint j = 0; j < opts->exec_paths->len; j++) {
                    g_ptr_array_add(args, g_ptr_array_index(opts->exec_paths, j));
                }
                has_subst = 1;
            } else {
                g_ptr_array_add(args, (gpointer)a);
            }
        }
        if (!has_subst) {
            for (guint j = 0; j < opts->exec_paths->len; j++) {
                g_ptr_array_add(args, g_ptr_array_index(opts->exec_paths, j));
            }
        }
        g_ptr_array_add(args, NULL);
        (void)execvp((const gchar *)g_ptr_array_index(args, 0),
                      (char *const *)args->pdata);
        (void)fprintf(stderr, "fd: %s: %s\n",
                      (const gchar *)g_ptr_array_index(args, 0), strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        int status;
        (void)waitpid(pid, &status, 0);
    } else {
        (void)fprintf(stderr, "fd: fork failed: %s\n", strerror(errno));
    }
}
```

- [ ] **Step 2: Wire exec into the walk function**

The walk function (from Task 3) already skips printing when `opts->has_exec` is set.
Add the exec call after `match_count++;` in the pattern-match block:

```c
        if (pattern_match) {
            match_count++;
            if (opts->has_exec) {
                fd_exec_file(full_path, opts);
            }
            if (!opts->has_exec) {
                // existing print logic ...
            }
        }
```

- [ ] **Step 3: Add exec_finalize call after walk loop in fd_command()**

In `fd_command()`, after the walk loop:
```c
fd_exec_finalize(&opts);
```

- [ ] **Step 4: Build and smoke test**

Run: `make compile 2>&1 | tail -5`
Expected: build succeeds
Run: `./target/modbox fd -x echo '.*\.c' 2>&1 || true`
Expected: lists .c files via echo (printed by echo, not fd)

- [ ] **Step 5: Commit**

```bash
git add src/commands/fd.c
git commit -m "feat: add exec support (-x, -X) to fd command"
```

---

### Task 5: Add tests

**Files:**
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Add fd test section before the Summary section**

Insert after the find tests (after line 1379) and before the Summary section (line 1381):

```bash
# ═══════════════════════════════════════════════════════════════════════════
#  fd
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── fd ──────────────────────────────────────"

mkdir -p "$TMPDIR"/fd_dir/sub
printf 'hello world\n'  > "$TMPDIR"/fd_dir/hello.txt
printf 'stuff\n'        > "$TMPDIR"/fd_dir/Hello.txt
printf 'fn main() {}\n' > "$TMPDIR"/fd_dir/main.rs
printf 'data\n'         > "$TMPDIR"/fd_dir/data.rs
printf 'nested content\n' > "$TMPDIR"/fd_dir/sub/nested.txt
printf 'empty file\n'   > "$TMPDIR"/fd_dir/sub/empty.txt
: > "$TMPDIR"/fd_dir/empty_file.txt
touch "$TMPDIR"/fd_dir/.hidden_file
chmod +x "$TMPDIR"/fd_dir/exec_script.sh 2>/dev/null || \
    printf '#!/bin/sh\n' > "$TMPDIR"/fd_dir/exec_script.sh && \
    chmod +x "$TMPDIR"/fd_dir/exec_script.sh

echo "  ── basic pattern search ──"
assert_cmd_pat 'hello\.txt' fd hello "$TMPDIR"/fd_dir

echo "  ── case-sensitive (-s) ──"
assert_cmd 'Hello.txt' fd -s Hello "$TMPDIR"/fd_dir
assert_cmd_pat 'hello\.txt' fd -s hello "$TMPDIR"/fd_dir
assert_cmd_not_pat 'Hello' fd -s hello "$TMPDIR"/fd_dir

echo "  ── case-insensitive (-i) ──"
assert_cmd_pat 'hello' fd -i HELLO "$TMPDIR"/fd_dir

echo "  ── smart-case (default): lowercase pat = case-insensitive ──"
assert_cmd_pat 'hello' fd hello "$TMPDIR"/fd_dir
assert_cmd_pat 'Hello' fd hello "$TMPDIR"/fd_dir

echo "  ── smart-case: uppercase pat = case-sensitive ──"
assert_cmd 'Hello.txt' fd Hello "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello' fd Hello "$TMPDIR"/fd_dir

echo "  ── glob mode (-g) ──"
assert_cmd_pat 'main\.rs' fd -g '*.rs' "$TMPDIR"/fd_dir

echo "  ── -H: hidden files ──"
assert_cmd_pat 'hidden_file' fd -H hidden "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hidden_file' fd hidden "$TMPDIR"/fd_dir

echo "  ── -t f: files only ──"
# Should find files, not directories like sub
results_f=$("$MODBOX" fd -t f '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
results_all=$("$MODBOX" fd '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
if [ "$results_f" -gt 0 ] && [ "$results_f" -lt "$results_all" ]; then
    pass "fd -t f → $results_f files (vs $results_all total)"
else
    fail "fd -t f — expected files only, got $results_f (total $results_all)"
fi

echo "  ── -t d: directories only ──"
assert_cmd_pat 'fd_dir/sub$' fd -t d 'sub' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello\.txt' fd -t d '' "$TMPDIR"/fd_dir

echo "  ── -t l: symlinks ──"
ln -sf "$TMPDIR"/fd_dir/hello.txt "$TMPDIR"/fd_dir/link.ln
assert_cmd_pat 'link\.ln' fd -t l 'link' "$TMPDIR"/fd_dir

echo "  ── -t x: executables ──"
assert_cmd_pat 'exec_script' fd -t x 'exec' "$TMPDIR"/fd_dir

echo "  ── -t e: empty files ──"
assert_cmd_pat 'empty_file' fd -t e 'empty' "$TMPDIR"/fd_dir

echo "  ── -e extension filter ──"
assert_cmd_pat 'main\.rs' fd -e rs 'main' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello\.txt' fd -e rs '' "$TMPDIR"/fd_dir

echo "  ── -E exclude pattern ──"
assert_cmd_not_pat 'sub' fd -E 'sub' '' "$TMPDIR"/fd_dir 2>/dev/null

echo "  ── -d max depth ──"
assert_cmd_pat 'hello\.txt' fd -d 1 'txt' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'nested\.txt' fd -d 1 'nested' "$TMPDIR"/fd_dir
assert_cmd_pat 'nested\.txt' fd -d 2 'nested' "$TMPDIR"/fd_dir

echo "  ── -p full path search ──"
assert_cmd_pat 'fd_dir/hello' fd -p 'fd_dir/' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'fd_dir/hello' fd 'fd_dir/' "$TMPDIR"/fd_dir

echo "  ── -0 print0 ──"
# print0 should produce NUL-separated output; use tr to verify
has_nul=$("$MODBOX" fd -0 'hello' "$TMPDIR"/fd_dir 2>/dev/null | tr -d '[:print:]' | grep -c $'\x00' || true)
if [ "$has_nul" -gt 0 ]; then
    pass "fd -0 → output contains NUL chars"
else
    fail "fd -0 — expected NUL-separated output"
fi

echo "  ── --color=always ──"
assert_cmd_pat $'\x1b\[' fd --color=always '.' "$TMPDIR"/fd_dir

echo "  ── --color=always has ANSI codes ──"
OUTPUT=$("$MODBOX" fd --color=always '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    pass "fd --color=always → ANSI codes present"
else
    fail "fd --color=always — expected ANSI codes"
fi

echo "  ── --color=auto (piped) no ANSI ──"
OUTPUT=$("$MODBOX" fd --color=auto '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    fail "fd --color=auto (piped) — expected no ANSI codes, got them"
else
    pass "fd --color=auto (piped) — ANSI codes correctly disabled"
fi

echo "  ── --color=never ──"
OUTPUT=$("$MODBOX" fd --color=never '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    fail "fd --color=never — expected no ANSI codes, got them"
else
    pass "fd --color=never — no ANSI codes"
fi

echo "  ── --max-results N ──"
line_count=$("$MODBOX" fd --max-results 3 '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
if [ "$line_count" -le 3 ]; then
    pass "fd --max-results 3 → $line_count lines (≤3)"
else
    fail "fd --max-results 3 → $line_count lines, expected ≤3"
fi

echo "  ── multiple paths ──"
assert_cmd_pat 'hello' fd hello "$TMPDIR"/fd_dir "$TMPDIR"

echo "  ── error: no pattern ──"
if "$MODBOX" fd >/dev/null 2>&1; then
    fail "fd (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "fd (no args) → exit 2"
    else
        fail "fd (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── error: invalid type ──"
assert_cmd_pat_stderr 'invalid type' fd -t z '.' /dev/null

echo "  ── exit code 0 on match ──"
if "$MODBOX" fd hello "$TMPDIR"/fd_dir >/dev/null 2>&1; then
    pass "fd hello → exit 0"
else
    fail "fd hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" fd nonexistentzzz "$TMPDIR"/fd_dir >/dev/null 2>&1; then
    fail "fd nonexistent → expected exit 1"
else
    pass "fd nonexistent → exit 1"
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' fd --help

echo "  ── -x exec per file (echo) ──"
result=$("$MODBOX" fd -x echo '\.rs$' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$result" | grep -qE '\.rs$'; then
    pass "fd -x echo '.rs' → exec output contains .rs paths"
else
    fail "fd -x echo '.rs' — expected .rs paths in output, got [$result]"
fi

echo "  ── -X exec batch (echo) ──"
result=$("$MODBOX" fd -X echo '\.rs$' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$result" | grep -qE '\.rs'; then
    pass "fd -X echo '.rs' → batch exec output contains .rs paths"
else
    fail "fd -X echo '.rs' — expected .rs paths in output, got [$result]"
fi

echo "  ── -L follow symlinks ──"
mkdir -p "$TMPDIR"/fd_follow/sub
printf 'followed\n' > "$TMPDIR"/fd_follow/sub/target.txt
ln -sf "$TMPDIR"/fd_follow "$TMPDIR"/fd_follow_link
assert_cmd_pat 'target' fd -L 'target' "$TMPDIR"/fd_follow_link
```

- [ ] **Step 2: Run the tests**

Run: `bash tests/run_tests.sh 2>&1 | tail -80`
Expected: All fd tests pass (PASS), any FAILS need investigation

- [ ] **Step 3: Commit**

```bash
git add tests/run_tests.sh
git commit -m "test: add fd command tests"
```

---

### Self-Review Checklist

- [ ] **Spec coverage:** All spec features covered: pattern search, glob, case control, hidden files, type filter, extension filter, exclude, max depth, full path, print0, max results, color modes, exec, follow, help, error codes.
- [ ] **Placeholder scan:** No TBD, TODO, or placeholder code in any task.
- [ ] **Type consistency:** FdOptions fields match between header and implementation. `fd_walk` signature consistent.
- [ ] **No missing imports:** GLib, argtable3, POSIX headers all included.
