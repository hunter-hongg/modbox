#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <string>
#include <vector>
#include <unistd.h>
#include <argtable3.h>

namespace {

// Read everything from the given fd into a freshly created temporary file and
// return its path. Returns an empty string on failure. The caller owns the
// file and must unlink() it.
std::string spill_fd_to_tempfile(int fd) {
    char tmpl[] = "/tmp/modbox-pr-XXXXXX";
    int const tmp_fd = mkstemp(tmpl);
    if (tmp_fd < 0) { return std::string(); }

    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t const w = write(tmp_fd, buf + written,
                                    static_cast<size_t>(n - written));
            if (w <= 0) { break; }
            written += w;
        }
    }
    close(tmp_fd);
    return std::string(tmpl);
}

}  // namespace

#include "commands/pr.hpp"
#include "commands/arg_util.hpp"
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

std::string get_date_string() {
    time_t const now = time(nullptr);
    const struct tm* tm_info = localtime(&now);
    char buf[64];
    (void)strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf);
}

int count_lines(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == nullptr) { return -1;
}

    int count = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        count++;
    }
    (void)fclose(fp);
    return count;
}

void print_page_header(const char* filename, int page_num, const PrOptions& opts) {
    std::string const header = opts.header_text.empty() ? filename : opts.header_text;
    std::string const date = get_date_string();
    printf("      %s          %s          Page %d\n\n",
           date.c_str(), header.c_str(), page_num);
}

void print_page_footer(int page_num) {
}

void paginate_file(const char* filename, PrOptions& opts) {
    int total_lines = count_lines(filename);
    if (total_lines < 0) {
        (void)fprintf(stderr, "pr: %s: %s\n", filename, strerror(errno));
        return;
    }

    int lines_per_page = opts.lines_per_page;
    if (opts.header != 0) {
        lines_per_page -= opts.header_count;
    }

    if (lines_per_page <= 0) { lines_per_page = 1;
}

    FILE* fp = fopen(filename, "r");
    if (fp == nullptr) {
        (void)fprintf(stderr, "pr: %s: %s\n", filename, strerror(errno));
        return;
    }

    std::vector<std::string> lines;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        std::string line(buf);
        // Store lines without the trailing newline so multi-column layout can
        // pad each field cleanly; a single '\n' is emitted per output row.
        while (!line.empty() &&
               (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        lines.push_back(line);
    }
    (void)fclose(fp);

    if (lines.empty()) {
        lines.push_back("");
        total_lines = 1;
    }

    int const col_width = opts.page_width / opts.columns;
    if (opts.multi_column != 0) {
        // Column-major fill. Columns are packed with ceil(N / cols) rows so
        // each column has roughly the same height, then chunked into pages of
        // (lines_per_page) rows.
        int const rows_for_content =
            (total_lines + opts.columns - 1) / opts.columns;
        int const rows = std::max(1, std::min(rows_for_content, lines_per_page));
        int const lines_per_page_total = rows * opts.columns;
        int page_num = 1;

        for (int start = 0; start < total_lines; start += lines_per_page_total) {
            if (opts.header != 0) {
                print_page_header(filename, page_num, opts);
            }

            for (int row = 0; row < rows; row++) {
                bool any = false;
                for (int col = 0; col < opts.columns; col++) {
                    int const idx = start + col * rows + row;
                    if (idx < static_cast<int>(lines.size())) {
                        any = true;
                        std::string line = lines[idx];
                        if (static_cast<int>(line.length()) > col_width) {
                            line = line.substr(0, col_width);
                        }
                        if (col + 1 < opts.columns) {
                            printf("%-*s", col_width, line.c_str());
                        } else {
                            printf("%s", line.c_str());
                        }
                    } else if (any && col + 1 < opts.columns) {
                        printf("%*s", col_width, "");
                    }
                }
                if (any) {
                    printf("\n");
                }
            }

            if (start + lines_per_page_total < total_lines) {
                printf("\f");
            }
            page_num++;
        }
    } else {
        int page_num = 1;
        int line_idx = 0;

        while (line_idx < static_cast<int>(lines.size())) {
            if (opts.header != 0) {
                print_page_header(filename, page_num, opts);
            }

            int const remaining = lines.size() - line_idx;
            int const page_lines = remaining < lines_per_page ? remaining : lines_per_page;

            for (int i = 0; i < page_lines; i++) {
                printf("%s\n", lines[line_idx + i].c_str());
                if (opts.double_space != 0) {
                    printf("\n");
                }
            }

            line_idx += page_lines;
            if (line_idx < static_cast<int>(lines.size())) {
                printf("\f");
            }
            page_num++;
        }
    }
}

}

