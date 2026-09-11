#include <cstdint>
#include <sys/stat.h>
#include <linux/limits.h>
#define _GNU_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <ftw.h>
#include <fnmatch.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <argtable3.h>
#include "commands/arg_util.hpp"

#include "commands/du.hpp"
#include "commands/command_macros.hpp"
#include "commands/json_stringifier.hpp"

/* ── Internal data structures ──────────────────────────────────────────── */

/* One file/dir entry from tree walk */
typedef struct {
    char *path;          /* relative path from start point */
    uint64_t size_bytes;   /* raw size in bytes (st_blocks*512 or st_size) */
    uint64_t agg_size;     /* aggregated size including descendants */
    int depth;            /* depth in tree, 0 = root arg */
    int is_dir;
    int is_error;         /* stat failed */
    time_t mtime;
} DuEntry;

/* ── File-scope globals for nftw callback ──────────────────────────────── */

static std::vector<DuEntry*> du_entries;  /* all DuEntry* collected */
static const DuOptions *du_glob_opts;
static int du_had_error;
static int du_scan_count;
static int du_progress_tty;

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Skip leading ./ prefix for cleaner display */
static const char *clean_path(const char *fpath, const char *base) {
    const char *p = fpath;
    /* If fpath starts with base, skip it */
    size_t const blen = strlen(base);
    if (strncmp(p, base, blen) == 0) {
        p += blen;
        while (*p == '/')
        {
            p++;
        }
        if (*p == '\0')
        {
            p = ".";
        }
    }
    return p;
}

/* Check if path matches any exclude pattern */
static int is_excluded(const char *fpath) {
    if ((du_glob_opts->exclude == nullptr) || du_glob_opts->exclude_count == 0) {
        return 0;
    }
    /* Get basename */
    const char *base = strrchr(fpath, '/');
    base = (base != nullptr) ? base + 1 : fpath;

    for (int i = 0; i < du_glob_opts->exclude_count; i++) {
        if (fnmatch(du_glob_opts->exclude[i], fpath, FNM_PATHNAME) == 0) {
            return 1;
        }
        if (fnmatch(du_glob_opts->exclude[i], base, 0) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ── nftw callback ──────────────────────────────────────────────────────── */

static int du_callback(const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf) {
    /* Skip excluded paths */
    if (is_excluded(fpath) != 0) {
        return 0;
    }

    /* Handle stat failures */
    if (typeflag == FTW_NS) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "du: cannot read '%s': %s\n",
                      fpath, strerror(errno));
        du_had_error = 1;
        return 0;
    }

    /* With FTW_PHYS+FTW_DEPTH, typeflag is FTW_F, FTW_DP, FTW_DNR,
       FTW_SL, FTW_SLN, or FTW_NS. We count all except FTW_DNR/FTW_NS. */
    DuEntry *e = new DuEntry{};
    e->path = strdup(fpath);
    e->depth = ftwbuf->level;
    e->is_dir = static_cast<int>(typeflag == FTW_D || typeflag == FTW_DP);
    e->is_error = 0;
    e->mtime = sb->st_mtime;

    if (du_glob_opts->apparent_size != 0) {
        e->size_bytes = static_cast<uint64_t>(sb->st_size);
    } else {
        e->size_bytes = static_cast<uint64_t>(sb->st_blocks) * 512ULL;
    }

    e->agg_size = e->size_bytes;
    du_entries.push_back(e);

    return 0;
}

/* ── Aggregate sizes upward ─────────────────────────────────────────────── */

static void aggregate_sizes() {
    if (du_entries.empty())
    {
        return;
    }

    /* Hash: path → DuEntry* for O(1) parent lookup */
    std::unordered_map<std::string, DuEntry*> map;
    for (size_t i = 0; i < du_entries.size(); i++) {
        DuEntry *e = du_entries[i];
        map[e->path] = e;
    }

    char *buf = static_cast<char*>(malloc(PATH_MAX));

    for (size_t i = 0; i < du_entries.size(); i++) {
        const DuEntry *e = du_entries[i];
        size_t const len = strlen(e->path);
        if (len >= PATH_MAX)
        {
            continue;
        }

        memcpy(buf, e->path, len + 1);
        char *slash = strrchr(buf, '/');
        if (slash == NULL)
        {
            continue;
        }

        *slash = '\0';
        auto it = map.find(buf);
        if (it != map.end()) {
            DuEntry *parent = it->second;
            parent->agg_size += e->agg_size;
        }
    }

    free(buf);

    /* Restore path order for printing */
    std::sort(du_entries.begin(), du_entries.end(), [](const DuEntry* a, const DuEntry* b) {
        return strcmp(a->path, b->path) < 0;
    });
}

/* ── Format size ────────────────────────────────────────────────────────── */

/* suffix array for 1024-based */
static const char *suffix_1024[] = {"", "K", "M", "G", "T", "P", "E"};
/* suffix array for 1000-based (SI) */
static const char *suffix_1000[] = {"", "kB", "MB", "GB", "TB", "PB", "EB"};

/* Scale size to display unit. Returns scaled value + writes suffix index.
   unit = 1024 or 1000 depending on --si */
static double scale_size(uint64_t bytes, int si, int *suffix_idx) {
    uint64_t const unit = (si != 0) ? 1000ULL : 1024ULL;
    int idx = 0;
    double val = static_cast<double>(bytes);
    while (val >= unit && idx < 6) {
        val /= static_cast<double>(unit);
        idx++;
    }
    *suffix_idx = idx;
    return val;
}

/* Format size into a static buffer. Returns buf. */
static char *format_size(uint64_t bytes, const DuOptions *opts, char *buf, size_t buf_size) {
    if (opts->bytes != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%llu", static_cast<unsigned long long>(bytes));
        return buf;
    }

    uint64_t display = bytes;
    if (opts->block_size_k != 0) {
        display = (bytes + 512) / 1024;  /* ceil, matching GNU du default */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%llu", static_cast<unsigned long long>(display));
        return buf;
    }
    if (opts->block_size_m != 0) {
        display = (bytes + 512 * 1024) / (1024 * 1024);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%llu", static_cast<unsigned long long>(display));
        return buf;
    }

    /* Default: 1024-byte blocks like GNU du */
    if (opts->human_readable == 0) {
        display = (bytes + 512) / 1024;
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%llu", static_cast<unsigned long long>(display));
        return buf;
    }

    /* Human-readable */
    int const si = opts->si;
    const char **suffixes = (si != 0) ? suffix_1000 : suffix_1024;
    int idx = 0;
    double const val = scale_size(bytes, si, &idx);

    if (idx == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%llu%s", static_cast<unsigned long long>(val), suffixes[idx]);
    } else if (val < 10.0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%.1f%s", val, suffixes[idx]);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%.0f%s", val, suffixes[idx]);
    }

    return buf;
}

