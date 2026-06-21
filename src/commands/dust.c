#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ftw.h>
#include <fnmatch.h>
#include <glib.h>
#include <argtable3.h>

#include "commands/dust.h"

/* ── Entry struct ───────────────────────────────────────────────────────── */

typedef struct {
    gchar *path;
    guint64 agg_size;     /* total including children */
    int depth;
    int is_dir;
} DustEntry;

/* ── Globals for nftw callback ──────────────────────────────────────────── */

static GPtrArray *entries;          /* all DustEntry* */
static GPtrArray *exclude_patterns; /* gchar* patterns */
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
    if (exclude_patterns && exclude_patterns->len > 0) {
        const char *base = strrchr(fpath, '/');
        base = base ? base + 1 : fpath;
        for (unsigned int i = 0; i < exclude_patterns->len; i++) {
            const char *pat = (const char *)g_ptr_array_index(exclude_patterns, i);
            if (fnmatch(pat, fpath, FNM_PATHNAME) == 0 ||
                fnmatch(pat, base, 0) == 0) {
                return 0;
            }
        }
    }

    DustEntry *e = g_malloc0(sizeof(DustEntry));
    e->path = g_strdup(fpath);
    e->depth = ftwb->level;
    e->is_dir = (typeflag == FTW_D || typeflag == FTW_DP);
    e->agg_size = (guint64)sb->st_blocks * 512ULL;
    g_ptr_array_add(entries, e);

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

static gint cmp_size_desc(gconstpointer a, gconstpointer b, gpointer d) {
    (void)d;
    const DustEntry *ea = *(const DustEntry * const *)a;
    const DustEntry *eb = *(const DustEntry * const *)b;
    if (ea->agg_size > eb->agg_size) return -1;
    if (ea->agg_size < eb->agg_size) return 1;
    return strcmp(ea->path, eb->path);
}

static void aggregate(void) {
    if (entries->len == 0) return;

    /* Hash: path → DustEntry* for O(1) parent lookup */
    GHashTable *map = g_hash_table_new(g_str_hash, g_str_equal);
    for (unsigned int i = 0; i < entries->len; i++) {
        DustEntry *e = (DustEntry *)g_ptr_array_index(entries, i);
        g_hash_table_insert(map, e->path, e);
    }

    /* Reusable buffer — avoids per-entry g_strndup */
    char *buf = g_malloc(PATH_MAX);

    for (unsigned int i = 0; i < entries->len; i++) {
        DustEntry *e = (DustEntry *)g_ptr_array_index(entries, i);
        size_t len = strlen(e->path);
        if (len >= PATH_MAX) continue;

        memcpy(buf, e->path, len + 1);      /* copy + NUL */
        char *slash = strrchr(buf, '/');
        if (slash == NULL) continue;        /* no parent in tree */

        *slash = '\0';
        DustEntry *parent = (DustEntry *)g_hash_table_lookup(map, buf);
        if (parent) {
            parent->agg_size += e->agg_size;
        }
    }

    g_free(buf);
    g_hash_table_destroy(map);

    /* Sort by size descending — one sort total */
    g_ptr_array_sort_with_data(entries, cmp_size_desc, NULL);
}

/* ── Format helpers ─────────────────────────────────────────────────────── */

#define BAR_WIDTH 40

static const char *suffix_1024[] = {"", "K", "M", "G", "T", "P"};
static const char *suffix_1000[] = {"", "kB", "MB", "GB", "TB", "PB"};

/* Format size into static buf. Returns buf. */
static const char *fmt_size(guint64 bytes, int si, int bytes_mode) {
    static char buf[16];
    if (bytes_mode) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, sizeof(buf), "%llu", (unsigned long long)bytes);
        return buf;
    }
    const char **suf = si ? suffix_1000 : suffix_1024;
    guint64 unit = si ? 1000ULL : 1024ULL;
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
void dust_command(gint argc, gchar **argv) {
    DustOptions opts = {0};
    entries = NULL;
    exclude_patterns = NULL;
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
        exclude_patterns = g_ptr_array_new();
        for (int i = 0; i < exclude_opt->count; i++) {
            g_ptr_array_add(exclude_patterns, (gpointer)exclude_opt->sval[i]);
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
        entries = g_ptr_array_new();
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
        GPtrArray *display = g_ptr_array_new();
        for (unsigned int i = 0; i < entries->len; i++) {
            DustEntry *e = (DustEntry *)g_ptr_array_index(entries, i);
            if (!opts.show_all && !e->is_dir) continue;
            if (opts.max_depth >= 0 && e->depth > opts.max_depth) continue;
            if (p > 0 && e->depth == 0) continue;
            g_ptr_array_add(display, e);
            if (display->len >= (unsigned int)opts.max_lines) break;
        }

        /* Find max size among displayed entries for bar scaling */
        guint64 max_size = 0;
        for (unsigned int i = 0; i < display->len; i++) {
            DustEntry *e = (DustEntry *)g_ptr_array_index(display, i);
            if (e->agg_size > max_size) max_size = e->agg_size;
        }

        /* Print in reverse (ascending) so smallest first, biggest at bottom */
        for (int i = (int)display->len - 1; i >= 0; i--) {
            DustEntry *e = (DustEntry *)g_ptr_array_index(display, i);

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
        g_ptr_array_free(display, FALSE);

        /* Cleanup */
        for (unsigned int i = 0; i < entries->len; i++) {
            DustEntry *e = (DustEntry *)g_ptr_array_index(entries, i);
            g_free(e->path);
            g_free(e);
        }
        g_ptr_array_free(entries, FALSE);
    }

    if (exclude_patterns) {
        g_ptr_array_free(exclude_patterns, FALSE);
    }
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (du_had_error) {
        exit(1);
    }
}
