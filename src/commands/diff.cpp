#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <argtable3.h>

#include "commands/diff.hpp"
#include "commands/command_macros.hpp"

/* ── Constants ──────────────────────────────────────────────────────────── */

#define DIFF_MAX_LINE 1048576   /* 1 MiB max line length */

/* ── Diff operations ────────────────────────────────────────────────────── */

enum class DiffOp { EQUAL, DELETE, INSERT, REPLACE };

struct DiffChange {
    DiffOp op;
    int old_start, old_count;   /* 0-indexed, half-open [start, start+count) */
    int new_start, new_count;
};

/* ── Line comparison helpers ────────────────────────────────────────────── */

static bool lines_match(const char* a, const char* b, const DiffOptions* opts) {
    if (opts->ignore_all_space) {
        while (*a && *b) {
            while (*a && (unsigned char)*a <= ' ' && *a != '\n') a++;
            while (*b && (unsigned char)*b <= ' ' && *b != '\n') b++;
            if (!*a || !*b) break;
            if (opts->ignore_case) {
                if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
                    return false;
            } else {
                if (*a != *b) return false;
            }
            a++; b++;
        }
        while (*a && (unsigned char)*a <= ' ' && *a != '\n') a++;
        while (*b && (unsigned char)*b <= ' ' && *b != '\n') b++;
        return (*a == '\0' || *a == '\n') && (*b == '\0' || *b == '\n');
    }
    if (opts->ignore_space_change) {
        while (*a && *b) {
            int ws_a = ((unsigned char)*a <= ' ' && *a != '\n');
            int ws_b = ((unsigned char)*b <= ' ' && *b != '\n');
            if (ws_a || ws_b) {
                if (!ws_a || !ws_b) return false;
                while (*a && (unsigned char)*a <= ' ' && *a != '\n') a++;
                while (*b && (unsigned char)*b <= ' ' && *b != '\n') b++;
                continue;
            }
            if (opts->ignore_case) {
                if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
                    return false;
            } else {
                if (*a != *b) return false;
            }
            a++; b++;
        }
        return (*a == '\0' || *a == '\n') && (*b == '\0' || *b == '\n');
    }
    if (opts->ignore_case) {
        while (*a && *b) {
            if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
                return false;
            a++; b++;
        }
        return *a == *b;
    }
    return strcmp(a, b) == 0;
}

/* ── Read file into lines ──────────────────────────────────────────────── */