/* ── Printing ────────────────────────────────────────────────────────────── */

static void print_entry(const DuEntry *e, const DuOptions *opts,
                         uint64_t *total_acc) {
    uint64_t const size = e->agg_size;

    /* --threshold: skip if below threshold (in bytes) */
    if ((opts->threshold_set != 0) && size < opts->threshold) {
        return;
    }

    char size_buf[64];
    format_size(size, opts, size_buf, sizeof(size_buf));

    if (total_acc != nullptr) {
        *total_acc += size;
    }

    const char *term = (opts->null_terminated != 0) ? "\0" : "\n";

    if (opts->show_time != 0) {
        char time_buf[64];
        const struct tm *tm = localtime(&e->mtime);
        if (tm != nullptr) {
            (void)strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", tm);
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)snprintf(time_buf, sizeof(time_buf), "?");
        }
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stdout, "%s\t%s\t%s%s", size_buf, time_buf, e->path, term);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stdout, "%s\t%s%s", size_buf, e->path, term);
    }
}

/* ── Main command ────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int du_command(int argc, char **argv) {
    DuOptions opts = {.bytes=0};
    du_entries.clear();
    du_glob_opts = &opts;
    du_had_error = 0;
    du_progress_tty = isatty(STDERR_FILENO);

    /* argtable3 */
    struct arg_lit *bytes_opt = arg_lit0("b", "bytes", "equivalent to --apparent-size --block-size=1");
    struct arg_lit *block_k_opt = arg_lit0("k", NULL, "block size 1K");
    struct arg_lit *block_m_opt = arg_lit0("m", NULL, "block size 1M");
    struct arg_lit *human_opt = arg_lit0("h", "human-readable", "print human readable sizes");
    struct arg_lit *summarize_opt = arg_lit0("s", "summarize", "display only a total for each argument");
    struct arg_lit *total_opt = arg_lit0("c", "total", "produce a grand total");
    struct arg_lit *all_opt = arg_lit0("a", "all", "write counts for all files, not just directories");
    struct arg_int *max_depth_opt = arg_int0("d", "max-depth", "N", "max recursion depth");
    struct arg_lit *one_fs_opt = arg_lit0("x", "one-file-system", "skip directories on different filesystems");
    struct arg_lit *count_links_opt = arg_lit0("l", "count-links", "count sizes multiple times if hard linked");
    struct arg_lit *si_opt = arg_lit0(NULL, "si", "use powers of 1000 not 1024");
    struct arg_lit *apparent_opt = arg_lit0(NULL, "apparent-size", "print apparent sizes, not disk usage");
    struct arg_lit *time_opt = arg_lit0(NULL, "time", "show time of last modification");
    struct arg_lit *separate_opt = arg_lit0("S", "separate-dirs", "do not include size of subdirectories");
    struct arg_lit *null_opt = arg_lit0("0", "null", "end each output line with NUL, not newline");
    struct arg_str *exclude_opt = arg_strn(NULL, "exclude", "PATTERN", 0, 100, "exclude files matching pattern");
    struct arg_str *threshold_opt = arg_str0("t", "threshold", "SIZE", "exclude entries smaller than SIZE");
    struct arg_lit *json_opt = arg_lit0(NULL, "json", "output in JSON format");
    struct arg_lit *help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file or directory");
    struct arg_end *end = arg_end(20);

    ArgTable at({bytes_opt, block_k_opt, block_m_opt, human_opt,
                 summarize_opt, total_opt, all_opt, max_depth_opt,
                 one_fs_opt, count_links_opt, si_opt, apparent_opt,
                 time_opt, separate_opt, null_opt,
                 exclude_opt, threshold_opt,
                 json_opt,
                 help_opt,
                 file_arg, end});

    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Summarize disk usage of each FILE, recursively for directories.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -b, --bytes              equivalent to --apparent-size --block-size=1\n");
        printf("  -k                       block size 1K\n");
        printf("  -m                       block size 1M\n");
        printf("  -h, --human-readable     print human readable sizes\n");
        printf("  -s, --summarize          display only a total for each argument\n");
        printf("  -c, --total              produce a grand total\n");
        printf("  -a, --all                write counts for all files, not just directories\n");
        printf("  -d, --max-depth=N        max recursion depth\n");
        printf("  -x, --one-file-system    skip directories on different filesystems\n");
        printf("  -l, --count-links        count sizes multiple times if hard linked\n");
        printf("      --si                 use powers of 1000 not 1024\n");
        printf("      --apparent-size      print apparent sizes, not disk usage\n");
        printf("      --time               show time of last modification\n");
        printf("  -S, --separate-dirs      do not include size of subdirectories\n");
        printf("  -0, --null               end each output line with NUL, not newline\n");
        printf("      --exclude=PATTERN    exclude files matching pattern\n");
        printf("  -t, --threshold=SIZE     exclude entries smaller than SIZE\n");
        printf("      --json               output in JSON format\n");
        printf("  -h, --help               display this help and exit\n");
        printf("\n");
        printf("With no FILE, read current directory.\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    /* Populate options */
    opts.bytes = static_cast<int>(bytes_opt->count > 0);
    opts.block_size_k = static_cast<int>(block_k_opt->count > 0);
    opts.block_size_m = static_cast<int>(block_m_opt->count > 0);
    opts.human_readable = static_cast<int>(human_opt->count > 0);
    opts.summarize = static_cast<int>(summarize_opt->count > 0);
    opts.total = static_cast<int>(total_opt->count > 0);
    opts.all = static_cast<int>(all_opt->count > 0);
    opts.max_depth = (max_depth_opt->count > 0) ? max_depth_opt->ival[0] : -1;
    opts.one_file_system = static_cast<int>(one_fs_opt->count > 0);
    opts.count_links = static_cast<int>(count_links_opt->count > 0);
    opts.si = static_cast<int>(si_opt->count > 0);
    opts.apparent_size = static_cast<int>(apparent_opt->count > 0);
    opts.show_time = static_cast<int>(time_opt->count > 0);
    opts.separate_dirs = static_cast<int>(separate_opt->count > 0);
    opts.null_terminated = static_cast<int>(null_opt->count > 0);
    bool const json_mode = (json_opt->count > 0);

    /* -b implies --apparent-size */
    if (opts.bytes != 0) {
        opts.apparent_size = 1;
    }

    /* Parse exclude patterns */
    if (exclude_opt->count > 0) {
        opts.exclude = static_cast<char**>(malloc(static_cast<size_t>(exclude_opt->count) * sizeof(char *)));
        opts.exclude_count = exclude_opt->count;
        for (int i = 0; i < exclude_opt->count; i++) {
            opts.exclude[i] = const_cast<char *>(exclude_opt->sval[i]);
        }
    }

    /* Parse threshold */
    if (threshold_opt->count > 0) {
        opts.threshold_set = 1;
        char *endp = NULL;
        // NOLINTNEXTLINE(cert-err34-c)
        opts.threshold = static_cast<uint64_t>(strtoull(threshold_opt->sval[0], &endp, 10));
        if ((endp != nullptr) && ((*endp) != 0)) {
            switch (*endp) {
                case 'K': case 'k': opts.threshold *= 1024ULL; break;
                case 'M': opts.threshold *= 1024ULL * 1024; break;
                case 'G': opts.threshold *= 1024ULL * 1024 * 1024; break;
                default: break;
            }
        }
    }

    /* Collect starting paths */
    int path_count = file_arg->count;
    const char **paths = (const char **)file_arg->filename;
    if (path_count == 0) {
        static const char *default_paths[] = {"."};
        paths = default_paths;
        path_count = 1;
    }

    /* Walk all paths */
    int nftw_flags = FTW_PHYS | FTW_DEPTH;
    if (opts.one_file_system != 0) {
        nftw_flags |= FTW_MOUNT;
    }

    /* Process each path independently to avoid double-counting with -c */
    uint64_t grand_total = 0;
    /* Hash set tracks printed paths */
    std::unordered_map<std::string, int> printed;
    std::vector<DuEntry*> json_entries;

    for (int p = 0; p < path_count; p++) {
        /* Fresh walk for each path */
        du_entries.clear();
        du_had_error = 0;
        du_scan_count = 0;

        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        if (nftw(paths[p], du_callback, 20, nftw_flags) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "du: %s: %s\n", paths[p], strerror(errno));
        }

        /* Clear progress line */
        if (du_progress_tty != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "\r  scanned %d files\n", du_scan_count);
        }

        aggregate_sizes();

        for (size_t i = 0; i < du_entries.size(); i++) {
            DuEntry *e = du_entries[i];

            /* -a: show all files. Default: only dirs */
            if ((opts.all == 0) && (e->is_dir == 0))
            {
                continue;
            }

            /* -s: only root-level entries (depth 0) */
            if ((opts.summarize != 0) && e->depth > 0)
            {
                continue;
            }

            /* --max-depth: skip deeper entries */
            if (opts.max_depth >= 0 && e->depth > opts.max_depth)
            {
                continue;
            }

            /* -S (separate-dirs): use own size, not aggregated */
            if ((opts.separate_dirs != 0) && (e->is_dir != 0)) {
                e->agg_size = e->size_bytes;
            }

            /* Skip if already printed (dedup overlapping paths) */
            if (printed.find(e->path) != printed.end())
            {
                continue;
            }
            printed[e->path] = 1;

            if (json_mode) {
                json_entries.push_back(e);
            } else {
                print_entry(e, &opts, &grand_total);
            }
        }

        /* Cleanup this path's entries (skip in JSON mode, done at end) */
        if (!json_mode) {
            for (size_t i = 0; i < du_entries.size(); i++) {
                const DuEntry *e = du_entries[i];
                free(e->path);
                delete e;
            }
            du_entries.clear();
        }
    }

    if (json_mode) {
        (void)fprintf(stdout, "[\n");
        for (size_t i = 0; i < json_entries.size(); i++) {
            const DuEntry *e = json_entries[i];
            (void)fprintf(stdout, "  {\n");
            (void)fprintf(stdout, "    \"path\": ");
            json_escape_string(stdout, e->path);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"size_bytes\": %llu,\n", static_cast<unsigned long long>(e->size_bytes));
            (void)fprintf(stdout, "    \"agg_size\": %llu,\n", static_cast<unsigned long long>(e->agg_size));
            (void)fprintf(stdout, "    \"depth\": %d,\n", e->depth);
            (void)fprintf(stdout, "    \"is_dir\": %s,\n", (e->is_dir != 0) ? "true" : "false");
            (void)fprintf(stdout, "    \"is_error\": %s,\n", (e->is_error != 0) ? "true" : "false");
            (void)fprintf(stdout, "    \"mtime\": %ld\n", static_cast<long>(e->mtime));
            (void)fprintf(stdout, "  }%s\n", (i + 1 < json_entries.size()) ? "," : "");
        }
        if (opts.total != 0) {
            if (!json_entries.empty())
            {
                (void)fprintf(stdout, "  ,\n");
            }
            (void)fprintf(stdout, "  {\n");
            (void)fprintf(stdout, "    \"path\": ");
            json_escape_string(stdout, "total");
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"total\": %llu\n", static_cast<unsigned long long>(grand_total));
            (void)fprintf(stdout, "  }\n");
        }
        (void)fprintf(stdout, "]\n");

        for (size_t i = 0; i < json_entries.size(); i++) {
            const DuEntry *e = json_entries[i];
            free(e->path);
            delete e;
        }
        json_entries.clear();
    } else {
        /* Cleanup this path's entries */
        for (size_t i = 0; i < du_entries.size(); i++) {
            const DuEntry *e = du_entries[i];
            free(e->path);
            delete e;
        }
        du_entries.clear();
    }

    if (!json_mode) {
        if (opts.total != 0) {
            char total_buf[64];
            format_size(grand_total, &opts, total_buf, sizeof(total_buf));
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stdout, "%s\ttotal%s",
                          total_buf,
                          (opts.null_terminated != 0) ? "\0" : "\n");
        }
    }

    if (opts.exclude != nullptr) {
        free(opts.exclude);
    }
    return 0;
}

REGISTER_COMMAND("du", du_command, "Estimate file space usage");
