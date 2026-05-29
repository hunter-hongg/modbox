#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <argtable3.h>

#include "commands/cat.h"
#include "commands/pager.h"

/* Named constants for character ranges used by output_char_visual */
#define ASCII_DEL          127
#define ASCII_128          128
#define ASCII_CP1252_END   159
#define ASCII_160          160
#define ASCII_255          255

static int is_blank_line(const char* buf) {
    return buf[0] == '\n';
}

static void output_char_visual(unsigned char c, int show_tabs, int show_nonprinting, FILE* out) {
    if (show_tabs && c == '\t') {
        fprintf(out, "^I");
        return;
    }

    if (show_nonprinting) {
        if (c == '\n' || c == '\t') {
            fputc(c, out);
            return;
        }
        if (c < 32) {
            fprintf(out, "^%c", c + 64);
            return;
        }
        if (c == ASCII_DEL) {
            fprintf(out, "^?");
            return;
        }
        if (c >= ASCII_128 && c <= ASCII_CP1252_END) {
            fprintf(out, "M-^%c", (c - ASCII_128) + 64);
            return;
        }
        if (c == ASCII_255) {
            fprintf(out, "M-^?");
            return;
        }
        if (c >= ASCII_160) {
            fprintf(out, "M-%c", c - ASCII_128);
            return;
        }
    }

    fputc(c, out);
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
            fprintf(out, "%6d  ", (*line_num)++);
        }

        if (opts->show_nonprinting) {
            size_t content_len = has_newline ? len - 1 : len;
            for (size_t j = 0; j < content_len; j++) {
                output_char_visual((unsigned char)buf[j], opts->show_tabs, 1, out);
            }
            if (has_newline) {
                if (opts->show_ends) {
                    fprintf(out, "$\n");
                } else {
                    fprintf(out, "\n");
                }
            }
        } else {
            int tab_processed = 0;
            if (opts->show_tabs) {
                for (size_t j = 0; j < len; j++) {
                    if (buf[j] == '\t') {
                        fprintf(out, "^I");
                    } else {
                        fputc(buf[j], out);
                    }
                }
                tab_processed = 1;
            }

            if (opts->show_ends && has_newline && !tab_processed) {
                buf[len - 1] = '$';
                fprintf(out, "%s\n", buf);
            } else if (!tab_processed) {
                fprintf(out, "%s", buf);
            }
        }
    }
    return has_newline;
}

static gchar** expand_short_options(int* argc, gchar** argv) {
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

    gchar** new_argv = (gchar**)g_malloc(new_argc * sizeof(gchar*));
    int j = 0;
    for (int i = 0; i < *argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '-' && argv[i][1] != '\0') {
            for (int k = 1; argv[i][k] != '\0'; k++) {
                gchar* opt = g_malloc(3);
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

// NOLINTNEXTLINE(misc-use-internal-linkage)
void cat_command(gint argc, gchar** argv) {
    CatOptions opts = {0};
    char* pager_buf = NULL;
    size_t pager_buf_size = 0;
    FILE* out_fp = stdout;

    int orig_argc = argc;
    gchar** my_argv = expand_short_options(&argc, argv);
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
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file to read");
    struct arg_end* end = arg_end(20);

    void* argtable[] = { number_opt, nonempty_number_opt, show_ends_opt, show_tabs_opt, squeeze_blank_opt, show_nonprinting_opt, show_all_opt, show_nonprinting_and_ends_opt, show_tabs_and_nonprinting_opt, less_opt, help_opt, file_arg, end };

    int nerrors = arg_parse(argc, my_argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Concatenate FILE(s) to standard output.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
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
        printf("  -h, --help               display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        goto cleanup;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        goto cleanup;
    }

    opts.show_line_numbers = (number_opt->count > 0);
    opts.show_nonempty_line_numbers = (nonempty_number_opt->count > 0);
    opts.show_ends = (show_ends_opt->count > 0);
    opts.show_tabs = (show_tabs_opt->count > 0);
    opts.squeeze_blank = (squeeze_blank_opt->count > 0);
    opts.show_nonprinting = (show_nonprinting_opt->count > 0);
    opts.less_mode = (less_opt->count > 0);

    // -A (--show-all) is equivalent to -vET
    if (show_all_opt->count > 0) {
        opts.show_nonprinting = 1;
        opts.show_ends = 1;
        opts.show_tabs = 1;
    }

    // -e is equivalent to -vE
    if (show_nonprinting_and_ends_opt->count > 0) {
        opts.show_nonprinting = 1;
        opts.show_ends = 1;
    }

    // -t is equivalent to -vT
    if (show_tabs_and_nonprinting_opt->count > 0) {
        opts.show_nonprinting = 1;
        opts.show_tabs = 1;
    }

    if (opts.show_nonempty_line_numbers) {
        opts.show_line_numbers = 0;
    }

    if (opts.less_mode && isatty(STDOUT_FILENO)) {
        out_fp = open_memstream(&pager_buf, &pager_buf_size);
        if (out_fp == NULL) {
            out_fp = stdout;
            opts.less_mode = 0;
        }
    }

    int line_num = 1;
    if (file_arg->count == 0) {
        process_file(stdin, &line_num, &opts, out_fp);
    } else {
        int prev_file_had_newline = 1; // Assume newline present before first file
        for (int i = 0; i < file_arg->count; i++) {
            FILE* fp;
            if (strcmp(file_arg->filename[i], "-") == 0) {
                fp = stdin;
            } else {
                fp = fopen(file_arg->filename[i], "r");
            }
            if (fp == NULL) {
                fprintf(stderr, "cat: %s: No such file or directory\n", file_arg->filename[i]);
                prev_file_had_newline = 1; // Assume newline present after error message
                continue;
            }
            if (i > 0 && !prev_file_had_newline) {
                fprintf(out_fp, "\n");
            }
            prev_file_had_newline = process_file(fp, &line_num, &opts, out_fp);
            // NOLINTNEXTLINE(bugprone-unused-return-value)
            (void)fclose(fp);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (out_fp != stdout) {
        fclose(out_fp);
        if (pager_buf != NULL) {
            GPtrArray* lines = g_ptr_array_new();
            gchar** parts = g_strsplit(pager_buf, "\n", -1);
            for (int i = 0; parts[i] != NULL; i++) {
                g_ptr_array_add(lines, g_strdup(parts[i]));
            }
            g_strfreev(parts);
            if (lines->len > 0 && strcmp((char*)g_ptr_array_index(lines, lines->len - 1), "") == 0) {
                g_ptr_array_remove_index(lines, lines->len - 1);
            }
            pager_run(lines);
            g_ptr_array_free(lines, 1);
            free(pager_buf);
        }
    }

cleanup:
    if (expanded) {
        for (int i = 0; i < argc; i++) {
            int from_original = 0;
            for (int j = 0; j < orig_argc; j++) {
                if (my_argv[i] == argv[j]) {
                    from_original = 1;
                    break;
                }
            }
            if (!from_original) {
                g_free(my_argv[i]);
            }
        }
        g_free((gpointer)my_argv);
    }
}