static char** read_lines(const char* filename, int* out_count) {
    FILE* fp = (strcmp(filename, "-") == 0) ? stdin : fopen(filename, "r");
    if (fp == NULL) {
        *out_count = -1;
        return NULL;
    }

    int cap = 1024;
    int n = 0;
    auto lines = (char**)malloc((size_t)cap * sizeof(char*));
    if (!lines) { *out_count = -1; if (fp != stdin) fclose(fp); return NULL; }

    char buf[DIFF_MAX_LINE];
    while (fgets(buf, DIFF_MAX_LINE, fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
        if (n >= cap) {
            cap *= 2;
            lines = (char**)realloc(lines, (size_t)cap * sizeof(char*));
            if (!lines) { *out_count = -1; if (fp != stdin) fclose(fp); return NULL; }
        }
        lines[n] = strdup(buf);
        if (!lines[n]) { *out_count = -1; if (fp != stdin) fclose(fp); return NULL; }
        n++;
    }

    if (fp != stdin) fclose(fp);
    *out_count = n;
    return lines;
}

static void free_lines(char** lines, int count) {
    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

/* ── LCS-based diff engine ─────────────────────────────────────────────── */

static bool is_same_line(const char* a, const char* b, const DiffOptions* opts) {
    return lines_match(a, b, opts);
}

/* Find the shortest edit script using a simple LCS approach.
 *
 * First we strip common prefix and suffix, then compute LCS on the middle.
 * The result is a list of DiffChange operations in chronological order. */
static std::vector<DiffChange> compute_changes(char** old_lines, int old_len,
                                                char** new_lines, int new_len,
                                                const DiffOptions* opts) {
    std::vector<DiffChange> result;

    if (old_len == 0 && new_len == 0) return result;

    /* Strip common prefix */
    int pref = 0;
    while (pref < old_len && pref < new_len &&
           is_same_line(old_lines[pref], new_lines[pref], opts)) {
        pref++;
    }

    /* Strip common suffix */
    int suff = 0;
    while (suff < old_len - pref && suff < new_len - pref &&
           is_same_line(old_lines[old_len - 1 - suff],
                        new_lines[new_len - 1 - suff], opts)) {
        suff++;
    }

    int mid_old_start = pref;
    int mid_old_end = old_len - suff;
    int mid_new_start = pref;
    int mid_new_end = new_len - suff;

    int mid_old_len = mid_old_end - mid_old_start;
    int mid_new_len = mid_new_end - mid_new_start;

    if (mid_old_len == 0 && mid_new_len == 0) {
        return result; /* identical */
    }

    if (mid_old_len == 0) {
        /* Only insertions */
        result.push_back({DiffOp::INSERT, mid_old_start, 0, mid_new_start, mid_new_len});
        return result;
    }
    if (mid_new_len == 0) {
        /* Only deletions */
        result.push_back({DiffOp::DELETE, mid_old_start, mid_old_len, mid_new_start, 0});
        return result;
    }

    /* Build LCS table for middle section */
    std::vector<std::vector<int>> lcs(mid_old_len + 1,
                                      std::vector<int>(mid_new_len + 1, 0));

    for (int i = 1; i <= mid_old_len; i++) {
        for (int j = 1; j <= mid_new_len; j++) {
            if (is_same_line(old_lines[mid_old_start + i - 1],
                             new_lines[mid_new_start + j - 1], opts)) {
                lcs[i][j] = lcs[i-1][j-1] + 1;
            } else {
                lcs[i][j] = std::max(lcs[i-1][j], lcs[i][j-1]);
            }
        }
    }

    /* Backtrack to find the edit sequence (in reverse) */
    std::vector<DiffChange> rev;
    int i = mid_old_len;
    int j = mid_new_len;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 &&
            is_same_line(old_lines[mid_old_start + i - 1],
                         new_lines[mid_new_start + j - 1], opts)) {
            /* Equal — skip */
            i--; j--;
        } else if (j > 0 && (i == 0 || lcs[i][j-1] >= lcs[i-1][j])) {
            /* Insertion: one or more new lines inserted */
            int j_end = j;
            while (j > 0 && (i == 0 || lcs[i][j-1] >= lcs[i-1][j])) {
                j--;
            }
            rev.push_back({DiffOp::INSERT, mid_old_start + i, 0,
                           mid_new_start + j, j_end - j});
        } else if (i > 0) {
            /* Deletion: one or more old lines removed */
            int i_end = i;
            while (i > 0 && (j == 0 || lcs[i-1][j] >= lcs[i][j-1])) {
                i--;
            }
            rev.push_back({DiffOp::DELETE, mid_old_start + i, i_end - i,
                           mid_new_start + j, 0});
        }
    }

    /* Reverse to get chronological order */
    for (int k = (int)rev.size() - 1; k >= 0; k--) {
        result.push_back(rev[k]);
    }

    /* Merge adjacent delete+insert into replace where they occur at the
       same logical position (the delete range end == insert range start,
       and the insert was right after the delete). */
    std::vector<DiffChange> merged;
    size_t k = 0;
    while (k < result.size()) {
        if (k + 1 < result.size() &&
            result[k].op == DiffOp::DELETE &&
            result[k+1].op == DiffOp::INSERT &&
            result[k].old_start + result[k].old_count == result[k+1].old_start) {
            /* Merge: the delete and insert happened at the same location */
            merged.push_back({DiffOp::REPLACE,
                              result[k].old_start, result[k].old_count,
                              result[k+1].new_start, result[k+1].new_count});
            k += 2;
        } else {
            merged.push_back(result[k]);
            k++;
        }
    }

    return merged;
}

/* ── Output format: range string ────────────────────────────────────────── */

/* Print a line range, e.g. "1" or "1,3".
 * count == 0 prints just the anchor line number (for "a" and "d" commands). */
static void print_range(int start, int count) {
    if (count <= 1) {
        printf("%d", start + (count == 0 ? 0 : 0));
        if (count == 0) {
            /* Print the line number after the deleted range, or before the inserted range.
             * For "d" (delete): the range before deletion.
             * For "a" (append): the line before insertion. */
        }
    }
    /* For normal format, the range format depends on the command type.
     * We handle it directly in the output functions instead. */
}

