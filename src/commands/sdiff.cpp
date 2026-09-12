/*
 * sdiff — side-by-side merge of file differences.
 *
 * Two modes:
 *   - default: print FILE1 and FILE2 side by side (byte-for-byte like
 *     `diff -y`, whose layout GNU sdiff borrows from diffutils' side.c);
 *   - `-o FILE`: walk the change hunks interactively and write the chosen
 *     versions to FILE.
 *
 * The layout arithmetic and the half-line renderer are ported from GNU
 * diffutils 3.12's src/side.c so widths, tab handling and gutter characters
 * match GNU exactly. The change script comes from an in-process LCS, so no
 * external diff program is required.
 */

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "commands/command_macros.hpp"
#include "commands/sdiff.hpp"

#define SDIFF_MAX_LINE 1048576 /* 1 MiB max line length */
#define GUTTER_WIDTH_MINIMUM 3
#define SDIFF_BUFSIZE 65536

/* ── Column geometry ────────────────────────────────────────────────────── */

/* `-W`/`-w` are swapped relative to plain `diff`: for sdiff `-W` means
 * --ignore-all-space and `-w` means --width. This matches GNU. */

struct Layout {
    int half_width = 61;
    int column2_offset = 64;
};

static Layout compute_layout(int width, int tabsize, int expand_tabs) {
    Layout l;
    int const t = expand_tabs != 0 ? 1 : tabsize;
    int const w = width;
    int const t_plus_g = t + GUTTER_WIDTH_MINIMUM;
    int const unaligned_off = (w >> 1) + (t_plus_g >> 1) + (w & t_plus_g & 1);
    int const off = unaligned_off - unaligned_off % t;
    int const half_width = std::max(0, std::min(off - GUTTER_WIDTH_MINIMUM, w - off));
    l.half_width = half_width;
    l.column2_offset = half_width != 0 ? off : w;
    return l;
}

/* ── Input files: lines plus whether each had a trailing newline ────────── */

struct Input {
    std::vector<char*> lines;   /* NUL-terminated, newline stripped */
    std::vector<bool> had_eol;  /* trailing newline present */
    int valid_lines = 0;
};

static void free_input(Input& in) {
    for (char* p : in.lines) {
        free(p);
    }
    in.lines.clear();
    in.had_eol.clear();
}

/* ── Line comparison (mirrors diff.cpp's lines_match) ───────────────────── */

static bool is_blank_line(const char* s) {
    for (; *s != '\0'; s++) {
        if (static_cast<unsigned char>(*s) > ' ') {
            return false;
        }
    }
    return true;
}

static bool has_trailing_space_char(unsigned char c) {
    return c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == ' ';
}

/* Does PAT (a literal or `[...]` character class) match the character at S?
 * Returns the number of pattern bytes consumed. */
static size_t match_pattern_char(const char* pat, size_t pat_len, char s) {
    if (pat_len >= 2 && pat[0] == '[') {
        bool const negate = pat[1] == '^';
        size_t k = negate ? 2u : 1u;
        bool hit = false;
        while (k < pat_len && pat[k] != ']') {
            if (k + 2 < pat_len && pat[k + 1] == '-' && pat[k + 2] != ']') {
                if (static_cast<unsigned char>(s) >=
                        static_cast<unsigned char>(pat[k]) &&
                    static_cast<unsigned char>(s) <=
                        static_cast<unsigned char>(pat[k + 2])) {
                    hit = true;
                }
                k += 3;
            } else {
                if (pat[k] == s) {
                    hit = true;
                }
                k++;
            }
        }
        if (k < pat_len && pat[k] == ']') {
            k++;
        }
        return (hit != negate) ? k : 0;
    }
    if (pat_len >= 1 && pat[0] == '.') {
        return 1;
    }
    if (pat_len >= 1 && pat[0] == s) {
        return 1;
    }
    return 0;
}

/* Try to match the literal/class pattern PAT (of PAT_LEN bytes) starting at S.
 * The whole pattern must be consumed; a trailing `.*` is not required. */
static bool match_pattern_at(const char* pat, size_t pat_len, const char* s) {
    size_t pi = 0;
    while (pi < pat_len) {
        if (*s == '\0') {
            return false;
        }
        size_t const used = match_pattern_char(pat + pi, pat_len - pi, *s);
        if (used == 0) {
            return false;
        }
        pi += used;
        s++;
    }
    return true;
}

/* Does line S contain a match for regular expression RE? Requires a match at
 * the very start of S. */
static bool match_pattern_prefix(const char* pat, size_t pat_len, const char* s) {
    return match_pattern_at(pat, pat_len, s);
}

/* Does line S contain a match for regular expression RE?
 * modbox carries no regex dependency, so the common `-I` shapes are
 * supported: `^...`, `...$`, `^...$`, a plain substring, and single-character
 * `.` / `[...]` classes within them. */
static bool regex_match_line(const char* re, const char* s) {
    size_t const re_len = strlen(re);
    bool const anchor_start = re_len > 0 && re[0] == '^';
    bool const anchor_end = re_len > (anchor_start ? 1u : 0u) && re[re_len - 1] == '$';
    size_t const core_start = anchor_start ? 1u : 0u;
    size_t const core_len = re_len - core_start - (anchor_end ? 1u : 0u);
    if (core_len == 0) {
        return true;
    }
    const char* core = re + core_start;
    if (anchor_start && anchor_end) {
        return strlen(s) == core_len && match_pattern_at(core, core_len, s);
    }
    if (anchor_start) {
        return match_pattern_prefix(core, core_len, s);
    }
    if (anchor_end) {
        size_t const s_len = strlen(s);
        for (size_t pos = 0; pos + core_len <= s_len; pos++) {
            if (match_pattern_at(core, core_len, s + pos) && pos + core_len == s_len) {
                return true;
            }
        }
        return false;
    }
    size_t const s_len = strlen(s);
    for (size_t pos = 0; pos + core_len <= s_len; pos++) {
        if (match_pattern_at(core, core_len, s + pos)) {
            return true;
        }
    }
    return false;
}

