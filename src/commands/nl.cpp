#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <regex>
#include <argtable3.h>

#include "commands/nl.hpp"
#include "commands/command_macros.hpp"

#define NL_MAX_LINE 1048576
#define NL_DEFAULT_WIDTH 6

/* Check if a line is a section delimiter.
 * Returns: 3 = header, 2 = body, 1 = footer, 0 = not a delimiter. */
static int check_section_delimiter(const char* line, const char delim[2]) {
    if (strlen(line) < 2) {
        return 0;
    }
    /* Count consecutive delimiter pairs at start of line */
    int count = 0;
    const char* p = line;
    while (*p == delim[0] && *(p + 1) == delim[1]) {
        count++;
        p += 2;
    }
    /* Allow trailing whitespace (GNU compat) */
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    /* Ensure nothing else follows */
    if (*p != '\0' && *p != '\n') {
        return 0;
    }
    if (count >= 3) {
        return 3; /* header */
    }
    if (count == 2) {
        return 2; /* body */
    }
    if (count == 1) {
        return 1; /* footer */
    }
    return 0;
}

/* Check if style is pBRE and line matches. Returns 1 if line should be numbered. */
static int should_number(const char* line, const char* style, int is_empty) {
    if (style == NULL || strcmp(style, "a") == 0) {
        return 1;
    }
    if (strcmp(style, "t") == 0) {
        return !is_empty;
    }
    if (strcmp(style, "n") == 0) {
        return 0;
    }
    /* pBRE format */
    if (style[0] == 'p') {
        const char* bre = style + 1;
        if (*bre == '\0') {
            return 1; /* empty BRE matches all */
        }
        try {
            std::regex re(bre);
            if (std::regex_search(line, re)) {
                return 1;
            }
            return 0;
        } catch (const std::regex_error&) {
            return 1; /* fallback: number all */
        }
    }
    return 1; /* fallback */
}

/* Format the line number into buffer according to format and width. */
static void format_number(int num, const char* fmt, int width, char* buf, size_t buf_size) {
    if (fmt == NULL || strcmp(fmt, "rn") == 0) {
        /* Right-justified, no leading zeros */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%*d", width, num);
    } else if (strcmp(fmt, "rz") == 0) {
        /* Right-justified, leading zeros */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%0*d", width, num);
    } else if (strcmp(fmt, "ln") == 0) {
        /* Left-justified */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%-*d", width, num);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buf_size, "%*d", width, num);
    }
}

/* Process input reading line by line, applying nl formatting. */
static void nl_process(FILE* fp, const NlOptions* opts, FILE* out_fp) {
    char line_buf[NL_MAX_LINE];
    int line_num;
    int blank_count = 0;
    int page_state = 2; /* start in body section */

    line_num = opts->starting_line_number;

    while (fgets(line_buf, NL_MAX_LINE, fp)) {
        size_t len = strlen(line_buf);
        /* Strip trailing newline for processing */
        int has_newline = (len > 0 && line_buf[len - 1] == '\n');
        if (has_newline) {
            line_buf[len - 1] = '\0';
            len--;
        }

        /* Check for section delimiter */
        int delim_type = check_section_delimiter(line_buf, opts->section_delimiters);
        if (delim_type > 0) {
            page_state = delim_type;
            if (!opts->no_renumber) {
                line_num = opts->starting_line_number;
            }
            blank_count = 0;
            continue;
        }

        /* Determine which numbering style to use */
        const char* num_style = NULL;
        if (page_state == 3) {
            num_style = opts->header_numbering;
        } else if (page_state == 2) {
            num_style = opts->body_numbering;
        } else {
            num_style = opts->footer_numbering;
        }

        /* Check if line is empty */
        int is_empty = (len == 0);

        /* Handle --join-blank-lines: group N consecutive blank lines as one logical line
         * for numbering purposes. All blank lines are still output. */
        int do_number;
        if (is_empty) {
            blank_count++;
            /* Number this blank line if it is the first in its logical group,
             * or if join-blank-lines is not active */
            if (opts->join_blank_lines > 0) {
                /* Only the first blank in each group of N gets numbered */
                if ((blank_count - 1) % opts->join_blank_lines == 0) {
                    do_number = should_number(line_buf, num_style, is_empty);
                } else {
                    do_number = 0;
                }
            } else {
                do_number = should_number(line_buf, num_style, is_empty);
            }
        } else {
            blank_count = 0;
            do_number = should_number(line_buf, num_style, is_empty);
        }

        if (do_number) {
            char num_buf[64];
            format_number(line_num, opts->number_format, opts->number_width, num_buf, sizeof(num_buf));
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out_fp, "%s%s%s\n", num_buf, opts->number_separator, line_buf);
            line_num += opts->line_increment;
        } else {
            /* Print line without number */
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out_fp, "%s\n", line_buf);
        }
    }
}

/* ── Main command ────────────────────────────────────────────────────────── */

