#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <cerrno>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <argtable3.h>

#include "commands/pr.hpp"
#include "commands/command_macros.hpp"

namespace {

struct PrOptions {
    int header = 1;
    int multi_column = 0;
    int columns = 1;
    int lines_per_page = 66;
    int page_width = 72;
    int no_fill = 0;
    int first_title_only = 0;
    int header_count = 5;
    int double_space = 0;
    std::string header_text;
    int sep_char = '\t';
};

static std::string get_date_string() {
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf);
}

static int count_lines(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return -1;

    int count = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        count++;
    }
    fclose(fp);
    return count;
}

static void print_page_header(const char* filename, int page_num, const PrOptions& opts) {
    std::string header = opts.header_text.empty() ? filename : opts.header_text;
    std::string date = get_date_string();
    printf("      %s          %s          Page %d\n\n",
           date.c_str(), header.c_str(), page_num);
}

static void print_page_footer(int page_num) {
}

static void paginate_file(const char* filename, PrOptions& opts) {
    int total_lines = count_lines(filename);
    if (total_lines < 0) {
        fprintf(stderr, "pr: %s: %s\n", filename, strerror(errno));
        return;
    }

    int lines_per_page = opts.lines_per_page;
    if (opts.header) {
        lines_per_page -= opts.header_count;
    }

    if (lines_per_page <= 0) lines_per_page = 1;

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "pr: %s: %s\n", filename, strerror(errno));
        return;
    }

    std::vector<std::string> lines;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        if (!lines.empty() && lines.back().empty()) {
        }
        lines.push_back(buf);
    }
    fclose(fp);

    if (lines.empty()) {
        lines.push_back("");
        total_lines = 1;
    }

    int col_width = opts.page_width / opts.columns;
    if (opts.multi_column) {
        int rows = (total_lines + opts.columns - 1) / opts.columns;
        int page_num = 1;

        for (int start = 0; start < total_lines; start += rows) {
            if (opts.header) {
                print_page_header(filename, page_num, opts);
            }

            for (int row = 0; row < rows; row++) {
                for (int col = 0; col < opts.columns; col++) {
                    int idx = start + col * rows + row;
                    if (idx < (int)lines.size()) {
                        std::string line = lines[idx];
                        if ((int)line.length() > col_width) {
                            line = line.substr(0, col_width);
                        }
                        printf("%-*s", col_width, line.c_str());
                    } else {
                        printf("%*s", col_width, "");
                    }
                }
                printf("\n");
            }

            if (opts.header && start + rows < total_lines) {
                printf("\n");
            }
            page_num++;
        }
    } else {
        int page_num = 1;
        int line_idx = 0;

        while (line_idx < (int)lines.size()) {
            if (opts.header) {
                print_page_header(filename, page_num, opts);
            }

            int remaining = lines.size() - line_idx;
            int page_lines = remaining < lines_per_page ? remaining : lines_per_page;

            for (int i = 0; i < page_lines; i++) {
                printf("%s", lines[line_idx + i].c_str());
                if (opts.double_space) {
                    printf("\n");
                }
            }

            line_idx += page_lines;
            if (line_idx < (int)lines.size()) {
                printf("\f");
            }
            page_num++;
        }
    }
}

}

void pr_command(int argc, char** argv) {
    struct arg_lit* header_opt = arg_lit0(NULL, "header", "page header (default)");
    struct arg_lit* no_header_opt = arg_lit0(NULL, "no-header", "suppress page headers");
    struct arg_lit* multi_col_opt = arg_lit0(NULL, "columns", "multi-column output");
    struct arg_str* col_num_opt = arg_strn(NULL, "columns", "<num>", 0, 1, "number of columns");
    struct arg_int* lines_opt = arg_int0("l", "length", "lines", "set lines per page");
    struct arg_int* width_opt = arg_int0("w", "width", "width", "set page width");
    struct arg_lit* no_fill_opt = arg_lit0(NULL, "no-fill", "no fill");
    struct arg_lit* first_only_opt = arg_lit0(NULL, "first-title-count", "first title only");
    struct arg_lit* double_opt = arg_lit0(NULL, "double-space", "double space output");
    struct arg_str* title_opt = arg_strn(NULL, "title", "<text>", 0, 1, "custom title");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input files");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {header_opt, no_header_opt, multi_col_opt, col_num_opt,
                        lines_opt, width_opt, no_fill_opt, first_only_opt,
                        double_opt, title_opt, help_opt, files_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Paginate or columnate files for printing.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -COLUMNS, --columns=COLUMNS   number of columns\n");
        printf("  -a, --across          fill columns across\n");
        printf("  -d, --double-space    double space output\n");
        printf("  -h, --header          with page header\n");
        printf("  -l LINES, --length=LINES  lines per page (default 66)\n");
        printf("  -t, --no-header       suppress headers\n");
        printf("  -w WIDTH, --width=WIDTH  page width (default 72)\n");
        printf("\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    PrOptions opts;

    opts.header = (no_header_opt->count == 0);
    opts.multi_column = (multi_col_opt->count > 0);
    opts.double_space = (double_opt->count > 0);

    if (col_num_opt->count > 0) {
        opts.columns = atoi(col_num_opt->sval[0]);
        if (opts.columns < 1) opts.columns = 1;
        if (opts.columns > 1000) opts.columns = 1000;
    }

    if (lines_opt->count > 0) {
        opts.lines_per_page = lines_opt->ival[0];
    }

    if (width_opt->count > 0) {
        opts.page_width = width_opt->ival[0];
    }

    if (title_opt->count > 0) {
        opts.header_text = title_opt->sval[0];
    }

    if (files_arg->count == 0) {
        const char* tmpfile = "/tmp/pr_input_XXXXXX";
        int fd = mkstemp(const_cast<char*>(tmpfile));
        if (fd < 0) {
            fprintf(stderr, "pr: cannot create temp file\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        char buf[4096];
        ssize_t n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            write(fd, buf, n);
        }
        close(fd);

        paginate_file(tmpfile, opts);
        unlink(tmpfile);
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            const char* filename = files_arg->filename[i];
            if (strcmp(filename, "-") == 0) {
                const char* tmpfile = "/tmp/pr_input_XXXXXX";
                int fd = mkstemp(const_cast<char*>(tmpfile));
                if (fd < 0) {
                    fprintf(stderr, "pr: cannot create temp file\n");
                    continue;
                }
                char buf[4096];
                ssize_t n;
                while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                    write(fd, buf, n);
                }
                close(fd);
                paginate_file(tmpfile, opts);
                unlink(tmpfile);
            } else {
                paginate_file(filename, opts);
            }
            if (opts.header && i < files_arg->count - 1) {
                printf("\n");
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("pr", pr_command, "Paginate or columnate files for printing");