static bool lines_match(const char* a, const char* b, const SdiffOptions* opts) {
    if (opts->ignore_all_space != 0) {
        while (*a != 0 && *b != 0) {
            while (*a != 0 && static_cast<unsigned char>(*a) <= ' ' && *a != '\n') {
                a++;
            }
            while (*b != 0 && static_cast<unsigned char>(*b) <= ' ' && *b != '\n') {
                b++;
            }
            if (*a == 0 || *b == 0) {
                break;
            }
            if (opts->ignore_case != 0) {
                if (std::tolower(static_cast<unsigned char>(*a)) !=
                    std::tolower(static_cast<unsigned char>(*b))) {
                    return false;
                }
            } else if (*a != *b) {
                return false;
            }
            a++;
            b++;
        }
        while (*a != 0 && static_cast<unsigned char>(*a) <= ' ' && *a != '\n') {
            a++;
        }
        while (*b != 0 && static_cast<unsigned char>(*b) <= ' ' && *b != '\n') {
            b++;
        }
        return (*a == '\0' || *a == '\n') && (*b == '\0' || *b == '\n');
    }
    if (opts->ignore_space_change != 0) {
        while (*a != 0 && *b != 0) {
            int const ws_a = static_cast<int>(static_cast<unsigned char>(*a) <= ' ' && *a != '\n');
            int const ws_b = static_cast<int>(static_cast<unsigned char>(*b) <= ' ' && *b != '\n');
            if (ws_a != 0 || ws_b != 0) {
                if (ws_a == 0 || ws_b == 0) {
                    return false;
                }
                while (*a != 0 && static_cast<unsigned char>(*a) <= ' ' && *a != '\n') {
                    a++;
                }
                while (*b != 0 && static_cast<unsigned char>(*b) <= ' ' && *b != '\n') {
                    b++;
                }
                continue;
            }
            if (opts->ignore_case != 0) {
                if (std::tolower(static_cast<unsigned char>(*a)) !=
                    std::tolower(static_cast<unsigned char>(*b))) {
                    return false;
                }
            } else if (*a != *b) {
                return false;
            }
            a++;
            b++;
        }
        return (*a == '\0' || *a == '\n') && (*b == '\0' || *b == '\n');
    }
    if (opts->ignore_trailing_space != 0) {
        size_t alen = strlen(a);
        while (alen > 0 && has_trailing_space_char(static_cast<unsigned char>(a[alen - 1]))) {
            alen--;
        }
        size_t blen = strlen(b);
        while (blen > 0 && has_trailing_space_char(static_cast<unsigned char>(b[blen - 1]))) {
            blen--;
        }
        if (alen != blen) {
            return false;
        }
        for (size_t i = 0; i < alen; i++) {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if (opts->ignore_case != 0) {
                ca = static_cast<unsigned char>(std::tolower(ca));
                cb = static_cast<unsigned char>(std::tolower(cb));
            }
            if (ca != cb) {
                return false;
            }
        }
        return true;
    }
    if (opts->ignore_case != 0) {
        while (*a != 0 && *b != 0) {
            if (std::tolower(static_cast<unsigned char>(*a)) !=
                std::tolower(static_cast<unsigned char>(*b))) {
                return false;
            }
            a++;
            b++;
        }
        return *a == *b;
    }
    return strcmp(a, b) == 0;
}

/* ── Read a file into an Input ──────────────────────────────────────────── */

static bool read_input(const char* filename, const SdiffOptions* opts, Input* out) {
    FILE* fp = strcmp(filename, "-") == 0 ? stdin : fopen(filename, "r");
    if (fp == nullptr) {
        return false;
    }

    std::vector<char> buf(SDIFF_MAX_LINE);
    for (;;) {
        size_t n = 0;
        bool saw_any = false;
        bool saw_newline = false;
        int c = 0;
        for (;;) {
            c = getc(fp);
            if (c == EOF) {
                break;
            }
            saw_any = true;
            if (c == '\n') {
                saw_newline = true;
                break;
            }
            if (n + 1 < buf.size()) {
                buf[n++] = static_cast<char>(c);
            }
        }
        if (!saw_any && c == EOF) {
            break;
        }

        char* line = static_cast<char*>(malloc(n + 1));
        if (line == nullptr) {
            if (fp != stdin) {
                (void)fclose(fp);
            }
            return false;
        }
        if (n > 0) {
            memcpy(line, buf.data(), n);
        }
        line[n] = '\0';

        if (opts->strip_trailing_cr != 0 && n > 0 && line[n - 1] == '\r') {
            line[n - 1] = '\0';
        }

        out->lines.push_back(line);
        out->had_eol.push_back(saw_newline);

        if (c == EOF) {
            break;
        }
    }

    if (fp != stdin) {
        (void)fclose(fp);
    }
    out->valid_lines = static_cast<int>(out->lines.size());
    return true;
}

/* ── Change script (LCS over lines) ─────────────────────────────────────── */

enum class Op { DELETE, INSERT, REPLACE };

struct Change {
    Op op;
    int old_start;
    int old_count;
    int new_start;
    int new_count;
};

/* Suppressed under -B when every involved line is blank, or under -I when
 * every involved line matches the regex. */
static bool change_is_ignorable(const Change& ch, const Input& lf, const Input& rf,
                                const SdiffOptions* opts) {
    if (opts->ignore_matching_lines != nullptr) {
        const char* re = opts->ignore_matching_lines;
        bool all_match = true;
        for (int i = 0; i < ch.old_count && all_match; i++) {
            if (!regex_match_line(re, lf.lines[static_cast<size_t>(ch.old_start + i)])) {
                all_match = false;
            }
        }
        for (int i = 0; i < ch.new_count && all_match; i++) {
            if (!regex_match_line(re, rf.lines[static_cast<size_t>(ch.new_start + i)])) {
                all_match = false;
            }
        }
        if (all_match) {
            return true;
        }
    }
    if (opts->ignore_blank_lines != 0) {
        bool all_blank = true;
        for (int i = 0; i < ch.old_count && all_blank; i++) {
            if (!is_blank_line(lf.lines[static_cast<size_t>(ch.old_start + i)])) {
                all_blank = false;
            }
        }
        for (int i = 0; i < ch.new_count && all_blank; i++) {
            if (!is_blank_line(rf.lines[static_cast<size_t>(ch.new_start + i)])) {
                all_blank = false;
            }
        }
        if (all_blank) {
            return true;
        }
    }
    return false;
}