void nl_command(int argc, char** argv) {
    NlOptions opts = {0};

    struct arg_str* body_numbering_opt = arg_str0("b", "body-numbering", "STYLE", "line numbering style (a, t, n, pBRE)");
    struct arg_str* header_numbering_opt = arg_str0("h", "header-numbering", "STYLE", "header numbering style");
    struct arg_str* footer_numbering_opt = arg_str0("f", "footer-numbering", "STYLE", "footer numbering style");
    struct arg_str* section_delim_opt = arg_str0("d", "section-delimiter", "CC", "two-character section delimiter (default: \\:)");
    struct arg_int* line_inc_opt = arg_int0("i", "line-increment", "NUMBER", "line number increment");
    struct arg_int* join_blanks_opt = arg_int0("l", "join-blank-lines", "NUMBER", "group N blank lines as one");
    struct arg_str* num_format_opt = arg_str0("n", "number-format", "FORMAT", "number format (ln, rn, rz)");
    struct arg_lit* no_renumber_opt = arg_lit0("p", "no-renumber", "do not reset line numbers at each page");
    struct arg_str* separator_opt = arg_str0("s", "number-separator", "STRING", "separator between number and line (default: TAB)");
    struct arg_int* start_num_opt = arg_int0("v", "starting-line-number", "NUMBER", "initial line number");
    struct arg_int* width_opt = arg_int0("w", "number-width", "NUMBER", "width of line number");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 1, "input file");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        body_numbering_opt, header_numbering_opt, footer_numbering_opt,
        section_delim_opt, line_inc_opt, join_blanks_opt,
        num_format_opt, no_renumber_opt, separator_opt,
        start_num_opt, width_opt,
        help_opt,
        file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]\n", argv[0]);
        printf("Number lines of files.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -b, --body-numbering=STYLE    line numbering style for body (a|t|n|pBRE)\n");
        printf("  -d, --section-delimiter=CC    two-character section delimiter\n");
        printf("  -f, --footer-numbering=STYLE  line numbering style for footer (a|t|n|pBRE)\n");
        printf("  -h, --header-numbering=STYLE  line numbering style for header (a|t|n|pBRE)\n");
        printf("  -i, --line-increment=NUMBER   line number increment\n");
        printf("  -l, --join-blank-lines=NUMBER  group N blank lines as one\n");
        printf("  -n, --number-format=FORMAT    number format (ln|rn|rz)\n");
        printf("  -p, --no-renumber             do not reset line numbers at each page\n");
        printf("  -s, --number-separator=STRING  separator between number and line\n");
        printf("  -v, --starting-line-number=N   initial line number\n");
        printf("  -w, --number-width=NUMBER      width of line number\n");
        printf("  -h, --help                    display this help and exit\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("STYLE is one of:\n");
        printf("  a      number all lines (default for body)\n");
        printf("  t      number only nonempty lines (default for header/footer)\n");
        printf("  n      number no lines\n");
        printf("  pBRE   number only lines matching basic regular expression BRE\n");
        printf("\n");
        printf("Default section delimiter is '\\\\:' (backslash followed by colon).\n");
        printf("Lines containing the delimiter repeated 3 times start a header section,\n");
        printf("2 times start a body section, 1 time start a footer section.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* Set defaults */
    opts.body_numbering = strdup(body_numbering_opt->count > 0 ? body_numbering_opt->sval[0] : "a");
    opts.header_numbering = strdup(header_numbering_opt->count > 0 ? header_numbering_opt->sval[0] : "t");
    opts.footer_numbering = strdup(footer_numbering_opt->count > 0 ? footer_numbering_opt->sval[0] : "t");
    opts.number_format = strdup(num_format_opt->count > 0 ? num_format_opt->sval[0] : "rn");
    opts.line_increment = (line_inc_opt->count > 0 ? line_inc_opt->ival[0] : 1);
    opts.join_blank_lines = (join_blanks_opt->count > 0 ? join_blanks_opt->ival[0] : 0);
    opts.no_renumber = (no_renumber_opt->count > 0);
    opts.starting_line_number = (start_num_opt->count > 0 ? start_num_opt->ival[0] : 1);
    opts.number_width = (width_opt->count > 0 ? width_opt->ival[0] : NL_DEFAULT_WIDTH);

    if (separator_opt->count > 0) {
        opts.number_separator = strdup(separator_opt->sval[0]);
    } else {
        opts.number_separator = strdup("\t");
    }

    /* Section delimiter: default is backslash-colon */
    if (section_delim_opt->count > 0) {
        const char* d = section_delim_opt->sval[0];
        opts.section_delimiters[0] = d[0];
        opts.section_delimiters[1] = (d[1] != '\0') ? d[1] : d[0];
    } else {
        opts.section_delimiters[0] = '\\';
        opts.section_delimiters[1] = ':';
    }

    /* Input */
    FILE* fp = stdin;
    if (file_arg->count > 0) {
        const char* fname = file_arg->filename[0];
        if (strcmp(fname, "-") != 0) {
            fp = fopen(fname, "r");
            if (fp == NULL) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "nl: %s: No such file or directory\n", fname);
                free(opts.body_numbering);
                free(opts.header_numbering);
                free(opts.footer_numbering);
                free(opts.number_format);
                free(opts.number_separator);
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
        }
    }

    nl_process(fp, &opts, stdout);

    if (fp != stdin) { (void)fclose(fp); }

    free(opts.body_numbering);
    free(opts.header_numbering);
    free(opts.footer_numbering);
    free(opts.number_format);
    free(opts.number_separator);

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("nl", nl_command, "Number lines of files");
