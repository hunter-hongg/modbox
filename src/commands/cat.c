#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <argtable3.h>

/* Named constants for character ranges used by output_char_visual */
#define ASCII_DEL          127
#define ASCII_128          128
#define ASCII_CP1252_END   159
#define ASCII_160          160
#define ASCII_255          255

static int is_blank_line(const char* buf) {
    return buf[0] == '\n';
}

static void output_char_visual(unsigned char c, int show_tabs, int show_nonprinting) {
    if (show_tabs && c == '\t') {
        printf("^I");
        return;
    }

    if (show_nonprinting) {
        if (c == '\n' || c == '\t') {
            putchar(c);
            return;
        }
        if (c < 32) {
            printf("^%c", c + 64);
            return;
        }
        if (c == ASCII_DEL) {
            printf("^?");
            return;
        }
        if (c >= ASCII_128 && c <= ASCII_CP1252_END) {
            printf("M-^%c", (c - ASCII_128) + 64);
            return;
        }
        if (c == ASCII_255) {
            printf("M-^?");
            return;
        }
        if (c >= ASCII_160) {
            printf("M-%c", c - ASCII_128);
            return;
        }
    }

    putchar(c);
}

static int process_file(FILE* fp, int* line_num, int show_line_numbers, int show_nonempty_line_numbers, int show_ends, int squeeze_blank, int show_tabs, int show_nonprinting) {
    char buf[1024];
    int has_newline = 1;
    int prev_blank = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        has_newline = (len > 0 && buf[len - 1] == '\n');
        int blank = is_blank_line(buf);

        if (squeeze_blank && blank && prev_blank) {
            continue;
        }
        prev_blank = blank;

        int should_number = 0;
        if (show_line_numbers) {
            should_number = 1;
        } else if (show_nonempty_line_numbers) {
            should_number = !blank;
        }

        if (should_number) {
            printf("%6d  ", (*line_num)++);
        }

        if (show_nonprinting) {
            size_t content_len = has_newline ? len - 1 : len;
            for (size_t j = 0; j < content_len; j++) {
                output_char_visual((unsigned char)buf[j], show_tabs, 1);
            }
            if (has_newline) {
                if (show_ends) {
                    printf("$\n");
                } else {
                    printf("\n");
                }
            }
        } else {
            int tab_processed = 0;
            if (show_tabs) {
                for (size_t j = 0; j < len; j++) {
                    if (buf[j] == '\t') {
                        printf("^I");
                    } else {
                        putchar(buf[j]);
                    }
                }
                tab_processed = 1;
            }

            if (show_ends && has_newline && !tab_processed) {
                buf[len - 1] = '$';
                printf("%s\n", buf);
            } else if (!tab_processed) {
                printf("%s", buf);
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
    int show_line_numbers = 0;
    int show_nonempty_line_numbers = 0;
    int show_ends = 0;
    int squeeze_blank = 0;
    int show_tabs = 0;
    int show_nonprinting = 0;

    int orig_argc = argc;
    gchar** my_argv = expand_short_options(&argc, argv);
    int expanded = (my_argv != argv);

    struct arg_lit* number_opt = arg_lit0("n", "number", "number all output lines");
    struct arg_lit* nonempty_number_opt = arg_lit0("b", "number-nonblank", "number nonempty output lines");
    struct arg_lit* show_ends_opt = arg_lit0("E", "show-ends", "display $ at end of each line");
    struct arg_lit* show_tabs_opt = arg_lit0("T", "show-tabs", "display TAB characters as ^I");
    struct arg_lit* squeeze_blank_opt = arg_lit0("s", "squeeze-blank", "never more than one single blank line");
    struct arg_lit* show_nonprinting_opt = arg_lit0("v", "show-nonprinting", "use ^ and M- notation, except for LFD and TAB");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file to read");
    struct arg_end* end = arg_end(20);

    void* argtable[] = { number_opt, nonempty_number_opt, show_ends_opt, show_tabs_opt, squeeze_blank_opt, show_nonprinting_opt, file_arg, end };

    int nerrors = arg_parse(argc, my_argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        goto cleanup;
    }

    show_line_numbers = (number_opt->count > 0);
    show_nonempty_line_numbers = (nonempty_number_opt->count > 0);
    show_ends = (show_ends_opt->count > 0);
    show_tabs = (show_tabs_opt->count > 0);
    squeeze_blank = (squeeze_blank_opt->count > 0);
    show_nonprinting = (show_nonprinting_opt->count > 0);

    // According to POSIX, -b overrides -n
    if (show_nonempty_line_numbers) {
        show_line_numbers = 0;
    }

    int line_num = 1;
    if (file_arg->count == 0) {
        process_file(stdin, &line_num, show_line_numbers, show_nonempty_line_numbers, show_ends, squeeze_blank, show_tabs, show_nonprinting);
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
                printf("cat: %s: No such file or directory\n", file_arg->filename[i]);
                prev_file_had_newline = 1; // Assume newline present after error message
                continue;
            }
            // If previous file did not end with newline, add one
            if (i > 0 && !prev_file_had_newline) {
                printf("\n");
            }
            prev_file_had_newline = process_file(fp, &line_num, show_line_numbers, show_nonempty_line_numbers, show_ends, squeeze_blank, show_tabs, show_nonprinting);
            // NOLINTNEXTLINE(bugprone-unused-return-value)
            (void)fclose(fp);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

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