/* Print a line range for normal diff: "line" or "line1,line2" (1-indexed) */
static void print_range_normal(int start, int count) {
    if (count <= 1) {
        printf("%d", start + 1);
    } else {
        printf("%d,%d", start + 1, start + count);
    }
}

/* ── Normal format output ───────────────────────────────────────────────── */

static void output_normal(const std::vector<DiffChange>& changes,
                           char** old_lines, int old_len,
                           char** new_lines, int new_len,
                           const DiffOptions* opts) {
    (void)old_len;
    (void)new_len;
    (void)opts;

    if (changes.empty()) return;

    for (auto& ch : changes) {
        switch (ch.op) {
        case DiffOp::DELETE: {
            /* "old_start[,old_end]dnew_start" — delete range in old file,
             * new file only has the line number of the first removed line */
            print_range_normal(ch.old_start, ch.old_count);
            printf("d%d\n", ch.new_start + 1);
            for (int i = 0; i < ch.old_count; i++) {
                printf("< %s\n", old_lines[ch.old_start + i]);
            }
            break;
        }
        case DiffOp::INSERT: {
            /* "old_starta new_start[,new_end]" — append after old_start
             * old_start is the line before the insertion point */
            int anchor = ch.old_start; /* insert after this line */
            printf("%da", anchor + 1);
            print_range_normal(ch.new_start, ch.new_count);
            printf("\n");
            for (int i = 0; i < ch.new_count; i++) {
                printf("> %s\n", new_lines[ch.new_start + i]);
            }
            break;
        }
        case DiffOp::REPLACE: {
            /* "old_start[,old_end]c new_start[,new_end]" */
            print_range_normal(ch.old_start, ch.old_count);
            printf("c");
            print_range_normal(ch.new_start, ch.new_count);
            printf("\n");
            for (int i = 0; i < ch.old_count; i++) {
                printf("< %s\n", old_lines[ch.old_start + i]);
            }
            printf("---\n");
            for (int i = 0; i < ch.new_count; i++) {
                printf("> %s\n", new_lines[ch.new_start + i]);
            }
            break;
        }
        default: break;
        }
    }
}

/* ── Unified format output ─────────────────────────────────────────────── */

