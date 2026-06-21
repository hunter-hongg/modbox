#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <glib.h>
#include <argtable3.h>

#include "commands/sort.h"

/* ── Internal data structures ──────────────────────────────────────────── */

/* Flags for ParsedKey */
#define KEY_FLAG_SKIP_BLANKS  (1 << 0)  /* b */
#define KEY_FLAG_IGNORE_CASE  (1 << 1)  /* f */
#define KEY_FLAG_NUMERIC      (1 << 2)  /* n */
#define KEY_FLAG_REVERSE      (1 << 3)  /* r */

typedef struct {
    int field_start;    /* 1-indexed field, 0 = error / whole line */
    int char_start;     /* 1-indexed character, 0 = start of field */
    int field_end;      /* 0 = end of line */
    int char_end;       /* 0 = end of field */
    int flags;          /* KEY_FLAG_* bitmask */
} ParsedKey;

typedef struct {
    gchar *line;
    int orig_index;
} SortLine;

/* ── Memory management ─────────────────────────────────────────────────── */

static SortLine *sort_line_new(const gchar *line, int index) {
    SortLine *sl = g_malloc(sizeof(SortLine));
    sl->line = g_strdup(line);
    sl->orig_index = index;
    return sl;
}

static void sort_line_free(SortLine *sl) {
    if (sl) {
        g_free(sl->line);
        g_free(sl);
    }
}

static void sort_line_free_wrapper(gpointer data) {
    sort_line_free((SortLine *)data);
}

/* ── Field extraction utilities ─────────────────────────────────────────── */

/* In default mode, fields are separated by transitions from blank to non-blank.
   With -t, fields are separated by a single character and empty fields are preserved. */

