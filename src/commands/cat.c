#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <argtable3.h>

static int process_file(FILE* fp, int* line_num, int show_line_numbers, int show_nonempty_line_numbers, int show_ends) {
    char buf[1024];
    int has_newline = 1; // 默认假设文件以换行符结尾
    while (fgets(buf, sizeof(buf), fp)) {
        has_newline = (buf[strlen(buf) - 1] == '\n');
        if (show_ends) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '$';
                printf("%s\n", buf);
            } else {
                printf("%s", buf);
            }
        } else if (show_line_numbers) {
            printf("%6d  %s", (*line_num)++, buf);
        } else if (show_nonempty_line_numbers) {
            if (buf[0] != '\n') {
                printf("%6d  %s", (*line_num)++, buf);
            } else {
                printf("%s", buf);
            }
        } else {
            printf("%s", buf);
        }
    }
    return has_newline;
}

void cat_command(gint argc, gchar** argv) {
    int show_line_numbers = 0;
    int show_nonempty_line_numbers = 0;
    int show_ends = 0;

    struct arg_lit* number_opt = arg_lit0("n", "number", "number all output lines");
    struct arg_lit* nonempty_number_opt = arg_lit0("b", "number-nonblank", "number nonempty output lines");
    struct arg_lit* show_ends_opt = arg_lit0("E", "show-ends", "display $ at end of each line");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "file to read");
    struct arg_end* end = arg_end(20);

    void* argtable[] = { number_opt, nonempty_number_opt, show_ends_opt, file_arg, end };

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    show_line_numbers = (number_opt->count > 0);
    show_nonempty_line_numbers = (nonempty_number_opt->count > 0);
    show_ends = (show_ends_opt->count > 0);

    // According to POSIX, -b overrides -n
    if (show_nonempty_line_numbers) {
        show_line_numbers = 0;
    }

    int line_num = 1;
    if (file_arg->count == 0) {
        process_file(stdin, &line_num, show_line_numbers, show_nonempty_line_numbers, show_ends);
    } else {
        int prev_file_had_newline = 1; // 默认假设第一个文件前有换行符
        for (int i = 0; i < file_arg->count; i++) {
            FILE* fp;
            if (strcmp(file_arg->filename[i], "-") == 0) {
                fp = stdin;
            } else {
                fp = fopen(file_arg->filename[i], "r");
            }
            if (fp == NULL) {
                printf("cat: %s: No such file or directory\n", file_arg->filename[i]);
                prev_file_had_newline = 1; // 错误信息后假设已有换行符
                continue;
            }
            // 如果前一个文件没有以换行符结尾，添加一个换行符
            if (i > 0 && !prev_file_had_newline) {
                printf("\n");
            }
            prev_file_had_newline = process_file(fp, &line_num, show_line_numbers, show_nonempty_line_numbers, show_ends);
            fclose(fp);
        }
    }
    
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}