static std::vector<Change> compute_changes(const Input& lf, const Input& rf,
                                           const SdiffOptions* opts) {
    std::vector<Change> result;
    int const old_len = lf.valid_lines;
    int const new_len = rf.valid_lines;
    if (old_len == 0 && new_len == 0) {
        return result;
    }

    /* Two lines are equivalent only when their text matches and they agree on
     * whether a trailing newline is present; otherwise the side-by-side view
     * would hide a missing-newline difference as a common line instead of
     * flagging it with a '/' or '\' separator. */
    auto same = [&](int o, int n) {
        return lf.had_eol[static_cast<size_t>(o)] == rf.had_eol[static_cast<size_t>(n)] &&
               lines_match(lf.lines[static_cast<size_t>(o)],
                           rf.lines[static_cast<size_t>(n)], opts);
    };

    /* Line equivalence within one file, used by the boundary-shifting pass:
     * two lines of the SAME file are interchangeable when their text and their
     * trailing-newline state agree. (same() above spans both files.) */
    auto same_left = [&](int a, int b) {
        return lf.had_eol[static_cast<size_t>(a)] == lf.had_eol[static_cast<size_t>(b)] &&
               lines_match(lf.lines[static_cast<size_t>(a)],
                           lf.lines[static_cast<size_t>(b)], opts);
    };
    auto same_right = [&](int a, int b) {
        return rf.had_eol[static_cast<size_t>(a)] == rf.had_eol[static_cast<size_t>(b)] &&
               lines_match(rf.lines[static_cast<size_t>(a)],
                           rf.lines[static_cast<size_t>(b)], opts);
    };

    /* same_line(f, a, b): equivalence of two lines within file f (0 = left). */
    auto same_line = [&](int f, int a, int b) {
        return f == 0 ? same_left(a, b) : same_right(a, b);
    };

    if (old_len == 0) {
        return std::vector<Change>{Change{Op::INSERT, 0, 0, 0, new_len}};
    }
    if (new_len == 0) {
        return std::vector<Change>{Change{Op::DELETE, 0, old_len, 0, 0}};
    }

    std::vector<std::vector<int>> lcs(static_cast<size_t>(old_len) + 1,
                                      std::vector<int>(static_cast<size_t>(new_len) + 1, 0));
    for (int i = 1; i <= old_len; i++) {
        for (int j = 1; j <= new_len; j++) {
            if (same(i - 1, j - 1)) {
                lcs[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                    lcs[static_cast<size_t>(i - 1)][static_cast<size_t>(j - 1)] + 1;
            } else {
                lcs[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                    std::max(lcs[static_cast<size_t>(i - 1)][static_cast<size_t>(j)],
                             lcs[static_cast<size_t>(i)][static_cast<size_t>(j - 1)]);
            }
        }
    }

    /* Walk the LCS table backwards to obtain the canonical alignment. Each
     * step is recorded as one of MATCH / DELETE / INSERT on the input lines. */
    auto cell = [&](int x, int y) {
        return (x <= 0 || y <= 0) ? 0
                                  : lcs[static_cast<size_t>(x)][static_cast<size_t>(y)];
    };
    enum class Step { MATCH, DELETE, INSERT };
    std::vector<Step> rev;
    int i = old_len;
    int j = new_len;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && same(i - 1, j - 1)) {
            i--;
            j--;
            rev.push_back(Step::MATCH);
        } else if (i > 0 && (j == 0 || cell(i - 1, j) > cell(i, j - 1))) {
            i--;
            rev.push_back(Step::DELETE);
        } else {
            j--;
            rev.push_back(Step::INSERT);
        }
    }

    /* Build per-file "changed" bitmaps from the LCS walk, then normalise them
     * with GNU diff's shift_boundaries (src/analyze.c). That pass slides each
     * run of changes backwards then forwards while the lines it steps over
     * remain equivalent within the same file, which is what makes GNU anchor
     * repeated-line runs later than a plain LCS tie-break does. */
    std::vector<char> old_changed(static_cast<size_t>(old_len), 0);
    std::vector<char> new_changed(static_cast<size_t>(new_len), 0);
    {
        int oi = 0;
        int nj = 0;
        for (auto it = rev.rbegin(); it != rev.rend(); ++it) {
            if (*it == Step::MATCH) {
                oi++;
                nj++;
            } else if (*it == Step::DELETE) {
                old_changed[static_cast<size_t>(oi++)] = 1;
            } else {
                new_changed[static_cast<size_t>(nj++)] = 1;
            }
        }
    }

    /* shift_boundaries: for each file independently, grow/shrink runs of
     * changes so that equivalent lines line up the way GNU presents them. */
    for (int f = 0; f < 2; f++) {
        std::vector<char>& changed = (f == 0) ? old_changed : new_changed;
        const std::vector<char>& other_changed = (f == 0) ? new_changed : old_changed;
        int const i_end = static_cast<int>(changed.size());
        int const other_end = static_cast<int>(other_changed.size());
        int i = 0;
        int j = 0;
        for (;;) {
            while (i < i_end && changed[static_cast<size_t>(i)] == 0) {
                while (j < other_end && other_changed[static_cast<size_t>(j)] != 0) {
                    j++;
                }
                i++;
            }
            if (i == i_end) {
                break;
            }
            int start = i;
            while (i < i_end && changed[static_cast<size_t>(i)] != 0) {
                i++;
            }
            while (j < other_end && other_changed[static_cast<size_t>(j)] != 0) {
                j++;
            }
            int runlength;
            do {
                runlength = i - start;

                /* Move the run backwards while the line before it matches the
                 * last changed line; this merges the run with an earlier one. */
                while (start != 0 && same_line(f, start - 1, i - 1)) {
                    changed[static_cast<size_t>(--start)] = 1;
                    changed[static_cast<size_t>(--i)] = 0;
                    while (start != 0 && changed[static_cast<size_t>(start - 1)] != 0) {
                        start--;
                    }
                    /* GNU: while (other_changed[--j]) continue; — the
                     * decrement happens before the test. */
                    if (j > 0) {
                        j--;
                    }
                    while (j > 0 && other_changed[static_cast<size_t>(j)] != 0) {
                        j--;
                    }
                }

                /* Move the run forwards while the first changed line matches
                 * the line that follows the run. */
                while (i != i_end && same_line(f, start, i)) {
                    changed[static_cast<size_t>(start++)] = 0;
                    changed[static_cast<size_t>(i++)] = 1;
                    while (i < i_end && changed[static_cast<size_t>(i)] != 0) {
                        i++;
                    }
                    /* GNU: while (other_changed[++j]) continue; — the
                     * increment happens before the test. */
                    if (j < other_end) {
                        j++;
                    }
                    while (j < other_end && other_changed[static_cast<size_t>(j)] != 0) {
                        j++;
                    }
                }
            } while (runlength != i - start);
        }
    }

    /* Rebuild the ordered change list from the normalised bitmaps. A position
     * is changed on a side when its flag is set; consecutive changes on both
     * sides collapse into a REPLACE so the renderer pairs the columns. */
    {
        int oi = 0;
        int nj = 0;
        while (oi < old_len || nj < new_len) {
            bool const dc = oi < old_len && old_changed[static_cast<size_t>(oi)] != 0;
            bool const ic = nj < new_len && new_changed[static_cast<size_t>(nj)] != 0;
            if (!dc && !ic) {
                oi++;
                nj++;
                continue;
            }
            int const old_start = oi;
            int const new_start = nj;
            int dels = 0;
            int ins = 0;
            while (oi < old_len && old_changed[static_cast<size_t>(oi)] != 0) {
                oi++;
                dels++;
            }
            while (nj < new_len && new_changed[static_cast<size_t>(nj)] != 0) {
                nj++;
                ins++;
            }
            if (dels > 0 && ins > 0) {
                result.push_back({Op::REPLACE, old_start, dels, new_start, ins});
            } else if (dels > 0) {
                result.push_back({Op::DELETE, old_start, dels, new_start, 0});
            } else {
                result.push_back({Op::INSERT, old_start, 0, new_start, ins});
            }
        }
    }

    std::vector<Change> kept;
    for (const Change& ch : result) {
        if (!change_is_ignorable(ch, lf, rf, opts)) {
            kept.push_back(ch);
        }
    }
    return kept;
}