/* Skip leading blanks (spaces/tabs) */
static const char *skip_blanks(const char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

/* Find the start of field N (1-indexed). Returns pointer or NULL if field doesn't exist. */
static const char *find_field_start(const char *line, int field_num, char sep) {
    if (field_num <= 0) {
        return line;
    }
    const char *p = line;
    int current = 1;

    if (sep == 0) {
        /* Default: blank-to-non-blank transitions */
        /* Skip leading blanks */
        p = skip_blanks(p);
        if (field_num == 1) {
            return (*p == '\0') ? NULL : p;
        }
        current = 1;
        while (*p) {
            /* Scan to next blank */
            while (*p && *p != ' ' && *p != '\t') {
                p++;
            }
            if (*p == '\0') {
                return NULL;
            }
            /* Skip blanks to find next field */
            p = skip_blanks(p);
            current++;
            if (current == field_num) {
                return (*p == '\0') ? NULL : p;
            }
        }
    } else {
        /* Single-character separator */
        while (*p) {
            if (current == field_num) {
                return p;
            }
            if (*p == sep) {
                current++;
            }
            p++;
        }
        /* After the last separator, there's an empty field */
        if (current == field_num) {
            return p; /* points to '\0', empty field */
        }
    }
    return NULL;
}

/* Find the end of field N (one past the last character). */
static const char *find_field_end(const char *line, int field_num, char sep, const char *start) {
    if (field_num <= 0) {
        return line + strlen(line);
    }
    const char *p = start;
    if (p == NULL) {
        return NULL;
    }

    if (sep == 0) {
        /* End at next blank or end of string */
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
    } else {
        /* End at next separator or end of string */
        while (*p && *p != sep) {
            p++;
        }
    }
    return p;
}

/* Extract the key portion of a line according to a ParsedKey. Returns newly allocated string. */
static gchar *extract_key(const gchar *line, const ParsedKey *key, char sep) {
    const char *start;
    const char *end;

    if (key->field_start <= 0) {
        /* Whole line as key */
        start = line;
        end = line + strlen(line);
    } else {
        start = find_field_start(line, key->field_start, sep);
        if (start == NULL) {
            return g_strdup("");
        }

        /* Apply char_start offset (1-indexed) */
        if (key->char_start > 1) {
            int offset = key->char_start - 1;
            while (offset > 0 && *start) {
                start++;
                offset--;
            }
        }

        if (key->field_end > 0) {
            end = find_field_end(line, key->field_end, sep,
                                 find_field_start(line, key->field_end, sep));
            if (end == NULL) {
                end = line + strlen(line);
            }
            /* Apply char_end offset */
            if (key->char_end > 0) {
                const char *fstart = find_field_start(line, key->field_end, sep);
                if (fstart) {
                    int offset = key->char_end;
                    const char *p = fstart;
                    while (offset > 1 && *p && p < end) {
                        p++;
                        offset--;
                    }
                    end = p;
                }
            }
        } else {
            end = line + strlen(line);
        }
    }

    /* Handle KEY_FLAG_SKIP_BLANKS: skip leading blanks within the key */
    const char *effective_start = start;
    if (key->flags & KEY_FLAG_SKIP_BLANKS) {
        effective_start = skip_blanks(start);
        if (effective_start > end) {
            effective_start = end;
        }
    }

    if (effective_start >= end) {
        return g_strdup("");
    }

    return g_strndup(effective_start, (gsize)(end - effective_start));
}

/* ── Number parsing helpers ─────────────────────────────────────────────── */

static gdouble parse_numeric(const char *s, int *ok) {
    char *end = NULL;
    /* Skip leading blanks */
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    /* Allow leading negative sign */
    if (*s == '-' || *s == '+') {
        s++;
    }
    /* Must start with a digit or decimal point */
    if (!g_ascii_isdigit(*s) && *s != '.') {
        *ok = 0;
        return 0.0;
    }
    /* Reset and parse properly */
    // NOLINTNEXTLINE(cert-err34-c)
    gdouble val = g_ascii_strtod(s, &end);
    if (end == s) {
        *ok = 0;
        return 0.0;
    }
    *ok = 1;
    return val;
}

/* ── Comparison functions ────────────────────────────────────────────────── */

/* Compare two strings using the current sort options on a single key.
   Returns <0, 0, >0 like strcmp. */
static int compare_key_strings(const gchar *a, const gchar *b,
                                const ParsedKey *key) {
    const gchar *sa = a;
    const gchar *sb = b;

    if (key->flags & KEY_FLAG_NUMERIC) {
        int ok_a = 0, ok_b = 0;
        gdouble va = parse_numeric(sa, &ok_a);
        gdouble vb = parse_numeric(sb, &ok_b);
        if (!ok_a && !ok_b) {
            return 0;
        }
        if (!ok_a) {
            return -1;
        }
        if (!ok_b) {
            return 1;
        }
        if (va < vb) {
            return -1;
        }
        if (va > vb) {
            return 1;
        }
        return 0;
    }

    if (key->flags & KEY_FLAG_IGNORE_CASE) {
        return g_ascii_strcasecmp(sa, sb);
    }

    return strcmp(sa, sb);
}

/* Compare content of two SortLine entries WITHOUT index tiebreaker.
   Used for -u dedup and -c -u unique checks. */
static gint compare_content(const SortLine *la,
                             const SortLine *lb,
                             const SortOptions *opts) {

    int result = 0;

    if (opts->keys && opts->keys->len > 0) {
        /* Compare by key fields */
        for (unsigned int i = 0; i < opts->keys->len; i++) {
            const ParsedKey *key = (const ParsedKey *)g_ptr_array_index(opts->keys, i);
            gchar *key_a = extract_key(la->line, key, opts->field_separator);
            gchar *key_b = extract_key(lb->line, key, opts->field_separator);

            result = compare_key_strings(key_a, key_b, key);

            g_free(key_a);
            g_free(key_b);

            if (result != 0) {
                break;
            }
        }

        /* Tiebreaker: if keys are equal and not stable, compare whole lines */
        if (result == 0 && !opts->stable) {
            result = strcmp(la->line, lb->line);
        }
    } else {
        /* No keys: sort by whole line */
        const gchar *sa = la->line;
        const gchar *sb = lb->line;

        /* Apply -b globally: skip leading blanks */
        if (opts->ignore_leading_blanks) {
            sa = skip_blanks(sa);
            sb = skip_blanks(sb);
        }

        if (opts->numeric_sort) {
            int ok_a = 0, ok_b = 0;
            gdouble va = parse_numeric(sa, &ok_a);
            gdouble vb = parse_numeric(sb, &ok_b);
            if (ok_a && ok_b) {
                result = (va < vb) ? -1 : (va > vb) ? 1 : 0;
            } else if (!ok_a && !ok_b) {
                result = 0;
            } else if (!ok_a) {
                result = -1;
            } else {
                result = 1;
            }
        } else {
            if (opts->ignore_case) {
                result = g_ascii_strcasecmp(sa, sb);
            } else {
                result = strcmp(sa, sb);
            }
        }
    }

    /* Apply reverse */
    if (opts->reverse) {
        result = -result;
    }

    return result;
}

/* Compare two SortLine entries including index tiebreaker for stable sort.
   The index tiebreaker ensures deterministic ordering: for equal content,
   the line that appeared first in input keeps its position. */
static gint compare_sort_lines_direct(const SortLine *la,
                                       const SortLine *lb,
                                       const SortOptions *opts) {
    int result = compare_content(la, lb, opts);

    /* Use original index as tiebreaker for stable sorting */
    if (result == 0) {
        if (la->orig_index < lb->orig_index) {
            result = -1;
        } else if (la->orig_index > lb->orig_index) {
            result = 1;
        }
    }

    return result;
}

/* Wrapper for g_ptr_array_sort_with_data: double-dereferences the SortLine**
   pointers that GLib passes to the comparison callback. */
static gint compare_sort_lines(gconstpointer a, gconstpointer b,
                                gpointer user_data) {
    const SortLine *la = *(const SortLine * const *)a;
    const SortLine *lb = *(const SortLine * const *)b;
    const SortOptions *opts = (const SortOptions *)user_data;
    return compare_sort_lines_direct(la, lb, opts);
}

/* ── Key spec parsing ───────────────────────────────────────────────────── */

/* Parse a single key spec like "2.3,4.5br" into a ParsedKey.
   Format: F[.C][OPTS][,F[.C][OPTS]]
   Returns newly allocated ParsedKey. */
static ParsedKey *parse_one_key(const gchar *spec) {
    ParsedKey *key = g_malloc0(sizeof(ParsedKey));
    const gchar *p = spec;

    /* Parse field_start */
    if (g_ascii_isdigit(*p)) {
        key->field_start = (int)strtol(p, (char **)&p, 10);
    } else {
        key->field_start = 1;
    }

    /* Parse optional .char_start */
    if (*p == '.') {
        p++;
        if (g_ascii_isdigit(*p)) {
            key->char_start = (int)strtol(p, (char **)&p, 10);
        }
    }

    /* Parse optional flags before comma */
    while (*p && *p != ',') {
        switch (*p) {
            case 'b': key->flags |= KEY_FLAG_SKIP_BLANKS; break;
            case 'f': key->flags |= KEY_FLAG_IGNORE_CASE; break;
            case 'n': key->flags |= KEY_FLAG_NUMERIC; break;
            case 'r': key->flags |= KEY_FLAG_REVERSE; break;
            default: break;
        }
        p++;
    }

    /* Parse optional second part after comma */
    if (*p == ',') {
        p++;
        if (g_ascii_isdigit(*p)) {
            key->field_end = (int)strtol(p, (char **)&p, 10);
        } else {
            key->field_end = key->field_start;
        }

        if (*p == '.') {
            p++;
            if (g_ascii_isdigit(*p)) {
                key->char_end = (int)strtol(p, (char **)&p, 10);
            }
        }

        /* Parse optional flags after comma */
        while (*p) {
            switch (*p) {
                case 'b': key->flags |= KEY_FLAG_SKIP_BLANKS; break;
                case 'f': key->flags |= KEY_FLAG_IGNORE_CASE; break;
                case 'n': key->flags |= KEY_FLAG_NUMERIC; break;
                case 'r': key->flags |= KEY_FLAG_REVERSE; break;
                default: break;
            }
            p++;
        }
    }

    return key;
}

static void parsed_key_free(gpointer data) {
    g_free(data);
}

/* Parse all key specs from a single -k argument (GNU sort accepts multiple -k).
   Each call handles one -k value. */
static void add_key_from_spec(const gchar *spec, GPtrArray *keys) {
    ParsedKey *key = parse_one_key(spec);
    g_ptr_array_add(keys, key);
}

/* ── I/O: reading lines ─────────────────────────────────────────────────── */

static GPtrArray *read_lines(int file_count, const gchar **filenames, int check_mode) {
    GPtrArray *lines = g_ptr_array_new_with_free_func(sort_line_free_wrapper);
    int line_index = 0;
    int file_index = 0;

    /* If no files, read from stdin */
    if (file_count == 0) {
        gchar buf[8192];
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        while (fgets(buf, (int)sizeof(buf), stdin)) {
            /* Remove trailing newline */
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }
            /* In check mode, track file mapping for error messages */
            g_ptr_array_add(lines, sort_line_new(buf, line_index));
            line_index++;
        }
        return lines;
    }

    /* Read from each file */
    for (int i = 0; i < file_count; i++) {
        const gchar *fname = filenames[i];
        FILE *fp = stdin;

        if (strcmp(fname, "-") == 0) {
            fp = stdin;
        } else {
            fp = fopen(fname, "r");
            if (fp == NULL) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "sort: %s: No such file or directory\n",
                              fname);
                /* Continue with remaining files / stdin for check mode */
                continue;
            }
        }

        gchar buf[8192];
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        while (fgets(buf, (int)sizeof(buf), fp)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }
            g_ptr_array_add(lines, sort_line_new(buf, line_index));
            line_index++;
        }

        if (fp != stdin) {
            // NOLINTNEXTLINE(bugprone-unused-return-value)
            (void)fclose(fp);
        }
    }

    return lines;
}

