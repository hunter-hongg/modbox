#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "commands/arg_util.hpp"

#include "commands/diff3.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/diff.hpp"

/* ── Constants ──────────────────────────────────────────────────────────── */

#define DIFF3_MAX_LINE 1048576   /* 1 MiB max line length */

/* ── Diff3 modes ────────────────────────────────────────────────────────── */

enum class Diff3Output {
    DEFAULT,    /* human-readable three-file output */
    ED_SCRIPT,  /* ed script format */
    MERGE,      /* merged output with conflict markers */
    SHOW_OVERLAP, /* -E: like -e but with conflict markers */
    EASY_ONLY,  /* -3: non-overlapping changes only */
    OVERLAP_ONLY, /* -x: overlapping changes only */
    OVERLAP_X   /* -X: like -x but with conflict markers */
};

struct Diff3Options {
    Diff3Output output = Diff3Output::DEFAULT;
    int show_all = 0;       // -A (implies MERGE)
    int strip_cr = 0;       // --strip-trailing-cr
    int initial_tab = 0;    // -T
    std::string label[3];   // -L labels
    std::string diff_program; // --diff-program
};

/* ── Line comparison helpers (reuse diff's logic) ───────────────────────── */

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

    char buf[DIFF3_MAX_LINE];
    while (fgets(buf, DIFF3_MAX_LINE, fp)) {
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

/* Simple line equality check */
static bool lines_equal(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

/* ── LCS-based diff (simplified version) ────────────────────────────────── */

/* Compute edit script between two files using LCS.
 * Returns a list of operations: KEEP, DELETE (old), INSERT (new). */
struct EditOp {
    enum Type { KEEP, DELETE, INSERT } type;
    int old_idx;   /* index in old file (for KEEP/DELETE) */
    int new_idx;   /* index in new file (for KEEP/INSERT) */
};

static std::vector<EditOp> compute_edit_script(char** old_lines, int old_len,
                                                char** new_lines, int new_len) {
    std::vector<EditOp> result;

    if (old_len == 0 && new_len == 0) return result;

    /* Build LCS table */
    std::vector<std::vector<int>> lcs(old_len + 1, std::vector<int>(new_len + 1, 0));

    for (int i = 1; i <= old_len; i++) {
        for (int j = 1; j <= new_len; j++) {
            if (lines_equal(old_lines[i - 1], new_lines[j - 1])) {
                lcs[i][j] = lcs[i-1][j-1] + 1;
            } else {
                lcs[i][j] = std::max(lcs[i-1][j], lcs[i][j-1]);
            }
        }
    }

    /* Backtrack */
    std::vector<EditOp> rev;
    int i = old_len, j = new_len;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && lines_equal(old_lines[i-1], new_lines[j-1])) {
            rev.push_back({EditOp::KEEP, i - 1, j - 1});
            i--; j--;
        } else if (j > 0 && (i == 0 || lcs[i][j-1] >= lcs[i-1][j])) {
            rev.push_back({EditOp::INSERT, -1, j - 1});
            j--;
        } else if (i > 0) {
            rev.push_back({EditOp::DELETE, i - 1, -1});
            i--;
        }
    }

    /* Reverse to chronological order */
    for (int k = (int)rev.size() - 1; k >= 0; k--) {
        result.push_back(rev[k]);
    }

    return result;
}

/* ── Merge algorithm ────────────────────────────────────────────────────── */

/* A change from OLD to YOURS is "overlapping" with a change from OLD to MINE
 * if they modify the same region of OLD.
 *
 * We represent changes as ranges in the old file: [start, end). */
struct ChangeRange {
    int old_start;
    int old_end;     /* exclusive */
    int new_start;   /* start in the new file's lines */
    int new_count;   /* number of lines */
};

