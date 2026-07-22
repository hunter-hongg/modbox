#define _GNU_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <ftw.h>
#include <fnmatch.h>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <argtable3.h>

#include "commands/dust.hpp"
#include "commands/command_macros.hpp"

/* ── Entry struct ───────────────────────────────────────────────────────── */

typedef struct {
    char *path;
    uint64_t agg_size;     /* total including children */
    int depth;
    int is_dir;
} DustEntry;

/* ── Globals for nftw callback ──────────────────────────────────────────── */

static std::vector<DustEntry*> entries;          /* all DustEntry* */
static std::vector<std::string> exclude_patterns; /* patterns */
static int one_fs_flag;             /* FTW_MOUNT flag */
static int du_had_error;
static int scan_count;              /* progress counter */
static int progress_tty;            /* stderr is a terminal */

/* ── nftw callback ──────────────────────────────────────────────────────── */

static int walk_cb(const char *fpath, const struct stat *sb,
                    int typeflag, struct FTW *ftwb) {
    if (typeflag == FTW_NS) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "dust: %s: %s\n", fpath, strerror(errno));
        du_had_error = 1;
        return 0;
    }

    /* Check exclude patterns */
    if (!exclude_patterns.empty()) {
        const char *base = strrchr(fpath, '/');
        base = base ? base + 1 : fpath;
        for (size_t i = 0; i < exclude_patterns.size(); i++) {
            const char *pat = exclude_patterns[i].c_str();
            if (fnmatch(pat, fpath, FNM_PATHNAME) == 0 ||
                fnmatch(pat, base, 0) == 0) {
                return 0;
            }
        }
    }

    DustEntry *e = new DustEntry{};
    e->path = strdup(fpath);
    e->depth = ftwb->level;
    e->is_dir = (typeflag == FTW_D || typeflag == FTW_DP);
    e->agg_size = (uint64_t)sb->st_blocks * 512ULL;
    entries.push_back(e);

    /* Progress animation */
    scan_count++;
    if (progress_tty && (scan_count & 0x1FF) == 0) {  /* every 512 files */
        static const char spin[] = {'|', '/', '-', '\\'};
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "\r  %c scanning... %d files",
                      spin[(scan_count >> 9) & 3], scan_count);
        (void)fflush(stderr);
    }

    return 0;
}

/* ── Sort / aggregate ──────────────────────────────────────────────────── */

static void aggregate(void) {
    if (entries.empty()) return;

    /* Hash: path → DustEntry* for O(1) parent lookup */
    std::unordered_map<std::string, DustEntry*> map;
    for (size_t i = 0; i < entries.size(); i++) {
        DustEntry *e = entries[i];
        map[e->path] = e;
    }

    /* Reusable buffer — avoids per-entry strndup */
    char *buf = (char*)malloc(PATH_MAX);

    for (size_t i = 0; i < entries.size(); i++) {
        DustEntry *e = entries[i];
        size_t len = strlen(e->path);
        if (len >= PATH_MAX) continue;

        memcpy(buf, e->path, len + 1);      /* copy + NUL */
        char *slash = strrchr(buf, '/');
        if (slash == NULL) continue;        /* no parent in tree */

        *slash = '\0';
        auto it = map.find(buf);
        if (it != map.end()) {
            DustEntry *parent = it->second;
            parent->agg_size += e->agg_size;
        }
    }

    free(buf);

    /* Sort by size descending — one sort total */
    std::sort(entries.begin(), entries.end(), [](const DustEntry* a, const DustEntry* b) {
        if (a->agg_size != b->agg_size) return a->agg_size > b->agg_size;
        return strcmp(a->path, b->path) < 0;
    });
}

/* ── Format helpers ─────────────────────────────────────────────────────── */

#define BAR_WIDTH 40

static const char *suffix_1024[] = {"", "K", "M", "G", "T", "P"};
static const char *suffix_1000[] = {"", "kB", "MB", "GB", "TB", "PB"};

/* Format size into static buf. Returns buf. */
static const char *fmt_size(uint64_t bytes, int si, int bytes_mode) {
    static char buf[16];
    if (bytes_mode) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, sizeof(buf), "%llu", (unsigned long long)bytes);
        return buf;
    }
    const char **suf = si ? suffix_1000 : suffix_1024;
    uint64_t unit = si ? 1000ULL : 1024ULL;
    int idx = 0;
    double val = (double)bytes;
    while (val >= unit && idx < 5) {
        val /= (double)unit;
        idx++;
    }
    if (idx == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, sizeof(buf), "%llu%s", (unsigned long long)val, suf[idx]);
    } else if (val < 10.0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, sizeof(buf), "%.1f%s", val, suf[idx]);
    } else if (val < 100.0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, sizeof(buf), "%.0f%s", val, suf[idx]);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, sizeof(buf), "%.0f%s", val, suf[idx]);
    }
    return buf;
}

