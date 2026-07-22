#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>
#include "commands/unexpand.hpp"
#include <argtable3.h>
#include "commands/command_macros.hpp"

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

static bool at_tab_stop(int col, const std::vector<int>& stops, bool interval_mode)
{
    if (col <= 0) return false;
    if (interval_mode) return (col % stops[0] == 0);
    for (int s : stops)
        if (s == col) return true;
    return false;
}

static void convert_spaces(int* pos, int* col, int nspaces,
                           const std::vector<int>& stops, bool interval_mode)
{
    int p = *pos;
    int s = 0;
    while (s < nspaces)
    {
        int next = next_tab_col(p, stops, interval_mode);
        int need = next - p;

        if (s + need <= nspaces && at_tab_stop(next, stops, interval_mode))
        {
            putchar('\t');
            *col = next;
            p = next;
            s += need;
        }
        else
        {
            int remain = nspaces - s;
            int fill = remain;
            int aligned = next_tab_col(p, stops, interval_mode);
            if (fill >= aligned - p)
                fill = aligned - p;
            for (int k = 0; k < fill; k++)
                putchar(' ');
            *col += fill;
            p += fill;
            s += fill;
        }
    }
    *pos = p;
}

static void unexpand_file(FILE* fp, bool all, const std::vector<int>& stops, bool interval_mode)
{
    char* buf = nullptr;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&buf, &cap, fp)) != -1)
    {
        int col = 0;
        size_t i = 0;

        if (!all)
        {
            while (i < (size_t)n)
            {
                unsigned char ch = (unsigned char)buf[i];
                if (ch == ' ')
                {
                    int start_col = col;
                    int scount = 0;
                    while (i < (size_t)n && (unsigned char)buf[i] == ' ')
                    {
                        i++;
                        scount++;
                    }
                    convert_spaces(&start_col, &col, scount, stops, interval_mode);
                }
                else if (ch == '\t')
                {
                    putchar('\t');
                    col = next_tab_col(col, stops, interval_mode);
                    i++;
                }
                else if (ch == '\n')
                {
                    putchar('\n');
                    col = 0;
                    i++;
                }
                else
                {
                    putchar(ch);
                    col++;
                    i++;
                    while (i < (size_t)n)
                    {
                        unsigned char c2 = (unsigned char)buf[i];
                        putchar(c2);
                        if (c2 == '\n') { col = 0; }
                        else { col++; }
                        i++;
                    }
                    break;
                }
            }
        }
        else
        {
            while (i < (size_t)n)
            {
                unsigned char ch = (unsigned char)buf[i];
                if (ch == ' ')
                {
                    int start_col = col;
                    int scount = 0;
                    while (i < (size_t)n && (unsigned char)buf[i] == ' ')
                    {
                        i++;
                        scount++;
                    }
                    convert_spaces(&start_col, &col, scount, stops, interval_mode);
                }
                else if (ch == '\t')
                {
                    putchar('\t');
                    col = next_tab_col(col, stops, interval_mode);
                    i++;
                }
                else if (ch == '\n')
                {
                    putchar('\n');
                    col = 0;
                    i++;
                }
                else
                {
                    putchar(ch);
                    col++;
                    i++;
                }
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
        interval_mode = true;
    else
        for (size_t i = 1; i < stops.size(); i++)
            if (stops[i] <= stops[i - 1])
                return false;

    return true;
}

void unexpand_command(int argc, char** argv)
{
    struct arg_lit* all_opt = arg_lit0("a", "all", "convert all spaces to tabs, not just leading");
    struct arg_lit* first_only_opt = arg_lit0(NULL, "first-only", "convert only leading sequences of spaces (default)");
    struct arg_str* tabs_opt = arg_str0("t", "tabs", "N", "tab stops (number or comma-separated list, implies -a)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {all_opt, first_only_opt, tabs_opt, help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0)
    {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Convert spaces to tabs in each FILE, writing to standard output.\n");
        printf("\n");
        printf("  -a, --all         convert all spaces to tabs, not just leading\n");
        printf("      --first-only  convert only leading sequences of spaces (default)\n");
        printf("  -t, --tabs=N      tab stops (single number or comma-separated list, implies -a)\n");
        printf("  -h, --help        display this help and exit\n");
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

    bool all = (all_opt->count > 0);

    if (tabs_opt->count > 0)
        all = true;

    if (first_only_opt->count > 0)
        all = false;

    std::vector<int> tab_stops;
    bool interval_mode = true;
    tab_stops.push_back(8);

    if (tabs_opt->count > 0)
    {
        if (!parse_tab_stops(tabs_opt->sval[0], tab_stops, interval_mode))
        {
            fprintf(stderr, "unexpand: invalid tab stop list: %s\n", tabs_opt->sval[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (file_arg->count == 0)
    {
        unexpand_file(stdin, all, tab_stops, interval_mode);
    }
    else
    {
        for (int i = 0; i < file_arg->count; i++)
        {
            const char* fname = file_arg->filename[i];
            if (strcmp(fname, "-") == 0)
            {
                unexpand_file(stdin, all, tab_stops, interval_mode);
            }
            else
            {
                FILE* fp = fopen(fname, "r");
                if (!fp)
                {
                    fprintf(stderr, "unexpand: %s: No such file or directory\n", fname);
                    continue;
                }
                unexpand_file(fp, all, tab_stops, interval_mode);
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("unexpand", unexpand_command, "Convert spaces to tabs");