/* Convert an edit script into change ranges. */
static std::vector<ChangeRange> edit_script_to_changes(const std::vector<EditOp>& script,
                                                        int old_len, int new_len) {
    std::vector<ChangeRange> changes;
    int oi = 0, ni = 0;
    int del_start = -1;
    int ins_start = -1;
    int ins_count = 0;

    for (auto& op : script) {
        switch (op.type) {
        case EditOp::KEEP:
            /* Flush any pending changes */
            if (del_start >= 0 || ins_count > 0) {
                if (del_start >= 0) {
                    changes.push_back({del_start, oi, ins_start, ins_count});
                } else {
                    changes.push_back({oi, oi, ins_start, ins_count});
                }
                del_start = -1;
                ins_count = 0;
            }
            oi++; ni++;
            break;
        case EditOp::DELETE:
            if (del_start < 0) del_start = oi;
            oi++;
            break;
        case EditOp::INSERT:
            if (ins_count == 0) ins_start = ni;
            ins_count++;
            ni++;
            break;
        }
    }

    /* Flush final changes */
    if (del_start >= 0 || ins_count > 0) {
        if (del_start >= 0) {
            changes.push_back({del_start, oi, ins_start, ins_count});
        } else {
            changes.push_back({oi, oi, ins_start, ins_count});
        }
    }

    return changes;
}

/* Check if two change ranges overlap in the old file. */
static bool ranges_overlap(const ChangeRange& a, const ChangeRange& b) {
    if (a.old_start >= b.old_end || b.old_start >= a.old_end) return false;
    return true;
}

/* ── Output: default human-readable ─────────────────────────────────────── */

static void output_default(int file1_n, char** file1_lines,
                            int file2_n, char** file2_lines,
                            int file3_n, char** file3_lines,
                            const std::string label[3],
                            const Diff3Options& opts) {
    /* Compare file2 (base) -> file1 (mine) and file2 (base) -> file3 (yours) */
    auto mine_script = compute_edit_script(file2_lines, file2_n, file1_lines, file1_n);
    auto your_script = compute_edit_script(file2_lines, file2_n, file3_lines, file3_n);

    auto mine_changes = edit_script_to_changes(mine_script, file2_n, file1_n);
    auto your_changes = edit_script_to_changes(your_script, file2_n, file3_n);

    /* Walk through both change lists simultaneously */
    int mi = 0, yi = 0;

    while (mi < (int)mine_changes.size() || yi < (int)your_changes.size()) {
        /* If one side has no more changes, just show the remaining changes */
        if (mi >= (int)mine_changes.size()) {
            /* Only yours changed */
            auto& yc = your_changes[yi];
            printf("%d,%da%d,%d\n", yc.old_start + 1, yc.old_end,
                   yc.new_start + 1, yc.new_start + yc.new_count);
            for (int k = 0; k < yc.new_count; k++) {
                printf("< %s\n", file3_lines[yc.new_start + k]);
            }
            yi++;
            continue;
        }
        if (yi >= (int)your_changes.size()) {
            /* Only mine changed */
            auto& mc = mine_changes[mi];
            printf("%d,%da%d,%d\n", mc.old_start + 1, mc.old_end,
                   mc.new_start + 1, mc.new_start + mc.new_count);
            for (int k = 0; k < mc.new_count; k++) {
                printf("> %s\n", file1_lines[mc.new_start + k]);
            }
            mi++;
            continue;
        }

        auto& mc = mine_changes[mi];
        auto& yc = your_changes[yi];

        /* Order by position in old file */
        int min_pos = std::min(mc.old_start, yc.old_start);
        int max_pos = std::max(mc.old_end, yc.old_end);

        /* Find all changes within this range */
        std::vector<const ChangeRange*> relevant_mine;
        std::vector<const ChangeRange*> relevant_your;

        for (int j = mi; j < (int)mine_changes.size() && j <= mi + 10; j++) {
            auto& c = mine_changes[j];
            if (c.old_end > max_pos && c.old_start >= max_pos) break;
            if (c.old_end > min_pos && c.old_start < max_pos) {
                relevant_mine.push_back(&c);
            }
        }
        for (int j = yi; j < (int)your_changes.size() && j <= yi + 10; j++) {
            auto& c = your_changes[j];
            if (c.old_end > max_pos && c.old_start >= max_pos) break;
            if (c.old_end > min_pos && c.old_start < max_pos) {
                relevant_your.push_back(&c);
            }
        }

        bool overlap = !relevant_mine.empty() && !relevant_your.empty();

        if (overlap && ranges_overlap(*relevant_mine[0], *relevant_your[0])) {
            /* Conflict: both changed the same region */
            printf("%d,%dc%d,%d^%d,%d\n",
                   relevant_mine[0]->old_start + 1, relevant_mine[0]->old_end,
                   relevant_mine[0]->new_start + 1, relevant_mine[0]->new_start + relevant_mine[0]->new_count,
                   relevant_your[0]->new_start + 1, relevant_your[0]->new_start + relevant_your[0]->new_count);

            for (int k = 0; k < relevant_mine[0]->new_count; k++) {
                printf("> %s\n", file1_lines[relevant_mine[0]->new_start + k]);
            }
            printf("---\n");
            for (int k = 0; k < relevant_your[0]->new_count; k++) {
                printf("< %s\n", file3_lines[relevant_your[0]->new_start + k]);
            }
            mi++;
            yi++;
        } else {
            /* No conflict — show whichever comes first */
            if (!relevant_mine.empty()) {
                auto& rc = *relevant_mine[0];
                printf("%d,%da%d,%d\n", rc.old_start + 1, rc.old_end,
                       rc.new_start + 1, rc.new_start + rc.new_count);
                for (int k = 0; k < rc.new_count; k++) {
                    printf("> %s\n", file1_lines[rc.new_start + k]);
                }
                mi++;
            }
            if (!relevant_your.empty()) {
                auto& rc = *relevant_your[0];
                printf("%d,%da%d,%d\n", rc.old_start + 1, rc.old_end,
                       rc.new_start + 1, rc.new_start + rc.new_count);
                for (int k = 0; k < rc.new_count; k++) {
                    printf("< %s\n", file3_lines[rc.new_start + k]);
                }
                yi++;
            }
        }
    }
}