/* ── I/O: writing lines ─────────────────────────────────────────────────── */

static void write_lines(GPtrArray *lines, const gchar *output_file) {
    FILE *fp = stdout;

    if (output_file) {
        fp = fopen(output_file, "w");
        if (fp == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "sort: %s: Cannot open for writing: %s\n",
                          output_file, strerror(errno));
            return;
        }
    }

    for (unsigned int i = 0; i < lines->len; i++) {
        SortLine *sl = (SortLine *)g_ptr_array_index(lines, i);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(fp, "%s\n", sl->line);
    }

    if (fp != stdout) {
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)fclose(fp);
    }
}

/* ── Check mode ──────────────────────────────────────────────────────────── */

/* Check if the lines are already sorted. Returns 0 if sorted, 1 if not.
   Prints the first disorder to stderr. */
static int check_sorted(GPtrArray *lines, const SortOptions *opts,
                         int file_count, const gchar **filenames) {
    for (unsigned int i = 1; i < lines->len; i++) {
        SortLine *prev = (SortLine *)g_ptr_array_index(lines, i - 1);
        SortLine *curr = (SortLine *)g_ptr_array_index(lines, i);

        /* Use compare function: result > 0 means previous > current → disorder */
        int cmp = compare_sort_lines_direct(prev, curr, opts);

        if (cmp > 0) {
            /* Out of order: find which file/line */
            int line_num = i;   /* 1-indexed for display */
            const gchar *fname = filenames && file_count > 0
                                 ? filenames[0] : "-";

            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "sort: %s: line %d: disorder: %s\n",
                          fname, line_num + 1, curr->line);
            return 1;
        }

        /* -c -u check: if adjacent lines compare equal by content, that's disorder */
        if (opts->unique && compare_content(prev, curr, opts) == 0) {
            int line_num = i;
            const gchar *fname = filenames && file_count > 0
                                 ? filenames[0] : "-";

            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "sort: %s: line %d: disorder: %s\n",
                          fname, line_num + 1, curr->line);
            return 1;
        }
    }

    return 0;
}