static void output_unified(const std::vector<DiffChange>& changes,
                            char** old_lines, int old_len,
                            char** new_lines, int new_len,
                            const char* file1, const char* file2,
                            const DiffOptions* opts) {
    if (changes.empty()) return;

    int ctx = opts->context_lines;

    printf("--- %s\n", file1);
    printf("+++ %s\n", file2);

    /* Build change maps */
    std::vector<bool> changed_old(old_len, false);
    std::vector<bool> changed_new(new_len, false);
    for (auto& ch : changes) {
        if (ch.op == DiffOp::DELETE || ch.op == DiffOp::REPLACE) {
            for (int i = 0; i < ch.old_count; i++)
                changed_old[ch.old_start + i] = true;
        }
        if (ch.op == DiffOp::INSERT || ch.op == DiffOp::REPLACE) {
            for (int i = 0; i < ch.new_count; i++)
                changed_new[ch.new_start + i] = true;
        }
    }

    /* Find hunk ranges: walk through files and group changes with context */
    struct HunkRange {
        int old_start, old_end;
        int new_start, new_end;
    };
    std::vector<HunkRange> hunks;

    {
        int o = 0, n = 0;
        while (o < old_len || n < new_len) {
            /* Find next change */
            int hunk_old_start = old_len, hunk_new_start = new_len;
            for (int j = o; j < old_len; j++) {
                if (changed_old[j]) { hunk_old_start = j; break; }
            }
            for (int j = n; j < new_len; j++) {
                if (changed_new[j]) { hunk_new_start = j; break; }
            }
            if (hunk_old_start >= old_len && hunk_new_start >= new_len) break;

            int hunk_start = std::min(hunk_old_start, hunk_new_start);
            int hunk_old_end = hunk_start;
            int hunk_new_end = hunk_start;

            /* Expand to cover all changes with context */
            bool expanded = true;
            while (expanded) {
                expanded = false;
                for (int j = hunk_start; j < std::min(hunk_old_end + ctx + 1, old_len); j++) {
                    if (changed_old[j] && j >= hunk_old_end) {
                        hunk_old_end = j + 1;
                        expanded = true;
                    }
                }
                for (int j = hunk_start; j < std::min(hunk_new_end + ctx + 1, new_len); j++) {
                    if (changed_new[j] && j >= hunk_new_end) {
                        hunk_new_end = j + 1;
                        expanded = true;
                    }
                }
            }

            /* Add context on both sides */
            hunk_start = std::max(0, hunk_start - ctx);
            hunk_old_end = std::min(old_len, hunk_old_end + ctx);
            hunk_new_end = std::min(new_len, hunk_new_end + ctx);

            hunks.push_back({hunk_start, hunk_old_end, hunk_start, hunk_new_end});
            o = hunk_old_end;
            n = hunk_new_end;
        }
    }

    /* Output hunks */
    for (auto& hunk : hunks) {
        int old_count = hunk.old_end - hunk.old_start;
        int new_count = hunk.new_end - hunk.new_start;
        printf("@@ -%d,%d +%d,%d @@\n",
               hunk.old_start + 1, old_count,
               hunk.new_start + 1, new_count);

        int o = hunk.old_start;
        int n = hunk.new_start;
        while (o < hunk.old_end || n < hunk.new_end) {
            if (o < hunk.old_end && n < hunk.new_end &&
                is_same_line(old_lines[o], new_lines[n], opts) &&
                !changed_old[o] && !changed_new[n]) {
                printf(" %s\n", old_lines[o]);
                o++; n++;
            } else if (o < hunk.old_end && changed_old[o]) {
                printf("-%s\n", old_lines[o]);
                o++;
            } else if (n < hunk.new_end && changed_new[n]) {
                printf("+%s\n", new_lines[n]);
                n++;
            } else if (o < hunk.old_end && n < hunk.new_end &&
                       !is_same_line(old_lines[o], new_lines[n], opts)) {
                if (changed_old[o]) { printf("-%s\n", old_lines[o]); o++; }
                if (changed_new[n]) { printf("+%s\n", new_lines[n]); n++; }
            } else if (o < hunk.old_end) {
                printf(" %s\n", old_lines[o]); o++;
                if (n < hunk.new_end) n++;
            } else if (n < hunk.new_end) {
                printf("+%s\n", new_lines[n]); n++;
            }
        }
    }
}

/* ── Context format output ──────────────────────────────────────────────── */

static void output_context(const std::vector<DiffChange>& changes,
                            char** old_lines, int old_len,
                            char** new_lines, int new_len,
                            const char* file1, const char* file2,
                            const DiffOptions* opts) {
    if (changes.empty()) return;

    int ctx = opts->context_lines;

    printf("*** %s\n", file1);
    printf("--- %s\n", file2);

    /* Build change flags */
    std::vector<char> old_flag(old_len, ' ');
    std::vector<char> new_flag(new_len, ' ');

    for (auto& ch : changes) {
        switch (ch.op) {
        case DiffOp::DELETE:
            for (int i = 0; i < ch.old_count; i++)
                old_flag[ch.old_start + i] = '-';
            break;
        case DiffOp::INSERT:
            for (int i = 0; i < ch.new_count; i++)
                new_flag[ch.new_start + i] = '+';
            break;
        case DiffOp::REPLACE:
            for (int i = 0; i < ch.old_count; i++)
                old_flag[ch.old_start + i] = '!';
            for (int i = 0; i < ch.new_count; i++)
                new_flag[ch.new_start + i] = '!';
            break;
        default: break;
        }
    }

    /* Find hunk ranges (same logic as unified) */
    std::vector<bool> changed_old(old_len, false);
    std::vector<bool> changed_new(new_len, false);
    for (int j = 0; j < old_len; j++) changed_old[j] = (old_flag[j] != ' ');
    for (int j = 0; j < new_len; j++) changed_new[j] = (new_flag[j] != ' ');

    struct HunkRange {
        int old_start, old_end;
        int new_start, new_end;
    };
    std::vector<HunkRange> hunks;

    {
        int o = 0, n = 0;
        while (o < old_len || n < new_len) {
            int hunk_old_start = old_len, hunk_new_start = new_len;
            for (int j = o; j < old_len; j++) {
                if (changed_old[j]) { hunk_old_start = j; break; }
            }
            for (int j = n; j < new_len; j++) {
                if (changed_new[j]) { hunk_new_start = j; break; }
            }
            if (hunk_old_start >= old_len && hunk_new_start >= new_len) break;

            int hunk_start = std::min(hunk_old_start, hunk_new_start);
            int hunk_old_end = hunk_start;
            int hunk_new_end = hunk_start;

            bool expanded = true;
            while (expanded) {
                expanded = false;
                for (int j = hunk_start; j < std::min(hunk_old_end + ctx + 1, old_len); j++) {
                    if (changed_old[j] && j >= hunk_old_end) {
                        hunk_old_end = j + 1;
                        expanded = true;
                    }
                }
                for (int j = hunk_start; j < std::min(hunk_new_end + ctx + 1, new_len); j++) {
                    if (changed_new[j] && j >= hunk_new_end) {
                        hunk_new_end = j + 1;
                        expanded = true;
                    }
                }
            }

            hunk_start = std::max(0, hunk_start - ctx);
            hunk_old_end = std::min(old_len, hunk_old_end + ctx);
            hunk_new_end = std::min(new_len, hunk_new_end + ctx);

            hunks.push_back({hunk_start, hunk_old_end, hunk_start, hunk_new_end});
            o = hunk_old_end;
            n = hunk_new_end;
        }
    }

    /* Output hunks */
    for (auto& hunk : hunks) {
        printf("***************\n");
        /* Old file section */
        printf("*** %d", hunk.old_start + 1);
        if (hunk.old_end - hunk.old_start != 1)
            printf(",%d", hunk.old_end);
        printf(" ****\n");
        for (int i = hunk.old_start; i < hunk.old_end; i++) {
            printf("%c %s\n", old_flag[i], old_lines[i]);
        }

        /* New file section */
        printf("--- %d", hunk.new_start + 1);
        if (hunk.new_end - hunk.new_start != 1)
            printf(",%d", hunk.new_end);
        printf(" ----\n");
        for (int i = hunk.new_start; i < hunk.new_end; i++) {
            printf("%c %s\n", new_flag[i], new_lines[i]);
        }
    }
}