/* ── Side-by-side renderer (ported from GNU src/side.c) ─────────────────── */

struct Renderer {
    FILE* out = nullptr;        /* stdout for the view, or the merge stream */
    FILE* view = nullptr;       /* where the -y rendering is displayed */
    FILE* sink = nullptr;       /* merge output file (may equal view) */
    Layout layout;
    SdiffOptions opts;
    int suppress_common = 0;
    int left_column = 0;
    int expand_tabs = 0;
    int tabsize = 8;
};

/* Advance to column TO, emitting tabs (unless -t) then spaces. */
static int tab_from_to(int from, int to, const Renderer* r) {
    if (r->expand_tabs == 0) {
        int const tab_size = r->tabsize;
        for (int tab = from + tab_size - from % tab_size; tab <= to; tab += tab_size) {
            putc('\t', r->out);
            from = tab;
        }
    }
    while (from++ < to) {
        putc(' ', r->out);
    }
    return to;
}

/* Print half a sdiff line: truncate to OUT_BOUND print columns, observing
 * tabs, and trim a trailing newline. Returns the column reached. */
static int print_half_line(const char* text, int indent, int out_bound,
                           const Renderer* r) {
    int in_position = 0;
    int out_position = 0;

    while (*text != '\0') {
        unsigned char const c = static_cast<unsigned char>(*text++);
        if (c == '\t') {
            int const spaces = r->tabsize - in_position % r->tabsize;
            int const tabstop = in_position + spaces;
            if (in_position == out_position) {
                if (r->expand_tabs != 0) {
                    int stop = tabstop;
                    if (out_bound < stop) {
                        stop = out_bound;
                    }
                    for (; out_position < stop; out_position++) {
                        putc(' ', r->out);
                    }
                } else if (tabstop < out_bound) {
                    out_position = tabstop;
                    putc('\t', r->out);
                }
            }
            in_position = tabstop;
        } else if (c == '\r') {
            putc('\r', r->out);
            (void)tab_from_to(0, indent, r);
            in_position = 0;
            out_position = 0;
        } else if (c == '\b') {
            if (in_position != 0 && --in_position < out_bound) {
                if (out_position <= in_position) {
                    for (; out_position < in_position; out_position++) {
                        putc(' ', r->out);
                    }
                } else {
                    out_position = in_position;
                    putc('\b', r->out);
                }
            }
        } else if (c == '\0' || c == '\a' || c == '\f' || c == '\v') {
            if (in_position <= out_bound) {
                putc(static_cast<int>(c), r->out);
            }
        } else {
            in_position++;
            if (in_position <= out_bound) {
                out_position = in_position;
                putc(static_cast<int>(c), r->out);
            }
        }
    }
    return out_position;
}

/* Print one side-by-side line with a separator. A null side is absent. */
static void print_1sdiff_line(const char* left, char sep, bool left_eol,
                              const char* right, bool right_eol,
                              const Renderer* r) {
    int const hw = r->layout.half_width;
    int const c2o = r->layout.column2_offset;
    int col = 0;
    bool put_newline = false;

    if (left != nullptr) {
        put_newline = put_newline || left_eol;
        col = print_half_line(left, 0, hw, r);
    }

    if (sep != ' ') {
        col = tab_from_to(col, (hw + c2o - 1) >> 1, r) + 1;
        if (sep == '|' && put_newline != right_eol) {
            sep = put_newline ? '/' : '\\';
        }
        putc(sep, r->out);
    }

    if (right != nullptr) {
        put_newline = put_newline || right_eol;
        /* modbox stores lines without their trailing newline, so an empty
         * string stands for a line that is just "\n". Such a line must not
         * provoke the second-column padding or an otherwise blank common line
         * would render as a run of tabs. */
        if (*right != '\n' && *right != '\0') {
            col = tab_from_to(col, c2o, r);
            (void)print_half_line(right, col, hw, r);
        }
    }

    if (put_newline) {
        putc('\n', r->out);
    }
}

/* Emit the rendered common lines in [i0,limit0) x [i1,limit1). */
static void print_common_lines(int i0, int limit0, int i1, int limit1,
                               const Input& lf, const Input& rf, Renderer* r) {
    if (r->suppress_common != 0 || (i0 == limit0 && i1 == limit1)) {
        return;
    }
    if (r->left_column == 0) {
        while (i0 != limit0 && i1 != limit1) {
            print_1sdiff_line(lf.lines[static_cast<size_t>(i0)], ' ',
                              lf.had_eol[static_cast<size_t>(i0)],
                              rf.lines[static_cast<size_t>(i1)],
                              rf.had_eol[static_cast<size_t>(i1)], r);
            i0++;
            i1++;
        }
        while (i1 != limit1) {
            print_1sdiff_line(nullptr, ')', false,
                              rf.lines[static_cast<size_t>(i1)],
                              rf.had_eol[static_cast<size_t>(i1)], r);
            i1++;
        }
    }
    while (i0 != limit0) {
        print_1sdiff_line(lf.lines[static_cast<size_t>(i0)], '(',
                          lf.had_eol[static_cast<size_t>(i0)], nullptr, false, r);
        i0++;
    }
}

