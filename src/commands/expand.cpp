#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>
#include "commands/expand.hpp"
#include <argtable3.h>

static int next_tab_col(int col, const std::vector<int>& stops, bool interval_mode)
{
    if (interval_mode)
    {
        int n = stops[0];
        return (col / n + 1) * n;
    }
    for (int s : stops)
        if (s > col) return s;
    return col + 1;
}

static void expand_file(FILE* fp, bool initial_only, const std::vector<int>& stops, bool interval_mode)
{
    char* buf = nullptr;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&buf, &cap, fp)) != -1)
    {
        size_t col = 0;
        bool leading = true;

        for (ssize_t i = 0; i < n; i++)
        {
            unsigned char ch = (unsigned char)buf[i];

            if (ch == '\t')
            {
                if (initial_only && !leading)
                {
                    putchar('\t');
                    int next = next_tab_col((int)col, stops, interval_mode);
                    col = next;
                }
                else
                {
                    int next = next_tab_col((int)col, stops, interval_mode);
                    for (int s = (int)col; s < next; s++)
                        putchar(' ');
                    col = next;
                }
            }
            else if (ch == '\n')
            {
                putchar('\n');
                col = 0;
                leading = true;
            }
            else
            {
                putchar(ch);
                col++;
                if (ch != ' ' && ch != '\t')
                    leading = false;
            }
        }
        if (n > 0 && buf[n - 1] != '\n')
            putchar('\n');
    }

    free(buf);
}

static bool parse_tab_stops(const char* s, std::vector<int>& stops, bool& interval_mode)
{
    stops.clear();
    interval_mode = false;

    const char* p = s;
    while (*p)
    {
        if (!isdigit((unsigned char)*p))
            return false;
        long v = strtol(p, (char**)&p, 10);
        if (v < 1 || v > 10000)
            return false;
        stops.push_back((int)v);
        if (*p == ',')
            p++;
        else if (*p != '\0')
            return false;
    }

    if (stops.empty())
        return false;

    if (stops.size() == 1)
    {
        interval_mode = true;
    }
    else
    {
        for (size_t i = 1; i < stops.size(); i++)
            if (stops[i] <= stops[i - 1])
                return false;
    }

    return true;
}

void expand_command(int argc, char** argv)
{
    struct arg_lit* initial_opt = arg_lit0("i", "initial", "do not convert tabs after non-blanks");
    struct arg_str* tabs_opt = arg_str0("t", "tabs", "N", "tab stops (number or comma-separated list)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {initial_opt, tabs_opt, help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0)
    {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Convert tabs to spaces in each FILE, writing to standard output.\n");
        printf("\n");
        printf("  -i, --initial   do not convert tabs after non-blanks\n");
        printf("  -t, --tabs=N    tab stops (single number or comma-separated list, default 8)\n");
        printf("  -h, --help      display this help and exit\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0)
    {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool initial_only = (initial_opt->count > 0);

    std::vector<int> tab_stops;
    bool interval_mode = true;
    tab_stops.push_back(8);

    if (tabs_opt->count > 0)
    {
        if (!parse_tab_stops(tabs_opt->sval[0], tab_stops, interval_mode))
        {
            fprintf(stderr, "expand: invalid tab stop list: %s\n", tabs_opt->sval[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (file_arg->count == 0)
    {
        expand_file(stdin, initial_only, tab_stops, interval_mode);
    }
    else
    {
        for (int i = 0; i < file_arg->count; i++)
        {
            const char* fname = file_arg->filename[i];
            if (strcmp(fname, "-") == 0)
            {
                expand_file(stdin, initial_only, tab_stops, interval_mode);
            }
            else
            {
                FILE* fp = fopen(fname, "r");
                if (!fp)
                {
                    fprintf(stderr, "expand: %s: No such file or directory\n", fname);
                    continue;
                }
                expand_file(fp, initial_only, tab_stops, interval_mode);
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