/* ── Brief mode ─────────────────────────────────────────────────────────── */

static int files_differ(char** old_lines, int old_len,
                         char** new_lines, int new_len,
                         const DiffOptions* opts) {
    if (old_len != new_len) return 1;
    for (int i = 0; i < old_len; i++) {
        if (!is_same_line(old_lines[i], new_lines[i], opts))
            return 1;
    }
    return 0;
}

/* ── Main command ───────────────────────────────────────────────────────── */

void diff_command(int argc, char** argv) {
    DiffOptions opts = {0};
    opts.context_lines = 3;

    struct arg_lit* brief_opt = arg_lit0("q", "brief", "report only whether files differ");
    struct arg_lit* report_identical_opt = arg_lit0("s", "report-identical-files", "report when two files are identical");
    struct arg_lit* ignore_case_opt = arg_lit0("i", "ignore-case", "ignore case differences");
    struct arg_lit* ignore_all_space_opt = arg_lit0("w", "ignore-all-space", "ignore all whitespace");
    struct arg_lit* ignore_space_change_opt = arg_lit0("b", "ignore-space-change", "ignore changes in whitespace amount");
    struct arg_lit* show_c_function_opt = arg_lit0("p", "show-c-function", "show which C function each change is in");
    struct arg_lit* expand_tabs_opt = arg_lit0("t", "expand-tabs", "expand tabs to spaces in output");
    struct arg_lit* unified_opt = arg_lit0("u", "unified", "output unified diff format (default 3 lines context)");
    struct arg_int* unified_ctx_opt = arg_int0("U", "unified-context", "LINES", "output unified diff with LINES lines of context");
    struct arg_lit* context_opt = arg_lit0("c", "context", "output context diff format (default 3 lines context)");
    struct arg_int* context_ctx_opt = arg_int0("C", "context-context", "LINES", "output context diff with LINES lines of context");
    struct arg_int* color_opt = arg_int0(nullptr, "color", "WHEN", "colorize output (always, auto, never)");
    struct arg_lit* normal_opt = arg_lit0(nullptr, "normal", "output a normal diff (default)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file1_arg = arg_filen(NULL, NULL, "FILE1", 1, 1, "first file (or - for stdin)");
    struct arg_file* file2_arg = arg_filen(NULL, NULL, "FILE2", 1, 1, "second file (or - for stdin)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        brief_opt, report_identical_opt,
        ignore_case_opt, ignore_all_space_opt, ignore_space_change_opt,
        show_c_function_opt, expand_tabs_opt,
        unified_opt, unified_ctx_opt, context_opt, context_ctx_opt,
        color_opt, normal_opt,
        help_opt,
        file1_arg, file2_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... FILE1 FILE2\n", argv[0]);
        printf("Compare files line by line.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -q, --brief                   report only whether files differ\n");
        printf("  -s, --report-identical-files   report when two files are identical\n");
        printf("  -i, --ignore-case             ignore case differences\n");
        printf("  -w, --ignore-all-space        ignore all whitespace\n");
        printf("  -b, --ignore-space-change     ignore changes in whitespace amount\n");
        printf("  -p, --show-c-function         show which C function each change is in\n");
        printf("  -t, --expand-tabs             expand tabs to spaces in output\n");
        printf("  -u, --unified                 output unified diff format (3 lines context)\n");
        printf("  -U, --unified-context=LINES   output unified diff with LINES lines of context\n");
        printf("  -c, --context                 output context diff format (3 lines context)\n");
        printf("  -C, --context-context=LINES   output context diff with LINES lines\n");
        printf("      --color=WHEN              colorize output (always, auto, never)\n");
        printf("      --normal                  output a normal diff (default)\n");
        printf("  -h, --help                    display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.brief = (brief_opt->count > 0);
    opts.report_identical = (report_identical_opt->count > 0);
    opts.ignore_case = (ignore_case_opt->count > 0);
    opts.ignore_all_space = (ignore_all_space_opt->count > 0);
    opts.ignore_space_change = (ignore_space_change_opt->count > 0);
    opts.show_c_function = (show_c_function_opt->count > 0);
    opts.expand_tabs = (expand_tabs_opt->count > 0);
    opts.color = (color_opt->count > 0 ? color_opt->ival[0] : 0);

    /* Determine format */
    if (unified_opt->count > 0) {
        opts.format = 1;
        opts.context_lines = (unified_ctx_opt->count > 0) ? unified_ctx_opt->ival[0] : 3;
    } else if (unified_ctx_opt->count > 0) {
        opts.format = 1;
        opts.context_lines = unified_ctx_opt->ival[0];
    } else if (context_opt->count > 0) {
        opts.format = 2;
        opts.context_lines = (context_ctx_opt->count > 0) ? context_ctx_opt->ival[0] : 3;
    } else if (context_ctx_opt->count > 0) {
        opts.format = 2;
        opts.context_lines = context_ctx_opt->ival[0];
    } else {
        opts.format = 0; /* normal */
    }

    const char* file1 = file1_arg->filename[0];
    const char* file2 = file2_arg->filename[0];

    int old_len, new_len;
    char** old_lines = read_lines(file1, &old_len);
    if (old_lines == NULL && old_len == -1) {
        fprintf(stderr, "diff: %s: No such file or directory\n", file1);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    char** new_lines = read_lines(file2, &new_len);
    if (new_lines == NULL && new_len == -1) {
        fprintf(stderr, "diff: %s: No such file or directory\n", file2);
        free_lines(old_lines, old_len);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (opts.brief) {
        int differ = files_differ(old_lines, old_len, new_lines, new_len, &opts);
        if (differ) {
            printf("Files %s and %s differ\n", file1, file2);
        } else if (opts.report_identical) {
            printf("Files %s and %s are identical\n", file1, file2);
        }
        free_lines(old_lines, old_len);
        free_lines(new_lines, new_len);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    auto changes = compute_changes(old_lines, old_len, new_lines, new_len, &opts);

    if (changes.empty()) {
        if (opts.report_identical) {
            printf("Files %s and %s are identical\n", file1, file2);
        }
    } else {
        switch (opts.format) {
        case 1:
            output_unified(changes, old_lines, old_len, new_lines, new_len,
                           file1, file2, &opts);
            break;
        case 2:
            output_context(changes, old_lines, old_len, new_lines, new_len,
                           file1, file2, &opts);
            break;
        default:
            output_normal(changes, old_lines, old_len, new_lines, new_len, &opts);
            break;
        }
    }

    free_lines(old_lines, old_len);
    free_lines(new_lines, new_len);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("diff", diff_command, "Compare files line by line");