/* Render the whole change script to r->out. */
static void render_script(const Input& lf, const Input& rf,
                          const std::vector<Change>& changes, Renderer* r) {
    int next0 = 0;
    int next1 = 0;

    for (const Change& ch : changes) {
        print_common_lines(next0, ch.old_start, next1, ch.new_start, lf, rf, r);
        next0 = ch.old_start;
        next1 = ch.new_start;

        if (ch.op == Op::REPLACE) {
            int i = ch.old_start;
            int j = ch.new_start;
            for (; i < ch.old_start + ch.old_count && j < ch.new_start + ch.new_count;
                 i++, j++) {
                print_1sdiff_line(lf.lines[static_cast<size_t>(i)], '|',
                                  lf.had_eol[static_cast<size_t>(i)],
                                  rf.lines[static_cast<size_t>(j)],
                                  rf.had_eol[static_cast<size_t>(j)], r);
            }
            for (; j < ch.new_start + ch.new_count; j++) {
                print_1sdiff_line(nullptr, '>', false,
                                  rf.lines[static_cast<size_t>(j)],
                                  rf.had_eol[static_cast<size_t>(j)], r);
            }
            for (; i < ch.old_start + ch.old_count; i++) {
                print_1sdiff_line(lf.lines[static_cast<size_t>(i)], '<',
                                  lf.had_eol[static_cast<size_t>(i)], nullptr, false, r);
            }
        } else if (ch.op == Op::INSERT) {
            for (int j = ch.new_start; j < ch.new_start + ch.new_count; j++) {
                print_1sdiff_line(nullptr, '>', false,
                                  rf.lines[static_cast<size_t>(j)],
                                  rf.had_eol[static_cast<size_t>(j)], r);
            }
        } else {
            for (int i = ch.old_start; i < ch.old_start + ch.old_count; i++) {
                print_1sdiff_line(lf.lines[static_cast<size_t>(i)], '<',
                                  lf.had_eol[static_cast<size_t>(i)], nullptr, false, r);
            }
        }

        next0 = ch.old_start + ch.old_count;
        next1 = ch.new_start + ch.new_count;
    }

    print_common_lines(next0, lf.valid_lines, next1, rf.valid_lines, lf, rf, r);
}

/* ── Usage / version ────────────────────────────────────────────────────── */

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTION]... FILE1 FILE2\n", prog);
    printf("Side-by-side merge of differences between FILE1 and FILE2.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -o, --output=FILE            operate interactively, sending output to FILE\n");
    printf("\n");
    printf("  -i, --ignore-case            consider upper- and lower-case to be the same\n");
    printf("  -E, --ignore-tab-expansion   ignore changes due to tab expansion\n");
    printf("  -Z, --ignore-trailing-space  ignore white space at line end\n");
    printf("  -b, --ignore-space-change    ignore changes in the amount of white space\n");
    printf("  -W, --ignore-all-space       ignore all white space\n");
    printf("  -B, --ignore-blank-lines     ignore changes whose lines are all blank\n");
    printf("  -I, --ignore-matching-lines=RE  ignore changes all whose lines match RE\n");
    printf("      --strip-trailing-cr      strip trailing carriage return on input\n");
    printf("  -a, --text                   treat all files as text\n");
    printf("\n");
    printf("  -w, --width=NUM              output at most NUM (default 130) print columns\n");
    printf("  -l, --left-column            output only the left column of common lines\n");
    printf("  -s, --suppress-common-lines  do not output common lines\n");
    printf("\n");
    printf("  -t, --expand-tabs            expand tabs to spaces in output\n");
    printf("      --tabsize=NUM            tab stops at every NUM (default 8) print columns\n");
    printf("\n");
    printf("  -d, --minimal                try hard to find a smaller set of changes\n");
    printf("  -H, --speed-large-files      assume large files, many scattered small changes\n");
    printf("      --diff-program=PROGRAM   use PROGRAM to compare files\n");
    printf("\n");
    printf("      --help                   display this help and exit\n");
    printf("  -v, --version                output version information and exit\n");
    printf("\n");
    printf("If a FILE is '-', read standard input.\n");
    printf("Exit status is 0 if inputs are the same, 1 if different, 2 if trouble.\n");
}

static void print_version(void) {
    printf("sdiff (modbox) 1.0\n");
    printf("Side-by-side merge of file differences.\n");
}

/* ── Option parsing ─────────────────────────────────────────────────────── */

static void try_help_exit(const char* msg, const char* arg) {
    if (msg != nullptr) {
        (void)fprintf(stderr, "sdiff: ");
        (void)fprintf(stderr, msg, arg);
        (void)fprintf(stderr, "\n");
    }
    (void)fprintf(stderr, "Try 'sdiff --help' for more information.\n");
    exit(2);
}

/* Parse a positive integer option argument; `what` names it for errors. */
static bool parse_positive_int(const char* s, int* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    long const v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v <= 0 || v > 0x7fffffffL) {
        return false;
    }
    *out = static_cast<int>(v);
    return true;
}

/* Take the value of an option that needs one, either glued (`-w20`,
 * `--width=20`) or as the next argv entry. Returns nullptr on failure. */
static const char* take_value(const std::string& arg, size_t prefix_len,
                              int argc, char** argv, int* i) {
    if (arg.size() > prefix_len) {
        return arg.c_str() + prefix_len;
    }
    if (*i + 1 < argc) {
        (*i)++;
        return argv[*i];
    }
    return nullptr;
}

/* ── Interactive merge (-o) ─────────────────────────────────────────────── */

static int sdiff_merge(const char* file1, const char* file2, const char* output,
                       const SdiffOptions* opts);

static void give_help() {
    (void)fputs(
        "ed:\tEdit then use both versions, each decorated with a header.\n"
        "eb:\tEdit then use both versions.\n"
        "el or e1:\tEdit then use the left version.\n"
        "er or e2:\tEdit then use the right version.\n"
        "e:\tDiscard both versions then edit a new one.\n"
        "l or 1:\tUse the left version.\n"
        "r or 2:\tUse the right version.\n"
        "s:\tSilently include common lines.\n"
        "v:\tVerbosely include common lines.\n"
        "q:\tQuit.\n",
        stderr);
}

/* Skip blanks on stdin; return the first non-space character, or '\n'/EOF. */
static int skip_white(void) {
    int c = 0;
    while ((c = getchar()) != '\n' && c != EOF &&
           std::isspace(static_cast<unsigned char>(c)) != 0) {
    }
    return c;
}

