#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <string>
#include <argtable3.h>

#include "commands/cat.hpp"
#include "commands/pager.hpp"
#include "commands/cat/helpers.hpp"
#include "commands/cat/blame.hpp"
#include "commands/cat/highlight.hpp"
#include "commands/cat/diff.hpp"
#include "commands/command_macros.hpp"

#define ASCII_DEL          127
#define ASCII_128          128
#define ASCII_CP1252_END   159
#define ASCII_160          160
#define ASCII_255          255
#define ARG_END_SIZE       30

static void do_cleanup_expanded(char** my_argv, char** argv, int orig_argc, int argc, int expanded) {
    if (!expanded) return;
    for (int i = 0; i < argc; i++) {
        int from_original = 0;
        for (int j = 0; j < orig_argc; j++) {
            if (my_argv[i] == argv[j]) {
                from_original = 1;
                break;
            }
        }
        if (!from_original) {
            free(my_argv[i]);
        }
    }
    free((void*)my_argv);
}

static int is_blank_line(const char* buf) {
    return buf[0] == '\n';
}

static void output_char_visual(unsigned char c, int show_tabs, int show_nonprinting, FILE* out) {
    if (show_tabs && c == '\t') {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out, "^I");
        return;
    }

    if (show_nonprinting) {
        if (c == '\n' || c == '\t') {
            (void)fputc(c, out);
            return;
        }
        if (c < 32) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "^%c", c + 64);
            return;
        }
        if (c == ASCII_DEL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "^?");
            return;
        }
        if (c >= ASCII_128 && c <= ASCII_CP1252_END) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "M-^%c", (c - ASCII_128) + 64);
            return;
        }
        if (c == ASCII_255) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "M-^?");
            return;
        }
        if (c >= ASCII_160) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "M-%c", c - ASCII_128);
            return;
        }
    }

    (void)fputc(c, out);
}

static int process_file(FILE* fp, int* line_num, const CatOptions* opts, FILE* out) {
    char buf[1024];
    int has_newline = 1;
    int prev_blank = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        has_newline = (len > 0 && buf[len - 1] == '\n');
        int blank = is_blank_line(buf);

        if (opts->squeeze_blank && blank && prev_blank) {
            continue;
        }
        prev_blank = blank;

        int should_number = 0;
        if (opts->show_line_numbers) {
            should_number = 1;
        } else if (opts->show_nonempty_line_numbers) {
            should_number = !blank;
        }

        if (should_number) {
            format_line_number(*line_num, opts->number_format, out);
            (*line_num)++;
        }

        if (opts->show_nonprinting) {
            size_t content_len = has_newline ? len - 1 : len;
            for (size_t j = 0; j < content_len; j++) {
                output_char_visual((unsigned char)buf[j], opts->show_tabs, 1, out);
            }
            if (has_newline) {
                if (opts->show_ends) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(out, "$\n");
                } else {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(out, "\n");
                }
            }
        } else {
            int tab_processed = 0;
            if (opts->show_tabs) {
                for (size_t j = 0; j < len; j++) {
                    if (buf[j] == '\t') {
                        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                        (void)fprintf(out, "^I");
                    } else {
                        (void)fputc(buf[j], out);
                    }
                }
                tab_processed = 1;
            }

            if (opts->show_ends && has_newline && !tab_processed) {
                buf[len - 1] = '$';
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out, "%s\n", buf);
            } else if (!tab_processed) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out, "%s", buf);
            }
        }
    }
    return has_newline;
}

static char** expand_short_options(int* argc, char** argv) {
    int new_argc = 0;
    for (int i = 0; i < *argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '-' && argv[i][1] != '\0') {
            new_argc += (int)strlen(argv[i]) - 1;
        } else {
            new_argc += 1;
        }
    }

    if (new_argc == *argc) {
        return argv;
    }

    char** new_argv = (char**)malloc((size_t)new_argc * sizeof(char*));
    int j = 0;
    for (int i = 0; i < *argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '-' && argv[i][1] != '\0') {
            for (int k = 1; argv[i][k] != '\0'; k++) {
                char* opt = (char*)malloc(3);
                opt[0] = '-';
                opt[1] = argv[i][k];
                opt[2] = '\0';
                new_argv[j++] = opt;
            }
        } else {
            new_argv[j++] = argv[i];
        }
    }

    *argc = new_argc;
    return new_argv;
}