/* ── Output: ed script format ───────────────────────────────────────────── */

static void output_ed(int file1_n, char** file1_lines,
                       int file2_n, char** file2_lines,
                       int file3_n, char** file3_lines,
                       const Diff3Options& opts) {
    auto mine_script = compute_edit_script(file2_lines, file2_n, file1_lines, file1_n);
    auto your_script = compute_edit_script(file2_lines, file2_n, file3_lines, file3_n);

    auto mine_changes = edit_script_to_changes(mine_script, file2_n, file1_n);
    auto your_changes = edit_script_to_changes(your_script, file2_n, file3_n);

    /* Walk through old file line by line, outputting ed commands */
    int mi = 0, yi = 0;
    int old_pos = 0;

    while (old_pos < file2_n || mi < (int)mine_changes.size() || yi < (int)your_changes.size()) {
        /* Determine what happens at this position */
        bool mine_here = (mi < (int)mine_changes.size() &&
                          mine_changes[mi].old_start <= old_pos &&
                          mine_changes[mi].old_end >= old_pos);
        bool your_here = (yi < (int)your_changes.size() &&
                          your_changes[yi].old_start <= old_pos &&
                          your_changes[yi].old_end >= old_pos);

        if (mine_here && your_here) {
            /* Both changed — conflict */
            printf("%da\n", old_pos);
            for (int k = 0; k < mine_changes[mi].new_count; k++) {
                printf("\t%s\n", file1_lines[mine_changes[mi].new_start + k]);
            }
            printf(".\n");
            printf("%dd\n", old_pos);
            for (int k = 0; k < your_changes[yi].new_count; k++) {
                printf("\t%s\n", file3_lines[your_changes[yi].new_start + k]);
            }
            printf(".\n");
            mi++; yi++;
            old_pos = std::max(mine_changes[mi > 0 ? mi - 1 : 0].old_end,
                               your_changes[yi > 0 ? yi - 1 : 0].old_end);
        } else if (mine_here) {
            printf("%da\n", old_pos);
            for (int k = 0; k < mine_changes[mi].new_count; k++) {
                printf("\t%s\n", file1_lines[mine_changes[mi].new_start + k]);
            }
            printf(".\n");
            mi++;
            old_pos = mine_changes[mi > 0 ? mi - 1 : old_pos].old_end;
        } else if (your_here) {
            printf("%da\n", old_pos);
            for (int k = 0; k < your_changes[yi].new_count; k++) {
                printf("\t%s\n", file3_lines[your_changes[yi].new_start + k]);
            }
            printf(".\n");
            yi++;
            old_pos = your_changes[yi > 0 ? yi - 1 : old_pos].old_end;
        } else {
            old_pos++;
        }
    }
    printf("w\nq\n");
}