static void flush_line(void) {
    int c = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

/* A cursor over one file's lines, used to copy/skip merge hunks in order. */
struct LineCursor {
    const Input* in = nullptr;
    int pos = 0;
};

static void cursor_copy(LineCursor* cur, int lines, FILE* out) {
    for (int k = 0; k < lines && cur->pos < cur->in->valid_lines; k++, cur->pos++) {
        size_t const idx = static_cast<size_t>(cur->pos);
        fputs(cur->in->lines[idx], out);
        if (cur->in->had_eol[idx]) {
            fputc('\n', out);
        }
    }
}

static void cursor_skip(LineCursor* cur, int lines) {
    cur->pos += lines;
    if (cur->pos > cur->in->valid_lines) {
        cur->pos = cur->in->valid_lines;
    }
}

/* Run the editor over a temp file seeded according to CMD, then append the
 * edited result to OUT. Returns false if the output could not be written. */
static bool edit_and_append(int cmd, const std::string& lname, int lline, int llen,
                            const std::string& rname, int rline, int rlen,
                            LineCursor* lcur, LineCursor* rcur, FILE* out) {
    bool const use_left = cmd == 'l' || cmd == 'b' || cmd == 'd';
    bool const use_right = cmd == 'r' || cmd == 'b' || cmd == 'd';
    bool const with_headers = cmd == 'd';
    int const keep_left = cmd == 'l';
    int const keep_right = cmd == 'r';

    (void)keep_left;
    (void)keep_right;

    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir == nullptr || *tmpdir == '\0') {
        tmpdir = "/tmp";
    }
    std::string tmpl = std::string(tmpdir) + "/sdiffXXXXXX";
    std::vector<char> tmpbuf(tmpl.begin(), tmpl.end());
    tmpbuf.push_back('\0');
    int const fd = mkstemp(tmpbuf.data());
    if (fd < 0) {
        (void)fprintf(stderr, "sdiff: mkstemp: %s\n", strerror(errno));
        return false;
    }
    std::string tmpname(tmpbuf.data());

    FILE* tmp = fdopen(fd, "w");
    if (tmp == nullptr) {
        (void)fprintf(stderr, "sdiff: %s: %s\n", tmpname.c_str(), strerror(errno));
        (void)close(fd);
        (void)unlink(tmpname.c_str());
        return false;
    }

    if (use_left) {
        if (with_headers && llen > 0) {
            if (llen == 1) {
                (void)fprintf(tmp, "--- %s %d\n", lname.c_str(), lline);
            } else {
                (void)fprintf(tmp, "--- %s %d,%d\n", lname.c_str(), lline, lline + llen - 1);
            }
        }
        cursor_copy(lcur, llen, tmp);
    } else {
        cursor_skip(lcur, llen);
    }

    if (use_right) {
        if (with_headers && rlen > 0) {
            if (rlen == 1) {
                (void)fprintf(tmp, "+++ %s %d\n", rname.c_str(), rline);
            } else {
                (void)fprintf(tmp, "+++ %s %d,%d\n", rname.c_str(), rline, rline + rlen - 1);
            }
        }
        cursor_copy(rcur, rlen, tmp);
    } else {
        cursor_skip(rcur, rlen);
    }

    (void)fclose(tmp);

    const char* editor = getenv("EDITOR");
    if (editor == nullptr || *editor == '\0') {
        editor = "vi";
    }

    pid_t const pid = fork();
    if (pid == 0) {
        const char* cargv[3];
        cargv[0] = editor;
        cargv[1] = tmpname.c_str();
        cargv[2] = nullptr;
        execvp(editor, const_cast<char* const*>(cargv));
        _exit(127);
    }
    if (pid < 0) {
        (void)fprintf(stderr, "sdiff: fork: %s\n", strerror(errno));
        (void)unlink(tmpname.c_str());
        return false;
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            break;
        }
    }

    FILE* edited = fopen(tmpname.c_str(), "r");
    if (edited == nullptr) {
        (void)unlink(tmpname.c_str());
        return false;
    }
    std::vector<char> buf(SDIFF_BUFSIZE);
    size_t n = 0;
    while ((n = fread(buf.data(), 1, buf.size(), edited)) > 0) {
        (void)fwrite(buf.data(), 1, n, out);
    }
    (void)fclose(edited);
    (void)unlink(tmpname.c_str());
    return true;
}

/* Prompt at a change hunk and act on the answer. Returns false to stop the
 * merge (the user quit or stdin reached EOF). */
static bool interact_hunk(const std::string& lname, int lline, int llen,
                          const std::string& rname, int rline, int rlen,
                          LineCursor* lcur, LineCursor* rcur, FILE* out,
                          Renderer* rr) {
    for (;;) {
        if (putchar('%') != '%') {
            return false;
        }
        (void)fflush(stdout);

        int cmd = skip_white();
        char action = 0;
        switch (cmd) {
        case 'e': {
            int const cmd1 = skip_white();
            switch (cmd1) {
            case '\n': action = 'e'; break;
            case '1': case '2': case 'b': case 'd': case 'l': case 'r':
                action = static_cast<char>(cmd1);
                break;
            default:
                give_help();
                flush_line();
                continue;
            }
            break;
        }
        case '1': case '2': case 'l': case 'r': case 's': case 'v': case 'q':
            if (skip_white() == '\n') {
                action = static_cast<char>(cmd == '1' ? 'l' : cmd == '2' ? 'r' : cmd);
            } else {
                give_help();
                flush_line();
                continue;
            }
            break;
        case EOF:
            action = 'q';
            break;
        default:
            flush_line();
            give_help();
            continue;
        }

        switch (action) {
        case 'l':
            cursor_copy(lcur, llen, out);
            cursor_skip(rcur, rlen);
            return true;
        case 'r':
            cursor_copy(rcur, rlen, out);
            cursor_skip(lcur, llen);
            return true;
        case 's':
            rr->suppress_common = 1;
            break;
        case 'v':
            rr->suppress_common = 0;
            break;
        case 'q':
            return false;
        case 'e':
        case 'b':
        case 'd': {
            if (!edit_and_append(action, lname, lline, llen, rname, rline, rlen,
                                 lcur, rcur, out)) {
                return false;
            }
            return true;
        }
        default:
            give_help();
            break;
        }
    }
}

