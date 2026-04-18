#include <stdio.h>
#include <glib.h>
#include <argtable3.h>

#include "commands/cat.h"

void cat_command(gint argc, gchar** argv) {
    int show_line_numbers = 0;
    int show_nonempty_line_numbers = 0;
    
    struct arg_lit* number_opt = arg_lit0("n", "number", "number all output lines");
    struct arg_lit* nonempty_number_opt = arg_lit0("b", "number-nonblank", "number nonempty output lines");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file to read");
    struct arg_end* end = arg_end(20);

    void* argtable[] = { number_opt, nonempty_number_opt, file_arg, end };

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    show_line_numbers = (number_opt->count > 0);
    show_nonempty_line_numbers = (nonempty_number_opt->count > 0);
    
    // According to POSIX, -b overrides -n
    if (show_nonempty_line_numbers) {
        show_line_numbers = 0;
    }

    int line_num = 1;
    FILE* fp;
    if (file_arg->count == 0) {
        fp = stdin;
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) {
            if (show_line_numbers) {
                printf("%6d  %s", line_num++, buf);
            } else if (show_nonempty_line_numbers) {
                if (buf[0] != '\n') {
                    printf("%6d  %s", line_num++, buf);
                } else {
                    printf("%s", buf);
                }
            } else {
                printf("%s", buf);
            }
        }
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            if (strcmp(file_arg->filename[i], "-") == 0) {
                fp = stdin;
            } else {
                fp = fopen(file_arg->filename[i], "r");
            }
            if (fp == NULL) {
                printf("cat: %s: No such file or directory\n", file_arg->filename[i]);
                continue;
            }
            char buf[1024];
            while (fgets(buf, sizeof(buf), fp)) {
                if (show_line_numbers) {
                    printf("%6d  %s", line_num++, buf);
                } else if (show_nonempty_line_numbers) {
                    if (buf[0] != '\n') {
                        printf("%6d  %s", line_num++, buf);
                    } else {
                        printf("%s", buf);
                    }
                } else {
                    printf("%s", buf);
                }
            }
            fclose(fp);
        }
    }
    
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}