/* ── Main command ────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void dust_command(int argc, char **argv) {
    DustOptions opts = {0};
    entries.clear();
    exclude_patterns.clear();
    one_fs_flag = 0;
    du_had_error = 0;

    struct arg_int *depth_opt = arg_int0("d", "depth", "N", "max depth");
    struct arg_int *lines_opt = arg_int0("n", "number-of-lines", "N", "max lines to show");
    struct arg_lit *all_opt = arg_lit0("a", "all", "show all files, not just dirs");
    struct arg_lit *one_fs_opt = arg_lit0("x", "one-file-system", "skip different filesystems");
    struct arg_lit *si_opt = arg_lit0("H", "si", "use powers of 1000");
    struct arg_lit *bytes_opt = arg_lit0("b", "bytes", "show bytes");
    struct arg_lit *no_color_opt = arg_lit0("c", "no-color", "disable color output");
    struct arg_str *exclude_opt = arg_strn("X", "exclude", "PATTERN", 0, 100, "exclude pattern");
    struct arg_lit *help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file or directory");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        depth_opt, lines_opt, all_opt, one_fs_opt,
        si_opt, bytes_opt, no_color_opt,
        exclude_opt,
        help_opt,
        file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Disk usage with size bars (dust-style).\n");
        printf("\n");
        printf("Options:\n");
        printf("  -d, --depth=N          max depth\n");
        printf("  -n, --number-of-lines=N max lines to show (default 40)\n");
        printf("  -a, --all              show all files, not just directories\n");
        printf("  -x, --one-file-system   skip directories on different filesystems\n");
        printf("  -H, --si               use powers of 1000 not 1024\n");
        printf("  -b, --bytes            show bytes\n");
        printf("  -c, --no-color         disable color output\n");
        printf("  -X, --exclude=PATTERN  exclude files matching pattern\n");
        printf("  -h, --help             display this help and exit\n");
        printf("\n");
        printf("With no FILE, read current directory.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.max_depth = (depth_opt->count > 0) ? depth_opt->ival[0] : -1;
    opts.max_lines = (lines_opt->count > 0) ? lines_opt->ival[0] : 40;
    opts.show_all = (all_opt->count > 0);
    opts.one_file_system = (one_fs_opt->count > 0);
    opts.si = (si_opt->count > 0);
    opts.bytes = (bytes_opt->count > 0);
    opts.no_color = (no_color_opt->count > 0);

    if (exclude_opt->count > 0) {
        for (int i = 0; i < exclude_opt->count; i++) {
            exclude_patterns.push_back(exclude_opt->sval[i]);
        }
    }

    if (opts.one_file_system) {
        one_fs_flag = FTW_MOUNT;
    }

    /* Collect paths */
    int path_count = file_arg->count;
    const char **paths = (const char **)file_arg->filename;
    if (path_count == 0) {
        static const char *def[] = {"."};
        paths = def;
        path_count = 1;
    }

    /* Check if stdout is a TTY for color */
    int use_color = !opts.no_color && isatty(STDOUT_FILENO);

    /* Check if stderr is a TTY for progress animation */
    progress_tty = isatty(STDERR_FILENO);

    /* Walk, aggregate, display for each path */
    for (int p = 0; p < path_count; p++) {
        entries.clear();
        scan_count = 0;

        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        if (nftw(paths[p], walk_cb, 20, FTW_PHYS | FTW_DEPTH | one_fs_flag) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "dust: %s: %s\n", paths[p], strerror(errno));
        }

        /* Clear progress line */
        if (progress_tty) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "\r  scanned %d files\n", scan_count);
        }

        aggregate();

        /* Collect displayable entries (sorted by size desc, top N) */
        std::vector<DustEntry*> display;
        for (size_t i = 0; i < entries.size(); i++) {
            DustEntry *e = entries[i];
            if (!opts.show_all && !e->is_dir) continue;
            if (opts.max_depth >= 0 && e->depth > opts.max_depth) continue;
            if (p > 0 && e->depth == 0) continue;
            display.push_back(e);
            if (display.size() >= (size_t)opts.max_lines) break;
        }

        /* Find max size among displayed entries for bar scaling */
        uint64_t max_size = 0;
        for (size_t i = 0; i < display.size(); i++) {
            DustEntry *e = display[i];
            if (e->agg_size > max_size) max_size = e->agg_size;
        }

        /* Print in reverse (ascending) so smallest first, biggest at bottom */
        for (int i = (int)display.size() - 1; i >= 0; i--) {
            DustEntry *e = display[i];

            /* Size field (right-aligned 5 chars) */
            const char *s = fmt_size(e->agg_size, opts.si, opts.bytes);
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stdout, "%5s ", s);

            /* Bar */
            int bar_len = 0;
            if (max_size > 0) {
                bar_len = (int)(e->agg_size * BAR_WIDTH / max_size);
                if (bar_len == 0 && e->agg_size > 0) bar_len = 1;
                if (bar_len > BAR_WIDTH) bar_len = BAR_WIDTH;
            }

            if (use_color) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stdout, "\033[34m");
            }
            for (int b = 0; b < bar_len; b++) {
                (void)fputs("\xE2\x96\x88", stdout);
            }
            if (use_color) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stdout, "\033[0m");
            }

            /* Spacer to align percentage */
            for (int b = bar_len; b < BAR_WIDTH; b++) {
                (void)fputc(' ', stdout);
            }

            /* Percentage */
            if (max_size > 0) {
                int pct = (int)(e->agg_size * 100 / max_size);
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stdout, " %3d%% ", pct);
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stdout, "   0%% ");
            }

            /* Path */
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stdout, "%s\n", e->path);
        }

        /* Cleanup */
        for (size_t i = 0; i < entries.size(); i++) {
            DustEntry *e = entries[i];
            free(e->path);
            delete e;
        }
        entries.clear();
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (du_had_error) {
        exit(1);
    }
}

REGISTER_COMMAND("dust", dust_command, "Alias for du --max-depth=1 -h");