static int sdiff_merge(const char* file1, const char* file2, const char* output,
                       const SdiffOptions* opts) {
    if (strcmp(file1, "-") == 0 || strcmp(file2, "-") == 0) {
        (void)fprintf(stderr, "sdiff: cannot interactively merge standard input\n");
        return 2;
    }

    Input lf;
    Input rf;
    if (!read_input(file1, opts, &lf)) {
        (void)fprintf(stderr, "diff: %s: No such file or directory\n", file1);
        return 2;
    }
    if (!read_input(file2, opts, &rf)) {
        (void)fprintf(stderr, "diff: %s: No such file or directory\n", file2);
        free_input(lf);
        return 2;
    }

    FILE* out = fopen(output, "w");
    if (out == nullptr) {
        (void)fprintf(stderr, "sdiff: %s: %s\n", output, strerror(errno));
        free_input(lf);
        free_input(rf);
        return 2;
    }

    Renderer rr;
    rr.out = stdout;
    rr.layout = compute_layout(opts->width, opts->tabsize, opts->expand_tabs);
    rr.suppress_common = opts->suppress_common_lines;
    rr.left_column = opts->left_column;
    rr.expand_tabs = opts->expand_tabs;
    rr.tabsize = opts->tabsize;

    auto changes = compute_changes(lf, rf, opts);

    LineCursor lcur{&lf, 0};
    LineCursor rcur{&rf, 0};
    int lline = 1;
    int rline = 1;
    bool ok = true;

    for (const Change& ch : changes) {
        /* The common lines that precede this hunk are rendered to stdout and
         * copied once each to the merged output. A common line appears in
         * both files, so it is copied from the left and the right cursor is
         * advanced past it. */
        int const pre_l = lcur.pos;
        int const pre_r = rcur.pos;

        while (lcur.pos < ch.old_start && rcur.pos < ch.new_start) {
            cursor_copy(&lcur, 1, out);
            cursor_skip(&rcur, 1);
        }
        while (rcur.pos < ch.new_start) {
            cursor_copy(&rcur, 1, out);
        }
        while (lcur.pos < ch.old_start) {
            cursor_copy(&lcur, 1, out);
        }

        /* Render the common lines before this hunk to stdout. */
        if (rr.suppress_common == 0) {
            int i0 = pre_l;
            int i1 = pre_r;
            while (i0 < ch.old_start && i1 < ch.new_start) {
                print_1sdiff_line(lf.lines[static_cast<size_t>(i0)], ' ',
                                  lf.had_eol[static_cast<size_t>(i0)],
                                  rf.lines[static_cast<size_t>(i1)],
                                  rf.had_eol[static_cast<size_t>(i1)], &rr);
                i0++;
                i1++;
            }
            while (i1 < ch.new_start) {
                print_1sdiff_line(nullptr, ')', false,
                                  rf.lines[static_cast<size_t>(i1)],
                                  rf.had_eol[static_cast<size_t>(i1)], &rr);
                i1++;
            }
            while (i0 < ch.old_start) {
                print_1sdiff_line(lf.lines[static_cast<size_t>(i0)], '(', false,
                                  nullptr, false, &rr);
                i0++;
            }
        }
        lline = ch.old_start + 1;
        rline = ch.new_start + 1;
        lcur.pos = ch.old_start;
        rcur.pos = ch.new_start;

        /* Render the hunk itself to stdout. */
        int i = ch.old_start;
        int j = ch.new_start;
        for (; i < ch.old_start + ch.old_count && j < ch.new_start + ch.new_count;
             i++, j++) {
            print_1sdiff_line(lf.lines[static_cast<size_t>(i)], '|',
                              lf.had_eol[static_cast<size_t>(i)],
                              rf.lines[static_cast<size_t>(j)],
                              rf.had_eol[static_cast<size_t>(j)], &rr);
        }
        for (; j < ch.new_start + ch.new_count; j++) {
            print_1sdiff_line(nullptr, '>', false, rf.lines[static_cast<size_t>(j)],
                              rf.had_eol[static_cast<size_t>(j)], &rr);
        }
        for (; i < ch.old_start + ch.old_count; i++) {
            print_1sdiff_line(lf.lines[static_cast<size_t>(i)], '<', false,
                              nullptr, false, &rr);
        }

        if (!interact_hunk(file1, lline, ch.old_count, file2, rline, ch.new_count,
                           &lcur, &rcur, out, &rr)) {
            ok = false;
            break;
        }
    }

    int const tail_l = lcur.pos;
    int const tail_r = rcur.pos;
    if (ok) {
        /* Copy the trailing common lines to the merged output, once each. */
        while (lcur.pos < lf.valid_lines && rcur.pos < rf.valid_lines) {
            cursor_copy(&lcur, 1, out);
            cursor_skip(&rcur, 1);
        }
        while (rcur.pos < rf.valid_lines) {
            cursor_copy(&rcur, 1, out);
        }
        while (lcur.pos < lf.valid_lines) {
            cursor_copy(&lcur, 1, out);
        }
    }

    if (ok && rr.suppress_common == 0) {
        int i0 = tail_l;
        int i1 = tail_r;
        while (i0 < lf.valid_lines && i1 < rf.valid_lines) {
            print_1sdiff_line(lf.lines[static_cast<size_t>(i0)], ' ',
                              lf.had_eol[static_cast<size_t>(i0)],
                              rf.lines[static_cast<size_t>(i1)],
                              rf.had_eol[static_cast<size_t>(i1)], &rr);
            i0++;
            i1++;
        }
        while (i1 < rf.valid_lines) {
            print_1sdiff_line(nullptr, ')', false,
                              rf.lines[static_cast<size_t>(i1)],
                              rf.had_eol[static_cast<size_t>(i1)], &rr);
            i1++;
        }
        while (i0 < lf.valid_lines) {
            print_1sdiff_line(lf.lines[static_cast<size_t>(i0)], '(', false,
                              nullptr, false, &rr);
            i0++;
        }
    }

    (void)fclose(out);
    free_input(lf);
    free_input(rf);

    if (!ok) {
        return 2;
    }
    return 0;
}