int pr_command(int argc, char** argv) {
    // GNU pr accepts "-COLUMNS" (a bare number immediately after '-') and
    // "-COLUMNS" style. argtable3 cannot express that form, so extract it
    // before parsing and remember the requested column count.
    std::vector<char*> filtered;
    filtered.reserve(static_cast<size_t>(argc));
    filtered.push_back(argv[0]);
    int digit_columns = 0;
    bool digit_columns_set = false;
    for (int a = 1; a < argc; a++) {
        const char* arg = argv[a];
        if (arg[0] == '-' && arg[1] >= '0' && arg[1] <= '9') {
            char* endp = nullptr;
            long const n = std::strtol(arg + 1, &endp, 10);
            if (endp != nullptr && *endp == '\0' && n > 0) {
                digit_columns = static_cast<int>(std::min<long>(n, 1000));
                digit_columns_set = true;
                continue;
            }
        }
        filtered.push_back(argv[a]);
    }
    int const fargc = static_cast<int>(filtered.size());
    char** const fargv = filtered.data();

    struct arg_lit* header_opt = arg_lit0(NULL, "header", "page header (default)");
    struct arg_lit* no_header_opt = arg_lit0("t", "no-header", "suppress page headers");
    struct arg_str* col_num_opt = arg_strn(NULL, "columns", "<num>", 0, 1, "number of columns");
    struct arg_int* lines_opt = arg_int0("l", "length", "lines", "set lines per page");
    struct arg_int* width_opt = arg_int0("w", "width", "width", "set page width");
    struct arg_lit* no_fill_opt = arg_lit0(NULL, "no-fill", "no fill");
    struct arg_lit* across_opt = arg_lit0("a", "across", "fill columns across");
    struct arg_lit* first_only_opt = arg_lit0(NULL, "first-title-count", "first title only");
    struct arg_lit* double_opt = arg_lit0("d", "double-space", "double space output");
    struct arg_str* title_opt = arg_strn(NULL, "title", "<text>", 0, 1, "custom title");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input files");
    struct arg_end* end = arg_end(20);

    ArgTable at({header_opt, no_header_opt, col_num_opt,
                 lines_opt, width_opt, no_fill_opt, across_opt, first_only_opt,
                 double_opt, title_opt, help_opt, files_arg, end});

    int const nerrors = at.parse(fargc, fargv);

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
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    PrOptions opts;

    opts.header = static_cast<int>(no_header_opt->count == 0);
    opts.double_space = static_cast<int>(double_opt->count > 0);

    if (digit_columns_set) {
        opts.columns = digit_columns;
        opts.multi_column = 1;
    }

    if (col_num_opt->count > 0) {
        opts.columns = atoi(col_num_opt->sval[0]);
        opts.columns = std::max(opts.columns, 1);
        opts.columns = std::min(opts.columns, 1000);
        if (opts.columns > 1) {
            opts.multi_column = 1;
        }
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
        std::string const tmpfile = spill_fd_to_tempfile(STDIN_FILENO);
        if (tmpfile.empty()) {
            (void)fprintf(stderr, "pr: cannot create temp file\n");
            return 1;
        }
        paginate_file(tmpfile.c_str(), opts);
        (void)unlink(tmpfile.c_str());
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            const char* filename = files_arg->filename[i];
            if (strcmp(filename, "-") == 0) {
                std::string const tmpfile = spill_fd_to_tempfile(STDIN_FILENO);
                if (tmpfile.empty()) {
                    (void)fprintf(stderr, "pr: cannot create temp file\n");
                    continue;
                }
                paginate_file(tmpfile.c_str(), opts);
                (void)unlink(tmpfile.c_str());
            } else {
                paginate_file(filename, opts);
            }
            if ((opts.header != 0) && i < files_arg->count - 1) {
                printf("\n");
            }
        }
    }

    return 0;
}

REGISTER_COMMAND("pr", pr_command, "Paginate or columnate files for printing");
