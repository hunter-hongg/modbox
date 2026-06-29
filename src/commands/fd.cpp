#include <argtable3.h>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fnmatch.h>
#include <climits>
#include <algorithm>
#include <string>
#include <vector>
#include <regex>

#include "commands/fd.hpp"

#define FD_MAX_ARGS 200

static int is_pattern_lowercase(const char *pattern) {
    for (const char *p = pattern; *p; p++) {
        if (isupper((unsigned char)*p)) {
            return 0;
        }
    }
    return 1;
}

static int is_hidden(const char *name) {
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

static int is_empty_file(const char *path, mode_t mode, const struct stat *st_in) {
    if (S_ISREG(mode)) {
        return st_in ? (st_in->st_size == 0) : 0;
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

static int match_extension(const char *name, const std::vector<std::string> &exts) {
    if (exts.empty()) return 1;
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    const char *ext = dot + 1;
    for (size_t i = 0; i < exts.size(); i++) {
        if (strcmp(ext, exts[i].c_str()) == 0) {
            return 1;
        }
    }
    return 0;
}

static int match_exclude(const char *path, const char *name,
                          const std::vector<std::string> &specs) {
    if (specs.empty()) return 0;
    for (size_t i = 0; i < specs.size(); i++) {
        const char *pat = specs[i].c_str();
        if (fnmatch(pat, path, FNM_PATHNAME) == 0 ||
            fnmatch(pat, name, 0) == 0) {
            return 1;
        }
    }
    return 0;
}

static int fd_should_ci(const FdOptions *opts) {
    if (opts->case_sensitive) return 0;
    if (opts->ignore_case) return 1;
    if (opts->smart_case) return is_pattern_lowercase(opts->pattern.c_str());
    return 0;
}

static void fd_exec_file(const char *fullpath, FdOptions *opts);
static void fd_exec_finalize(FdOptions *opts);

// NOLINTNEXTLINE(misc-no-recursion)
static int fd_walk(const char *dirpath, FdOptions *opts, std::regex *re,
                    int is_ci, int is_glob, int depth) {
    if (opts->max_depth >= 0 && depth > opts->max_depth) return 0;

    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        (void)fprintf(stderr, "fd: %s: %s\n", dirpath, strerror(errno));
        return 0;
    }

    int match_count = 0;
    int use_color = (opts->color_mode == FdColorMode::ALWAYS ||
                     (opts->color_mode == FdColorMode::AUTO && isatty(STDOUT_FILENO)));

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (opts->max_results > 0 && match_count >= opts->max_results) break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (!opts->hidden && is_hidden(entry->d_name)) continue;

        char full_path[PATH_MAX];
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);

        /* Always lstat first for type detection (needed for symlink filter
           even with -L, where stat() follows the link). Then optionally
           follow with stat() for recursion decisions. */
        struct stat lst, st;
        int rc_lst = lstat(full_path, &lst);
        if (rc_lst != 0) {
            continue;
        }
        if (opts->follow) {
            if (stat(full_path, &st) != 0) st = lst;
        } else {
            st = lst;
        }

        /* Use lstat result for type filter — catches symlinks even with -L */
        mode_t check_mode = opts->follow ? lst.st_mode : st.st_mode;

        if (match_exclude(full_path, entry->d_name, opts->exclude_patterns)) {
            continue;
        }

        int matches_type = 1;
        if (opts->type_filter != 0) {
            matches_type = match_type(check_mode, opts->type_filter);
        }
        if (opts->type_filter == 'e') {
            matches_type = is_empty_file(full_path, check_mode, &st);
        }

        int matches_ext = 1;
        if (!S_ISDIR(st.st_mode)) {
            matches_ext = match_extension(entry->d_name, opts->extensions);
        }

        int pattern_match = 0;
        if (matches_type && matches_ext) {
            const char *target = opts->full_path ? full_path : entry->d_name;
            if (is_glob) {
                if (is_ci) {
                    /* Case-insensitive: lowercase both target and pattern */
                    std::string lower_target;
                    lower_target.resize(strlen(target));
                    for (size_t i = 0; target[i]; i++) {
                        lower_target[i] = (char)tolower((unsigned char)target[i]);
                    }
                    std::string lower_pat;
                    lower_pat.resize(strlen(opts->pattern.c_str()));
                    for (size_t i = 0; opts->pattern[i]; i++) {
                        lower_pat[i] = (char)tolower((unsigned char)opts->pattern[i]);
                    }
                    pattern_match = (fnmatch(lower_pat.c_str(), lower_target.c_str(), FNM_PATHNAME) == 0);
                } else {
                    pattern_match = (fnmatch(opts->pattern.c_str(), target, FNM_PATHNAME) == 0);
                }
            } else {
                pattern_match = (int)std::regex_search(target, *re);
            }
        }

        if (pattern_match) {
            match_count++;
            if (opts->has_exec) {
                fd_exec_file(full_path, opts);
            } else {
                if (opts->print0) {
                    printf("%s%c", full_path, '\0');
                } else if (use_color) {
                    if (S_ISDIR(st.st_mode)) {
                        printf("\033[01;34m%s\033[0m\n", full_path);
                    } else if (S_ISLNK(lst.st_mode)) {
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
            int sub = fd_walk(full_path, opts, re, is_ci, is_glob, depth + 1);
            match_count += sub;
            /* Clamp after recursive add to avoid max-results overshoot */
            if (opts->max_results > 0 && match_count > opts->max_results) {
                match_count = opts->max_results;
            }
        }
    }

    (void)closedir(dir);
    return match_count;
}

static void fd_exec_file(const char *fullpath, FdOptions *opts) {
    if (opts->exec_batch) {
        opts->exec_paths.push_back(fullpath);
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> args;
        int has_subst = 0;
        for (size_t i = 0; i < opts->exec_args.size(); i++) {
            const char *a = opts->exec_args[i].c_str();
            if (strcmp(a, "{}") == 0) {
                args.push_back((char*)fullpath);
                has_subst = 1;
            } else {
                args.push_back((char*)a);
            }
        }
        if (!has_subst) {
            args.push_back((char*)fullpath);
        }
        args.push_back(NULL);
        (void)execvp(args[0], args.data());
        (void)fprintf(stderr, "fd: %s: %s\n",
                      args[0], strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        int status;
        (void)waitpid(pid, &status, 0);
    } else {
        (void)fprintf(stderr, "fd: fork failed: %s\n", strerror(errno));
    }
}

static void fd_exec_finalize(FdOptions *opts) {
    if (!opts->has_exec || !opts->exec_batch || opts->exec_paths.empty()) return;

    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> args;
        int has_subst = 0;
        for (size_t i = 0; i < opts->exec_args.size(); i++) {
            const char *a = opts->exec_args[i].c_str();
            if (strcmp(a, "{}") == 0) {
                for (size_t j = 0; j < opts->exec_paths.size(); j++) {
                    args.push_back((char*)opts->exec_paths[j].c_str());
                }
                has_subst = 1;
            } else {
                args.push_back((char*)a);
            }
        }
        if (!has_subst) {
            for (size_t j = 0; j < opts->exec_paths.size(); j++) {
                args.push_back((char*)opts->exec_paths[j].c_str());
            }
        }
        args.push_back(NULL);
        (void)execvp(args[0], args.data());
        (void)fprintf(stderr, "fd: %s: %s\n",
                      args[0], strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        int status;
        (void)waitpid(pid, &status, 0);
    } else {
        (void)fprintf(stderr, "fd: fork failed: %s\n", strerror(errno));
    }
}

// NOLINTNEXTLINE(readability-function-size)
void fd_command(int argc, char **argv) {
    FdOptions opts;
    opts.smart_case = 1;
    opts.max_depth = -1;
    opts.color_mode = FdColorMode::AUTO;

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
        for (int i = 0; i < extension_opt->count; i++) {
            opts.extensions.push_back(extension_opt->sval[i]);
        }
    }

    if (exclude_opt->count > 0) {
        for (int i = 0; i < exclude_opt->count; i++) {
            opts.exclude.push_back(exclude_opt->sval[i]);
            opts.exclude_patterns.push_back(exclude_opt->sval[i]);
        }
    }

    if (exec_opt->count > 0) {
        opts.has_exec = 1;
        for (int i = 0; i < exec_opt->count; i++) {
            opts.exec_args.push_back(exec_opt->sval[i]);
        }
    }

    if (exec_batch_opt->count > 0) {
        opts.has_exec = 1;
        opts.exec_batch = 1;
        for (int i = 0; i < exec_batch_opt->count; i++) {
            opts.exec_args.push_back(exec_batch_opt->sval[i]);
        }
    }

    if (color_opt->count > 0) {
        const char *cv = color_opt->sval[0];
        if (strcmp(cv, "never") == 0) {
            opts.color_mode = FdColorMode::NEVER;
        } else if (strcmp(cv, "always") == 0) {
            opts.color_mode = FdColorMode::ALWAYS;
        } else if (strcmp(cv, "auto") == 0) {
            opts.color_mode = FdColorMode::AUTO;
        } else {
            (void)fprintf(stderr, "fd: invalid color '%s' (valid: never, auto, always)\n", cv);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            exit(2);
        }
    }

    opts.pattern = pattern_arg->filename[0];

    int is_ci = fd_should_ci(&opts);
    int is_glob = opts.glob_mode;

    std::regex *re = NULL;
    if (!is_glob) {
        std::regex::flag_type flags = std::regex::optimize;
        if (is_ci) {
            flags |= std::regex::icase;
        }
        try {
            re = new std::regex(opts.pattern, flags);
        } catch (const std::regex_error &e) {
            (void)fprintf(stderr, "fd: invalid pattern '%s': %s\n",
                          opts.pattern.c_str(), e.what());
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
            const char *p = path_arg->filename[i];
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

    fd_exec_finalize(&opts);
    delete re;
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    exit(total_matches > 0 ? 0 : 1);
}