/* ── Output: merge format with conflict markers ─────────────────────────── */

static void output_merge(int file1_n, char** file1_lines,
                          int file2_n, char** file2_lines,
                          int file3_n, char** file3_lines,
                          const std::string label[3],
                          const Diff3Options& opts) {
    auto mine_script = compute_edit_script(file2_lines, file2_n, file1_lines, file1_n);
    auto your_script = compute_edit_script(file2_lines, file2_n, file3_lines, file3_n);

    auto mine_changes = edit_script_to_changes(mine_script, file2_n, file1_n);
    auto your_changes = edit_script_to_changes(your_script, file2_n, file3_n);

    /* Rebuild the merged output by walking through old file + changes */
    int mi = 0, yi = 0;
    int old_pos = 0;

    while (old_pos < file2_n || mi < (int)mine_changes.size() || yi < (int)your_changes.size()) {
        bool mine_here = (mi < (int)mine_changes.size() &&
                          mine_changes[mi].old_start <= old_pos &&
                          mine_changes[mi].old_end > old_pos);
        bool your_here = (yi < (int)your_changes.size() &&
                          your_changes[yi].old_start <= old_pos &&
                          your_changes[yi].old_end > old_pos);

        if (!mine_here && !your_here) {
            /* Unchanged line */
            if (opts.initial_tab) printf("\t");
            printf("%s\n", file2_lines[old_pos]);
            old_pos++;
        } else if (mine_here && your_here && ranges_overlap(mine_changes[mi], your_changes[yi])) {
            /* Conflict */
            printf("<<<<<<< %s\n", !opts.label[0].empty() ? opts.label[0].c_str() : "mine");
            for (int k = 0; k < mine_changes[mi].new_count; k++) {
                printf("%s\n", file1_lines[mine_changes[mi].new_start + k]);
            }
            printf("=======\n");
            for (int k = 0; k < your_changes[yi].new_count; k++) {
                printf("%s\n", file3_lines[your_changes[yi].new_start + k]);
            }
            printf(">>>>>>> %s\n", !opts.label[2].empty() ? opts.label[2].c_str() : "yours");
            mi++; yi++;
            old_pos = std::max(mine_changes[mi > 0 ? mi - 1 : 0].old_end,
                               your_changes[yi > 0 ? yi - 1 : 0].old_end);
        } else if (mine_here) {
            /* Only mine changed here */
            for (int k = 0; k < mine_changes[mi].new_count; k++) {
                printf("%s\n", file1_lines[mine_changes[mi].new_start + k]);
            }
            mi++;
            old_pos = (mi > 0) ? mine_changes[mi - 1].old_end : old_pos + 1;
        } else if (your_here) {
            /* Only yours changed here */
            for (int k = 0; k < your_changes[yi].new_count; k++) {
                printf("%s\n", file3_lines[your_changes[yi].new_start + k]);
            }
            yi++;
            old_pos = (yi > 0) ? your_changes[yi - 1].old_end : old_pos + 1;
        } else {
            old_pos++;
        }
    }
}