static void run_pipeline(const char* path, const CatOptions* opts, int* line_num, FILE* out) {
    int from_stdin = (path == NULL);

    if (opts->diff_file && !from_stdin) {
        if (opts->header_mode) { print_header(path, out); }
        run_diff(path, opts->diff_file, out);
        return;
    }

    // lines will be allocated below; ensure cleanup on error paths

    auto read_vec = from_stdin ? read_stdin_to_lines() : read_file_to_lines(path);
    std::vector<PipelineLine*>* lines = new std::vector<PipelineLine*>(std::move(read_vec));
    if (lines->empty()) {
        if (!from_stdin) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "cat: %s: No such file or directory\n", path);
        }
        return;
    }

    if (opts->header_mode && !from_stdin) {
        print_header(path, out);
    }

    BlameInfo* blame = NULL;
    int blame_count = 0;
    if (opts->blame_mode && !from_stdin) {
        blame = parse_blame(path, &blame_count);
    }

    std::vector<PipelineLine*>* cur = lines;

    if (opts->squeeze_blank) {
        std::vector<PipelineLine*>* filtered = squeeze_blank_lines(cur);
        free_pipeline_lines(cur);
        cur = filtered;
    }

    if (opts->range_start || opts->range_end) {
        std::vector<PipelineLine*>* filtered = slice_range(cur, opts->range_start, opts->range_end);
        free_pipeline_lines(cur);
        cur = filtered;
    }

    if (opts->head_lines) {
        std::vector<PipelineLine*>* filtered = slice_head(cur, opts->head_lines);
        free_pipeline_lines(cur);
        cur = filtered;
    }

    if (opts->tail_lines) {
        std::vector<PipelineLine*>* filtered = slice_tail(cur, opts->tail_lines);
        free_pipeline_lines(cur);
        cur = filtered;
    }

    std::vector<unsigned int>* match_indices = NULL;
    if (opts->grep_pattern) {
        match_indices = find_matching_indices(cur, opts->grep_pattern);
        if (opts->context_lines > 0) {
            std::vector<unsigned int>* expanded = expand_indices(cur, match_indices, opts->context_lines);
            delete match_indices;
            match_indices = expanded;
        }
        std::vector<PipelineLine*>* filtered = extract_lines(cur, match_indices);
        free_pipeline_lines(cur);
        cur = filtered;
    }

    const char* ext = NULL;
    if (opts->highlight_mode && !from_stdin) {
        ext = get_file_extension(path);
    }

    for (size_t i = 0; i < cur->size(); i++) {
        PipelineLine* pl = (*cur)[i];
        int blank = (pl->text[0] == '\n');
        int show_num = 0;

        if (opts->show_line_numbers) {
            show_num = 1;
        } else if (opts->show_nonempty_line_numbers) {
            show_num = !blank;
        }

        if (opts->blame_mode && blame && pl->orig_index < blame_count) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "%.7s  %-12.12s  %-10.10s  |  ",
                    blame[pl->orig_index].commit,
                    blame[pl->orig_index].author,
                    blame[pl->orig_index].date);
        }

        if (show_num) {
            format_line_number(*line_num, opts->number_format, out);
            (*line_num)++;
        }

        if (opts->highlight_mode && isatty(STDOUT_FILENO) && ext) {
            print_highlighted(pl->text.c_str(), ext, out);
        } else {
            output_line_visual(pl->text.c_str(), opts, out);
        }
    }

    if (opts->show_stats) {
        print_stats(cur, out);
    }

    delete match_indices;
    free_pipeline_lines(cur);
    free_blame(blame, blame_count);
    // Note: 'lines' is now owned by 'cur' and freed via free_pipeline_lines
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity, misc-use-internal-linkage)
void cat_command(int argc, char** argv) {
    CatOptions opts = {0};
    char* pager_buf = NULL;
    size_t pager_buf_size = 0;
    FILE* out_fp = stdout;

    int orig_argc = argc;
    char** my_argv = expand_short_options(&argc, argv);
    int expanded = (my_argv != argv);

    struct arg_lit* number_opt = arg_lit0("n", "number", "number all output lines");
    struct arg_lit* nonempty_number_opt = arg_lit0("b", "number-nonblank", "number nonempty output lines");
    struct arg_lit* show_ends_opt = arg_lit0("E", "show-ends", "display $ at end of each line");
    struct arg_lit* show_tabs_opt = arg_lit0("T", "show-tabs", "display TAB characters as ^I");
    struct arg_lit* squeeze_blank_opt = arg_lit0("s", "squeeze-blank", "never more than one single blank line");
    struct arg_lit* show_nonprinting_opt = arg_lit0("v", "show-nonprinting", "use ^ and M- notation, except for LFD and TAB");
    struct arg_lit* show_all_opt = arg_lit0("A", "show-all", "equivalent to -vET");
    struct arg_lit* less_opt = arg_lit0(NULL, "less", "pager mode (j/k/q navigation)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* show_nonprinting_and_ends_opt = arg_lit0("e", NULL, "equivalent to -vE");
    struct arg_lit* show_tabs_and_nonprinting_opt = arg_lit0("t", NULL, "equivalent to -vT");

    struct arg_lit* blame_opt = arg_lit0(NULL, "blame", "show git blame per line");
    struct arg_lit* highlight_opt = arg_lit0(NULL, "highlight", "syntax highlight output");
    struct arg_lit* header_opt = arg_lit0(NULL, "header", "show file metadata banner");
    struct arg_str* diff_opt = arg_str0(NULL, "diff", "FILE", "unified diff between file and FILE");
    struct arg_str* range_opt = arg_str0(NULL, "range", "N-M", "show only lines N through M");
    struct arg_str* grep_opt = arg_str0(NULL, "grep", "PATTERN", "keep lines matching extended regex");
    struct arg_int* context_opt = arg_int0(NULL, "context", "N", "show N context lines around --grep matches");
    struct arg_int* head_opt = arg_int0(NULL, "head", "N", "show first N lines only");
    struct arg_int* tail_opt = arg_int0(NULL, "tail", "N", "show last N lines only");
    struct arg_str* number_format_opt = arg_str0(NULL, "number-format", "FMT", "line number format: decimal, hex, octal");
    struct arg_lit* stats_opt = arg_lit0(NULL, "stats", "show line/word/char count");

    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file to read");
    struct arg_end* end = arg_end(ARG_END_SIZE);

    void* argtable[] = { number_opt, nonempty_number_opt, show_ends_opt, show_tabs_opt,
        squeeze_blank_opt, show_nonprinting_opt, show_all_opt,
        show_nonprinting_and_ends_opt, show_tabs_and_nonprinting_opt,
        less_opt, help_opt,
        blame_opt, highlight_opt, header_opt, diff_opt,
        range_opt, grep_opt, context_opt, head_opt, tail_opt,
        number_format_opt, stats_opt,
        file_arg, end };

    int nerrors = arg_parse(argc, my_argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Concatenate FILE(s) to standard output.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("Standard options:\n");
        printf("  -b, --number-nonblank    number nonempty output lines\n");
        printf("  -E, --show-ends          display $ at end of each line\n");
        printf("  -n, --number             number all output lines\n");
        printf("  -s, --squeeze-blank      never more than one single blank line\n");
        printf("  -T, --show-tabs          display TAB characters as ^I\n");
        printf("  -v, --show-nonprinting   use ^ and M- notation, except for LFD and TAB\n");
        printf("  -e                       equivalent to -vE\n");
        printf("  -t                       equivalent to -vT\n");
        printf("  -A, --show-all           equivalent to -vET\n");
        printf("      --less               pager mode (j/k/q navigation)\n");
        printf("\n");
        printf("Dev tools:\n");
        printf("      --blame              show git blame per line\n");
        printf("      --highlight          syntax highlight output by file extension\n");
        printf("      --header             show file metadata banner before content\n");
        printf("      --diff=FILE          unified diff between file and FILE\n");
        printf("\n");
        printf("Content navigation:\n");
        printf("      --range=N-M          show only lines N through M\n");
        printf("      --grep=PATTERN       keep lines matching extended regex\n");
        printf("      --context=N          show N context lines around --grep matches\n");
        printf("      --head=N             show first N lines only\n");
        printf("      --tail=N             show last N lines only\n");
        printf("      --number-format=FMT  line number format: decimal|hex|octal\n");
        printf("      --stats              show line/word/char count\n");
        printf("  -h, --help               display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        do_cleanup_expanded(my_argv, argv, orig_argc, argc, expanded);
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        do_cleanup_expanded(my_argv, argv, orig_argc, argc, expanded);
        return;
    }

    opts.show_line_numbers = (number_opt->count > 0);
    opts.show_nonempty_line_numbers = (nonempty_number_opt->count > 0);
    opts.show_ends = (show_ends_opt->count > 0);
    opts.show_tabs = (show_tabs_opt->count > 0);
    opts.squeeze_blank = (squeeze_blank_opt->count > 0);
    opts.show_nonprinting = (show_nonprinting_opt->count > 0);
    opts.less_mode = (less_opt->count > 0);

    opts.blame_mode = (blame_opt->count > 0);
    opts.highlight_mode = (highlight_opt->count > 0);
    opts.header_mode = (header_opt->count > 0);
    opts.diff_file = (char*)(diff_opt->count > 0 ? diff_opt->sval[0] : NULL);
    opts.grep_pattern = (char*)(grep_opt->count > 0 ? grep_opt->sval[0] : NULL);
    opts.context_lines = (context_opt->count > 0 ? context_opt->ival[0] : 0);
    opts.head_lines = (head_opt->count > 0 ? head_opt->ival[0] : 0);
    opts.tail_lines = (tail_opt->count > 0 ? tail_opt->ival[0] : 0);
    opts.show_stats = (stats_opt->count > 0);

    if (range_opt->count > 0) {
        int s = 0, e = 0;
        if (sscanf(range_opt->sval[0], "%d-%d", &s, &e) >= 1) {
            opts.range_start = s;
            opts.range_end = e;
        }
    }

    if (number_format_opt->count > 0) {
        const char* fmt = number_format_opt->sval[0];
        if (strcmp(fmt, "hex") == 0) opts.number_format = 1;
        else if (strcmp(fmt, "octal") == 0) opts.number_format = 2;
    }

    if (show_all_opt->count > 0) {
        opts.show_nonprinting = 1;
        opts.show_ends = 1;
        opts.show_tabs = 1;
    }

    if (show_nonprinting_and_ends_opt->count > 0) {
        opts.show_nonprinting = 1;
        opts.show_ends = 1;
    }

    if (show_tabs_and_nonprinting_opt->count > 0) {
        opts.show_nonprinting = 1;
        opts.show_tabs = 1;
    }

    if (opts.show_nonempty_line_numbers) {
        opts.show_line_numbers = 0;
    }

    int use_buffer = opts.blame_mode || opts.highlight_mode || opts.header_mode ||
                     opts.diff_file || opts.range_start || opts.range_end ||
                     opts.grep_pattern || opts.context_lines > 0 ||
                     opts.head_lines > 0 || opts.tail_lines > 0 ||
                     opts.show_stats || opts.number_format > 0;
    int line_num = 1;

    if (opts.less_mode && isatty(STDOUT_FILENO)) {
        out_fp = open_memstream(&pager_buf, &pager_buf_size);
        if (out_fp == NULL) {
            out_fp = stdout;
            opts.less_mode = 0;
        }
    }

    if (file_arg->count == 0) {
        if (use_buffer) {
            run_pipeline(NULL, &opts, &line_num, out_fp);
        } else {
            process_file(stdin, &line_num, &opts, out_fp);
        }
    } else {
        int prev_file_had_newline = 1;
        for (int i = 0; i < file_arg->count; i++) {
            if (use_buffer) {
                if (i > 0 && !prev_file_had_newline) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(out_fp, "\n");
                }
                run_pipeline(file_arg->filename[i], &opts, &line_num, out_fp);
                prev_file_had_newline = 1;
            } else {
                FILE* fp;
                if (strcmp(file_arg->filename[i], "-") == 0) {
                    fp = stdin;
                } else {
                    fp = fopen(file_arg->filename[i], "r");
                }
                if (fp == NULL) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "cat: %s: No such file or directory\n", file_arg->filename[i]);
                    prev_file_had_newline = 1;
                    continue;
                }
                if (i > 0 && !prev_file_had_newline) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(out_fp, "\n");
                }
                prev_file_had_newline = process_file(fp, &line_num, &opts, out_fp);
                // NOLINTNEXTLINE(bugprone-unused-return-value)
                (void)fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (out_fp != stdout) {
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)fclose(out_fp);
        if (pager_buf != NULL) {
            std::vector<std::string> pager_lines;
            char* saveptr;
            char* line = strtok_r(pager_buf, "\n", &saveptr);
            while (line) {
                pager_lines.push_back(line);
                line = strtok_r(nullptr, "\n", &saveptr);
            }
            if (!pager_lines.empty() && pager_lines.back().empty()) {
                pager_lines.pop_back();
            }
            pager_run(pager_lines);
            free(pager_buf);
        }
    }

    do_cleanup_expanded(my_argv, argv, orig_argc, argc, expanded);
}

REGISTER_COMMAND("cat", cat_command, "Concatenate files and print");