int sdiff_command(int argc, char** argv) {
    SdiffOptions opts;
    const char* output = nullptr;
    std::vector<const char*> operands;
    bool end_of_options = false;

    for (int i = 1; i < argc; i++) {
        const char* raw = argv[i];
        std::string arg(raw);

        if (end_of_options || arg == "-" || arg.empty() || arg[0] != '-') {
            operands.push_back(raw);
            continue;
        }
        if (arg == "--") {
            end_of_options = true;
            continue;
        }

        auto long_value = [&](const char* name, const char** val) -> bool {
            std::string const prefix = std::string("--") + name;
            if (arg == prefix) {
                if (i + 1 < argc) {
                    i++;
                    *val = argv[i];
                    return true;
                }
                return false;
            }
            if (arg.starts_with(prefix + "=")) {
                *val = raw + prefix.size() + 1;
                return true;
            }
            return false;
        };

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        }
        if (arg == "-i" || arg == "--ignore-case") {
            opts.ignore_case = 1;
            continue;
        }
        if (arg == "-E" || arg == "--ignore-tab-expansion") {
            opts.ignore_tab_expansion = 1;
            continue;
        }
        if (arg == "-Z" || arg == "--ignore-trailing-space") {
            opts.ignore_trailing_space = 1;
            continue;
        }
        if (arg == "-b" || arg == "--ignore-space-change") {
            opts.ignore_space_change = 1;
            continue;
        }
        if (arg == "-W" || arg == "--ignore-all-space") {
            opts.ignore_all_space = 1;
            continue;
        }
        if (arg == "-B" || arg == "--ignore-blank-lines") {
            opts.ignore_blank_lines = 1;
            continue;
        }
        if (arg == "--strip-trailing-cr") {
            opts.strip_trailing_cr = 1;
            continue;
        }
        if (arg == "-a" || arg == "--text") {
            opts.text = 1;
            continue;
        }
        if (arg == "-l" || arg == "--left-column") {
            opts.left_column = 1;
            continue;
        }
        if (arg == "-s" || arg == "--suppress-common-lines") {
            opts.suppress_common_lines = 1;
            continue;
        }
        if (arg == "-t" || arg == "--expand-tabs") {
            opts.expand_tabs = 1;
            continue;
        }
        if (arg == "-d" || arg == "--minimal" || arg == "-H" ||
            arg == "--speed-large-files") {
            continue; /* accepted, no observable effect */
        }
        {
            const char* val = nullptr;
            if (long_value("diff-program", &val)) {
                (void)val; /* accepted; modbox has no external diff */
                continue;
            }
            if (long_value("ignore-matching-lines", &val)) {
                if (val == nullptr) {
                    try_help_exit("option '--ignore-matching-lines' requires an argument", nullptr);
                }
                opts.ignore_matching_lines = val;
                continue;
            }
            if (arg == "-I" || (arg.size() > 2 && arg.starts_with("-I"))) {
                val = take_value(arg, 2, argc, argv, &i);
                if (val == nullptr) {
                    try_help_exit("option requires an argument -- 'I'", nullptr);
                }
                opts.ignore_matching_lines = val;
                continue;
            }
            if (long_value("output", &val)) {
                if (val == nullptr) {
                    try_help_exit("option '--output' requires an argument", nullptr);
                }
                output = val;
                continue;
            }
            if (arg == "-o" || (arg.size() > 2 && arg.starts_with("-o"))) {
                val = take_value(arg, 2, argc, argv, &i);
                if (val == nullptr) {
                    try_help_exit("option requires an argument -- 'o'", nullptr);
                }
                output = val;
                continue;
            }
            if (long_value("width", &val)) {
                if (val == nullptr) {
                    try_help_exit("option '--width' requires an argument", nullptr);
                }
                if (!parse_positive_int(val, &opts.width)) {
                    (void)fprintf(stderr, "diff: invalid width '%s'\n", val);
                    (void)fprintf(stderr, "diff: Try 'diff --help' for more information.\n");
                    return 2;
                }
                continue;
            }
            if (arg == "-w" || (arg.size() > 2 && arg.starts_with("-w"))) {
                val = take_value(arg, 2, argc, argv, &i);
                if (val == nullptr) {
                    try_help_exit("option requires an argument -- 'w'", nullptr);
                }
                if (!parse_positive_int(val, &opts.width)) {
                    (void)fprintf(stderr, "diff: invalid width '%s'\n", val);
                    (void)fprintf(stderr, "diff: Try 'diff --help' for more information.\n");
                    return 2;
                }
                continue;
            }
            if (long_value("tabsize", &val)) {
                if (val == nullptr) {
                    try_help_exit("option '--tabsize' requires an argument", nullptr);
                }
                if (!parse_positive_int(val, &opts.tabsize)) {
                    (void)fprintf(stderr, "diff: invalid tabsize '%s'\n", val);
                    (void)fprintf(stderr, "diff: Try 'diff --help' for more information.\n");
                    return 2;
                }
                continue;
            }
        }

        (void)fprintf(stderr, "sdiff: unrecognized option '%s'\n", raw);
        (void)fprintf(stderr, "Try 'sdiff --help' for more information.\n");
        return 2;
    }

    if (operands.size() != 2) {
        if (operands.size() < 2) {
            const char* last = operands.empty() ? "sdiff" : operands.back();
            (void)fprintf(stderr, "sdiff: missing operand after '%s'\n", last);
        } else {
            (void)fprintf(stderr, "sdiff: extra operand '%s'\n", operands[2]);
        }
        (void)fprintf(stderr, "Try 'sdiff --help' for more information.\n");
        return 2;
    }

    const char* file1 = operands[0];
    const char* file2 = operands[1];

    if (output == nullptr) {
        Input lf;
        Input rf;
        if (!read_input(file1, &opts, &lf)) {
            (void)fprintf(stderr, "diff: %s: No such file or directory\n", file1);
            return 2;
        }
        if (!read_input(file2, &opts, &rf)) {
            (void)fprintf(stderr, "diff: %s: No such file or directory\n", file2);
            free_input(lf);
            return 2;
        }

        Renderer r;
        r.out = stdout;
        r.layout = compute_layout(opts.width, opts.tabsize, opts.expand_tabs);
        r.suppress_common = opts.suppress_common_lines;
        r.left_column = opts.left_column;
        r.expand_tabs = opts.expand_tabs;
        r.tabsize = opts.tabsize;

        auto changes = compute_changes(lf, rf, &opts);
        render_script(lf, rf, changes, &r);

        /* compute_changes already drops differences that the ignore options
         * (-B, -I, whitespace, case, ...) make insignificant, so any change
         * left is a real one. Basing the status solely on that list keeps
         * `-B`/`-I` inputs that differ only in ignored lines at exit 0. */
        int const differ = !changes.empty();
        free_input(lf);
        free_input(rf);
        return differ ? 1 : 0;
    }

    /* Interactive merge. */
    return sdiff_merge(file1, file2, output, &opts);
}

REGISTER_COMMAND("sdiff", sdiff_command, "Side-by-side merge of file differences")