/* ── Main command ───────────────────────────────────────────────────────── */

int diff3_command(int argc, char** argv) {
    struct arg_lit* show_all_opt = arg_lit0("A", "show-all", "output all changes with conflict markers");
    struct arg_lit* ed_opt = arg_lit0("e", "ed", "output ed script");
    struct arg_lit* show_overlap_opt = arg_lit0("E", "show-overlap", "like -e with conflict markers");
    struct arg_lit* easy_only_opt = arg_lit0("3", "easy-only", "non-overlapping changes only");
    struct arg_lit* overlap_only_opt = arg_lit0("x", "overlap-only", "overlapping changes only");
    struct arg_lit* overlap_x_opt = arg_lit0("X", nullptr, "like -x with conflict markers");
    struct arg_lit* merge_opt = arg_lit0("m", "merge", "output merged file");
    struct arg_lit* text_opt = arg_lit0("a", "text", "treat all files as text");
    struct arg_lit* strip_cr_opt = arg_lit0(nullptr, "strip-trailing-cr", "strip trailing CR");
    struct arg_lit* initial_tab_opt = arg_lit0("T", "initial-tab", "add tab before output");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_str* label_opt = arg_strn("L", "label", "LABEL", 0, 3, "use LABEL instead of filename");
    struct arg_file* file1_arg = arg_filen(NULL, NULL, "MYFILE", 1, 1, "my file");
    struct arg_file* file2_arg = arg_filen(NULL, NULL, "BASEFILE", 1, 1, "base file");
    struct arg_file* file3_arg = arg_filen(NULL, NULL, "YOURFILE", 1, 1, "your file");
    struct arg_end* end = arg_end(20);

    ArgTable at({show_all_opt, ed_opt, show_overlap_opt, easy_only_opt, overlap_only_opt,
                 overlap_x_opt, merge_opt, text_opt, strip_cr_opt, initial_tab_opt,
                 help_opt, label_opt, file1_arg, file2_arg, file3_arg, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... MYFILE BASEFILE YOURFILE\n", argv[0]);
        printf("Compare three files line by line.\n");
        printf("\n");
        printf("Output formats:\n");
        printf("  -A, --show-all          output all changes with conflict markers\n");
        printf("  -e, --ed                output ed script\n");
        printf("  -E, --show-overlap      like -e with conflict markers\n");
        printf("  -3, --easy-only         non-overlapping changes only\n");
        printf("  -x, --overlap-only      overlapping changes only\n");
        printf("  -X                      overlapping changes with conflict markers\n");
        printf("  -m, --merge             output merged file with conflict markers\n");
        printf("\n");
        printf("  -a, --text              treat all files as text\n");
        printf("  -T, --initial-tab       add tab before output\n");
        printf("  -L, --label=LABEL       use LABEL instead of filename\n");
        printf("  -h, --help              display this help and exit\n");

        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    Diff3Options opts;
    opts.strip_cr = (strip_cr_opt->count > 0);
    opts.initial_tab = (initial_tab_opt->count > 0);

    for (int i = 0; i < label_opt->count; i++) {
        opts.label[i] = label_opt->sval[i];
    }

    /* Determine output mode */
    if (show_all_opt->count > 0 || merge_opt->count > 0) {
        opts.output = Diff3Output::MERGE;
    } else if (ed_opt->count > 0) {
        opts.output = Diff3Output::ED_SCRIPT;
    } else if (show_overlap_opt->count > 0) {
        opts.output = Diff3Output::SHOW_OVERLAP;
    } else if (easy_only_opt->count > 0) {
        opts.output = Diff3Output::EASY_ONLY;
    } else if (overlap_only_opt->count > 0) {
        opts.output = Diff3Output::OVERLAP_ONLY;
    } else if (overlap_x_opt->count > 0) {
        opts.output = Diff3Output::OVERLAP_X;
    }

    const char* file1 = file1_arg->filename[0];
    const char* file2 = file2_arg->filename[0];
    const char* file3 = file3_arg->filename[0];

    int f1_n, f2_n, f3_n;
    char** f1_lines = read_lines(file1, &f1_n);
    if (f1_lines == NULL && f1_n == -1) {
        cmd_perror("diff3", file1);

        return 0;
    }

    char** f2_lines = read_lines(file2, &f2_n);
    if (f2_lines == NULL && f2_n == -1) {
        cmd_perror("diff3", file2);
        free_lines(f1_lines, f1_n);

        return 0;
    }

    char** f3_lines = read_lines(file3, &f3_n);
    if (f3_lines == NULL && f3_n == -1) {
        cmd_perror("diff3", file3);
        free_lines(f1_lines, f1_n);
        free_lines(f2_lines, f2_n);

        return 0;
    }

    /* Set defaults for labels */
    if (opts.label[0].empty()) opts.label[0] = file1;
    if (opts.label[1].empty()) opts.label[1] = file2;
    if (opts.label[2].empty()) opts.label[2] = file3;

    /* Check if all three files are identical */
    bool all_same = (f1_n == f2_n && f2_n == f3_n);
    if (all_same) {
        for (int i = 0; i < f1_n; i++) {
            if (!lines_equal(f1_lines[i], f2_lines[i]) || !lines_equal(f2_lines[i], f3_lines[i])) {
                all_same = false;
                break;
            }
        }
    }

    if (all_same) {
        /* All files identical — nothing to do */
        free_lines(f1_lines, f1_n);
        free_lines(f2_lines, f2_n);
        free_lines(f3_lines, f3_n);

        return 0;
    }

    /* Check if mine == base == yours pairwise */
    bool mine_eq_base = (f1_n == f2_n);
    bool yours_eq_base = (f3_n == f2_n);
    if (mine_eq_base) {
        for (int i = 0; i < f1_n; i++) {
            if (!lines_equal(f1_lines[i], f2_lines[i])) { mine_eq_base = false; break; }
        }
    }
    if (yours_eq_base) {
        for (int i = 0; i < f3_n; i++) {
            if (!lines_equal(f3_lines[i], f2_lines[i])) { yours_eq_base = false; break; }
        }
    }

    if (mine_eq_base && yours_eq_base) {
        /* All three identical */
        free_lines(f1_lines, f1_n);
        free_lines(f2_lines, f2_n);
        free_lines(f3_lines, f3_n);

        return 0;
    }

    if (mine_eq_base) {
        /* Only yours changed — show your changes */
        output_default(f1_n, f1_lines, f2_n, f2_lines, f3_n, f3_lines, opts.label, opts);
    } else if (yours_eq_base) {
        /* Only mine changed — show my changes */
        output_default(f1_n, f1_lines, f2_n, f2_lines, f3_n, f3_lines, opts.label, opts);
    } else {
        /* Both changed — check for conflicts */
        switch (opts.output) {
        case Diff3Output::MERGE:
        case Diff3Output::SHOW_OVERLAP:
        case Diff3Output::OVERLAP_X:
            output_merge(f1_n, f1_lines, f2_n, f2_lines, f3_n, f3_lines,
                         opts.label, opts);
            break;
        case Diff3Output::ED_SCRIPT:
        case Diff3Output::EASY_ONLY:
        case Diff3Output::OVERLAP_ONLY:
            output_ed(f1_n, f1_lines, f2_n, f2_lines, f3_n, f3_lines, opts);
            break;
        default:
            output_default(f1_n, f1_lines, f2_n, f2_lines, f3_n, f3_lines,
                           opts.label, opts);
            break;
        }
    }

    free_lines(f1_lines, f1_n);
    free_lines(f2_lines, f2_n);
    free_lines(f3_lines, f3_n);
    return 0;
}

REGISTER_COMMAND("diff3", diff3_command, "Compare three files line by line");