/* ── Main command ────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void sort_command(gint argc, gchar **argv) {
    SortOptions opts = {0};

    /* argtable3 declarations */
    struct arg_lit *ignore_blanks_opt = arg_lit0("b", "ignore-leading-blanks",
                                                  "ignore leading blanks");
    struct arg_lit *ignore_case_opt = arg_lit0("f", "ignore-case",
                                                "fold lower case to upper case characters");
    struct arg_lit *numeric_sort_opt = arg_lit0("n", "numeric-sort",
                                                 "compare according to string numerical value");
    struct arg_lit *reverse_opt = arg_lit0("r", "reverse",
                                            "reverse the result of comparisons");
    struct arg_lit *unique_opt = arg_lit0("u", "unique",
                                           "with -c, check for strict ordering; "
                                           "without -c, output only the first of an equal run");
    struct arg_lit *check_opt = arg_lit0("c", "check",
                                          "check whether input is sorted; "
                                          "do not sort");
    struct arg_lit *stable_opt = arg_lit0("s", "stable",
                                           "stabilize sort by disabling last-resort comparison");
    struct arg_str *key_opt = arg_strn("k", "key", "POS1[,POS2]", 0, 100,
                                        "sort via a key; POS is F[.C][OPTS]");
    struct arg_str *field_sep_opt = arg_str0("t", "field-separator", "SEP",
                                              "use SEP as field separator");
    struct arg_str *output_opt = arg_str0("o", "output", "FILE",
                                           "write result to FILE instead of standard output");
    struct arg_lit *help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        ignore_blanks_opt, ignore_case_opt, numeric_sort_opt,
        reverse_opt, unique_opt, check_opt, stable_opt,
        key_opt, field_sep_opt, output_opt,
        help_opt,
        file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Sort lines of text files.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -b, --ignore-leading-blanks   ignore leading blanks\n");
        printf("  -f, --ignore-case             fold lower case to upper case\n");
        printf("  -n, --numeric-sort            compare according to string numerical value\n");
        printf("  -r, --reverse                 reverse the result of comparisons\n");
        printf("  -u, --unique                  with -c, check for strict ordering;\n");
        printf("                                without -c, output only the first of an equal run\n");
        printf("  -c, --check                   check whether input is sorted; do not sort\n");
        printf("  -s, --stable                  stabilize sort by disabling last-resort comparison\n");
        printf("  -k, --key=POS1[,POS2]         sort via a key; POS is F[.C][OPTS]\n");
        printf("  -t, --field-separator=SEP     use SEP instead of non-blank to blank transition\n");
        printf("  -o, --output=FILE             write result to FILE instead of standard output\n");
        printf("  -h, --help                    display this help and exit\n");
        printf("\n");
        printf("Key flags (append after position):\n");
        printf("  b   ignore leading blanks in the key\n");
        printf("  f   ignore case within the key\n");
        printf("  n   numeric comparison within the key\n");
        printf("  r   reverse comparison within the key\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* Populate options */
    opts.ignore_leading_blanks = (ignore_blanks_opt->count > 0);
    opts.ignore_case = (ignore_case_opt->count > 0);
    opts.numeric_sort = (numeric_sort_opt->count > 0);
    opts.reverse = (reverse_opt->count > 0);
    opts.unique = (unique_opt->count > 0);
    opts.check = (check_opt->count > 0);
    opts.stable = (stable_opt->count > 0);
    opts.output_file = (char *)(output_opt->count > 0 ? output_opt->sval[0] : NULL);
    opts.keys = g_ptr_array_new_with_free_func(parsed_key_free);

    /* Parse field separator */
    if (field_sep_opt->count > 0) {
        if (field_sep_opt->sval[0][0] == '\0') {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "sort: invalid field separator\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            g_ptr_array_free(opts.keys, TRUE);
            return;
        }
        opts.field_separator = field_sep_opt->sval[0][0];
    } else {
        opts.field_separator = 0;
    }

    /* Parse key specs */
    for (int i = 0; i < key_opt->count; i++) {
        add_key_from_spec(key_opt->sval[i], opts.keys);
        /* Apply global flags to key flags as defaults.
           Key-specific flags (explicitly parsed) take precedence. */
        ParsedKey *key = (ParsedKey *)g_ptr_array_index(opts.keys, i);
        if ((key->flags & KEY_FLAG_SKIP_BLANKS) == 0 && opts.ignore_leading_blanks) {
            key->flags |= KEY_FLAG_SKIP_BLANKS;
        }
        if ((key->flags & KEY_FLAG_IGNORE_CASE) == 0 && opts.ignore_case) {
            key->flags |= KEY_FLAG_IGNORE_CASE;
        }
        if ((key->flags & KEY_FLAG_NUMERIC) == 0 && opts.numeric_sort) {
            key->flags |= KEY_FLAG_NUMERIC;
        }
        if ((key->flags & KEY_FLAG_REVERSE) == 0 && opts.reverse) {
            key->flags |= KEY_FLAG_REVERSE;
        }
    }

    /* Collect filenames */
    int file_count = file_arg->count;
    const gchar **filenames = (const gchar **)(file_arg->filename);

    /* Read all lines */
    GPtrArray *lines = read_lines(file_count, filenames, opts.check);
    if (lines == NULL || lines->len == 0) {
        g_ptr_array_free(lines, TRUE);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        g_ptr_array_free(opts.keys, TRUE);
        return;
    }

    /* Check mode or sort */
    if (opts.check) {
        int sorted = check_sorted(lines, &opts, file_count, filenames);
        g_ptr_array_free(lines, TRUE);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        g_ptr_array_free(opts.keys, TRUE);
        exit(sorted ? 1 : 0);
    }

    /* Sort */
    g_ptr_array_sort_with_data(lines, compare_sort_lines, &opts);

    /* Unique: remove consecutive duplicates */
    if (opts.unique) {
        GPtrArray *uniq = g_ptr_array_new_with_free_func(sort_line_free_wrapper);
        if (lines->len > 0) {
            g_ptr_array_add(uniq,
                            sort_line_new(((SortLine *)g_ptr_array_index(lines, 0))->line, 0));
        }
        for (unsigned int i = 1; i < lines->len; i++) {
            SortLine *prev = (SortLine *)g_ptr_array_index(uniq, uniq->len - 1);
            SortLine *curr = (SortLine *)g_ptr_array_index(lines, i);
            /* Use compare_content (no index tiebreaker) to detect true duplicates */
            if (compare_content(prev, curr, &opts) != 0) {
                g_ptr_array_add(uniq,
                                sort_line_new(curr->line, (int)uniq->len));
            }
        }
        g_ptr_array_free(lines, TRUE);
        lines = uniq;
    }

    /* Write output */
    write_lines(lines, opts.output_file);

    /* Cleanup */
    g_ptr_array_free(lines, TRUE);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    g_ptr_array_free(opts.keys, TRUE);
}
