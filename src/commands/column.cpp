#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/column.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

struct ColumnOptions {
    int width = 80;
    bool has_sep = false;
    char sep_char = 0;
    bool table = false;
    std::string output_file;
    bool has_output_file = false;
    std::string col_names;
    bool has_col_names = false;
    bool right_all = false;
    bool rightmost = false;
    bool divider = false;
    bool has_indent = false;
    std::string indent;
    std::string entry;
    bool has_entry = false;
    int lines_per_record = 1;
    bool print_head = false;
};

// Width of a string for alignment purposes: the length minus any trailing
// spaces/tabs, so that trailing whitespace does not distort measured widths.
size_t display_width(const std::string& s) {
    size_t len = s.size();
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        --len;
    }
    return len;
}

// Trim trailing spaces/tabs from a string (for alignment output).
std::string rstrip(const std::string& s) {
    return s.substr(0, display_width(s));
}

// How to split a line into fields.
enum class SepMode {
    Whitespace,  // runs of whitespace (spaces/tabs) are separators
    Char,        // every occurrence of a single character is a separator
    String,      // every occurrence of a string is a separator
};

// Split `line` on every occurrence of the string `sep`. An empty `sep`
// returns the whole line as a single field.
std::vector<std::string> split_on(const std::string& line, const std::string& sep) {
    std::vector<std::string> fields;
    if (sep.empty()) {
        fields.push_back(line);
        return fields;
    }
    size_t start = 0;
    for (;;) {
        size_t const pos = line.find(sep, start);
        if (pos == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, pos - start));
        start = pos + sep.size();
    }
    return fields;
}

// Split a line into fields according to the selected mode.
std::vector<std::string> split_fields(const std::string& line, SepMode mode,
                                      char sep_char, const std::string& sep_str) {
    if (mode == SepMode::Char) {
        return split_on(line, std::string(1, sep_char));
    }
    if (mode == SepMode::String) {
        return split_on(line, sep_str);
    }
    // Whitespace: runs of spaces/tabs are separators.
    std::vector<std::string> fields;
    size_t i = 0;
    const size_t n = line.size();
    while (i < n) {
        while (i < n && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        size_t const start = i;
        while (i < n && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
        fields.push_back(line.substr(start, i - start));
    }
    if (fields.empty()) {
        fields.push_back("");
    }
    return fields;
}

// Extract a single separator character from a user-supplied string, handling a
// small set of two-character escape sequences (like cut's -d).
char parse_sep_char(const std::string& s) {
    if (s.empty()) {
        return ' ';
    }
    if (s.size() >= 2 && s[0] == '\\') {
        switch (s[1]) {
            case 't':
                return '\t';
            case 'n':
                return '\n';
            case '0':
                return '\0';
            case '\\':
                return '\\';
            default:
                break;
        }
    }
    return s[0];
}

// Read every line from a stream (a trailing newline does not create an extra
// empty line; a final partial line without a newline is included).
std::vector<std::string> read_all_lines(FILE* fp) {
    std::vector<std::string> lines;
    std::string line;
    int ch;
    bool any = false;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            lines.push_back(line);
            line.clear();
            any = false;
        } else {
            line.push_back(static_cast<char>(ch));
            any = true;
        }
    }
    if (any || !line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

// A cell is the text for one column within a record; it may span several
// physical lines (when -l groups lines into a single record). Indexed as
// [cell][physical_line].
using Cell = std::vector<std::string>;
using Record = std::vector<Cell>;

// Build records: group `lines` into blocks of `lines_per_record`, split each
// physical line into fields, and stack the fields of the same column index into
// a (possibly multi-line) cell.
std::vector<Record> build_records(const std::vector<std::string>& lines,
                                  int lines_per_record, SepMode mode,
                                  char sep_char, const std::string& sep_str) {
    int const npr = (lines_per_record > 0) ? lines_per_record : 1;
    std::vector<Record> records;
    for (size_t base = 0; base < lines.size(); base += static_cast<size_t>(npr)) {
        std::vector<std::vector<std::string>> phys_fields;
        for (int j = 0; j < npr && base + static_cast<size_t>(j) < lines.size(); ++j) {
            phys_fields.push_back(
                split_fields(lines[base + static_cast<size_t>(j)], mode, sep_char,
                             sep_str));
        }
        size_t num_cells = 0;
        for (auto& pf : phys_fields) {
            num_cells = std::max(num_cells, pf.size());
        }
        Record rec;
        rec.reserve(num_cells);
        for (size_t c = 0; c < num_cells; ++c) {
            Cell cell;
            for (auto& pf : phys_fields) {
                cell.push_back(c < pf.size() ? pf[c] : "");
            }
            rec.push_back(std::move(cell));
        }
        records.push_back(std::move(rec));
    }
    return records;
}

size_t cell_width(const Cell& cell) {
    size_t w = 0;
    for (auto& l : cell) {
        w = std::max(w, display_width(l));
    }
    return w;
}

// Override a record's cell values with comma-separated column names. A name
// equal to "-" keeps the original value for that column.
void apply_column_names(Record& rec, const std::string& names) {
    std::vector<std::string> const parts = split_on(names, ",");
    for (size_t c = 0; c < rec.size() && c < parts.size(); ++c) {
        if (parts[c] != "-") {
            rec[c].clear();
            rec[c].push_back(parts[c]);
        }
    }
}

// Align a single physical line of a cell to the given width.
std::string align_text(const std::string& text, size_t width, bool right) {
    std::string const t = rstrip(text);
    size_t const w = display_width(t);
    if (w >= width) {
        return t;
    }
    size_t const pad = width - w;
    if (right) {
        return std::string(pad, ' ') + t;
    }
    return t + std::string(pad, ' ');
}

// ---------------------------------------------------------------------------
// Fill mode (default): distribute lines into as many columns as fit in the
// width, filling down each column before moving to the next.
// ---------------------------------------------------------------------------
void emit_fill(const std::vector<std::string>& lines, int width,
               const std::string& indent, bool has_indent, FILE* out) {
    if (lines.empty()) {
        return;
    }
    size_t max_width = 0;
    for (auto& l : lines) {
        max_width = std::max(max_width, display_width(l));
    }
    const size_t num = lines.size();
    size_t ncols = (static_cast<size_t>(width) + 1) / (max_width + 1);
    if (ncols < 1) {
        ncols = 1;
    }
    const size_t nrows = (num + ncols - 1) / ncols;
    for (size_t r = 0; r < nrows; ++r) {
        std::vector<size_t> idxs;
        for (size_t c = 0; c * nrows + r < num; ++c) {
            idxs.push_back(c * nrows + r);
        }
        if (idxs.empty()) {
            continue;
        }
        if (has_indent) {
            (void)fputs(indent.c_str(), out);
        }
        for (size_t k = 0; k < idxs.size(); ++k) {
            if (k > 0) {
                (void)fputc(' ', out);
            }
            bool const is_last = (k == idxs.size() - 1);
            std::string cell = align_text(lines[idxs[k]], max_width, false);
            if (is_last) {
                cell = rstrip(cell);
            }
            (void)fputs(cell.c_str(), out);
        }
        (void)fputc('\n', out);
    }
}

// ---------------------------------------------------------------------------
// Table mode: align fields into columns.
// ---------------------------------------------------------------------------

// Number of data rows between repeated headers when -H is given.
constexpr int kHeaderRepeatEvery = 25;

void emit_table(const ColumnOptions* opts, const std::vector<std::string>& lines,
                FILE* out) {
    SepMode mode = SepMode::Whitespace;
    char sep_c = 0;
    std::string sep_s;
    if (opts->has_entry && !opts->entry.empty()) {
        mode = SepMode::String;
        sep_s = opts->entry;
    } else if (opts->has_sep) {
        mode = SepMode::Char;
        sep_c = opts->sep_char;
    }

    std::vector<Record> records =
        build_records(lines, opts->lines_per_record, mode, sep_c, sep_s);
    if (records.empty()) {
        return;
    }

    if (opts->has_col_names) {
        apply_column_names(records[0], opts->col_names);
    }

    size_t num_cols = 0;
    for (auto& rec : records) {
        num_cols = std::max(num_cols, rec.size());
    }

    std::vector<size_t> col_width(num_cols, 0);
    for (auto& rec : records) {
        for (size_t c = 0; c < rec.size(); ++c) {
            col_width[c] = std::max(col_width[c], cell_width(rec[c]));
        }
    }

    std::vector<bool> col_right(num_cols, false);
    for (size_t c = 0; c < num_cols; ++c) {
        if (opts->right_all) {
            col_right[c] = true;
        } else if (opts->rightmost && c == num_cols - 1) {
            col_right[c] = true;
        }
    }

    size_t const sep_spaces = opts->divider ? 2 : 1;

    auto print_record = [&](const Record& rec) {
        // Skip trailing empty cells so a ragged row does not emit trailing
        // whitespace. last_used is the highest column index with any content.
        int last_used = -1;
        for (size_t c = 0; c < rec.size(); ++c) {
            bool nonempty = false;
            for (auto& l : rec[c]) {
                if (!rstrip(l).empty()) {
                    nonempty = true;
                    break;
                }
            }
            if (nonempty) {
                last_used = static_cast<int>(c);
            }
        }
        size_t const num_print_cols = (last_used >= 0)
            ? static_cast<size_t>(last_used + 1)
            : 0;

        size_t num_out_lines = 0;
        for (size_t c = 0; c < num_print_cols; ++c) {
            num_out_lines = std::max(num_out_lines, rec[c].size());
        }
        if (num_out_lines == 0) {
            num_out_lines = 1;
        }
        for (size_t p = 0; p < num_out_lines; ++p) {
            if (opts->has_indent) {
                (void)fputs(opts->indent.c_str(), out);
            }
            for (size_t c = 0; c < num_print_cols; ++c) {
                if (c > 0) {
                    for (size_t s = 0; s < sep_spaces; ++s) {
                        (void)fputc(' ', out);
                    }
                }
                bool const is_last_col = (c == num_print_cols - 1);
                std::string text;
                if (c < rec.size() && p < rec[c].size()) {
                    text = rec[c][p];
                }
                std::string cellout = align_text(text, col_width[c], col_right[c]);
                if (is_last_col) {
                    cellout = rstrip(cellout);
                }
                (void)fputs(cellout.c_str(), out);
            }
            (void)fputc('\n', out);
        }
    };

    print_record(records[0]);
    int data_since_header = 0;
    for (size_t i = 1; i < records.size(); ++i) {
        if (opts->print_head && data_since_header > 0 &&
            data_since_header % kHeaderRepeatEvery == 0) {
            print_record(records[0]);
            data_since_header = 0;
        }
        print_record(records[i]);
        ++data_since_header;
    }
}

void print_help(const char* prog) {
    (void)printf("Usage: %s [OPTION]... [FILE]...\n", prog);
    (void)printf("Format data into columns.\n");
    (void)printf("\n");
    (void)printf("  -c, --width=WIDTH            set output width to WIDTH characters (default 80)\n");
    (void)printf("  -s, --separator=CHAR         set the input field separator (default: runs of whitespace)\n");
    (void)printf("  -t, --table                  make the output a table\n");
    (void)printf("  -o, --table-output=FILE      send output to FILE instead of stdout\n");
    (void)printf("  -N, --table-column-names=TEXT override column names (comma separated; '-' keeps the original)\n");
    (void)printf("  -r, --table-right-justified  right align the columns\n");
    (void)printf("  -R, --table-right            right align the columns\n");
    (void)printf("  -C, --table-rightmost        right align the rightmost column\n");
    (void)printf("  -d, --table-divider          insert a divider between columns\n");
    (void)printf("  -L, --table-indent=TEXT      indent each output line with TEXT\n");
    (void)printf("  -a, --table-full             allow full output (no width constraint)\n");
    (void)printf("  -e, --table-entry=TEXT       column separator within table cells\n");
    (void)printf("  -l, --table-lines=NUM        fold the table into line groups of NUM\n");
    (void)printf("  -H, --table-print-head       repeat the header line\n");
    (void)printf("  -h, --help                   display this help and exit\n");
    (void)printf("      --version                output version information and exit\n");
    (void)printf("\n");
    (void)printf("With no FILE, or when FILE is -, read standard input.\n");
}

}  // namespace

int column_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(nullptr, "version", "output version information and exit");
    struct arg_int* width_opt = arg_int0("c", "width", "<num>", "set output width to num characters");
    struct arg_str* sep_opt = arg_str0("s", "separator", "<char>", "set the input field separator");
    struct arg_lit* table_opt = arg_lit0("t", "table", "make the output a table");
    struct arg_str* output_opt = arg_str0("o", "table-output", "<file>", "send output to file");
    struct arg_str* names_opt = arg_str0("N", "table-column-names", "<text>", "override column names");
    struct arg_lit* right_justified_opt = arg_lit0("r", "table-right-justified", "right align the columns");
    struct arg_lit* table_right_opt = arg_lit0("R", "table-right", "right align the columns");
    struct arg_lit* rightmost_opt = arg_lit0("C", "table-rightmost", "right align the rightmost column");
    struct arg_lit* divider_opt = arg_lit0("d", "table-divider", "insert a divider between columns");
    struct arg_str* indent_opt = arg_str0("L", "table-indent", "<text>", "indent each line with text");
    struct arg_lit* full_opt = arg_lit0("a", "table-full", "allow full output");
    struct arg_str* entry_opt = arg_str0("e", "table-entry", "<text>", "column separator within cells");
    struct arg_int* lines_opt = arg_int0("l", "table-lines", "<num>", "fold the table into line groups");
    struct arg_lit* head_opt = arg_lit0("H", "table-print-head", "repeat the header line");
    struct arg_file* file_arg = arg_filen(nullptr, nullptr, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(32);

    ArgTable at({help_opt, version_opt, width_opt, sep_opt, table_opt,
                 output_opt, names_opt, right_justified_opt, table_right_opt,
                 rightmost_opt,
                 divider_opt, indent_opt, full_opt, entry_opt, lines_opt,
                 head_opt, file_arg, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_help(argv[0]);
        return 0;
    }
    if (version_opt->count > 0) {
        print_version("column");
        return 0;
    }
    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    ColumnOptions opts;
    if (width_opt->count > 0) {
        opts.width = width_opt->ival[0];
    }
    if (sep_opt->count > 0) {
        opts.has_sep = true;
        opts.sep_char = parse_sep_char(sep_opt->sval[0]);
    }
    opts.table = (table_opt->count > 0);
    if (output_opt->count > 0) {
        opts.has_output_file = true;
        opts.output_file = output_opt->sval[0];
    }
    if (names_opt->count > 0) {
        opts.has_col_names = true;
        opts.col_names = names_opt->sval[0];
    }
    opts.right_all = (right_justified_opt->count > 0 || table_right_opt->count > 0);
    opts.rightmost = (rightmost_opt->count > 0);
    opts.divider = (divider_opt->count > 0);
    if (indent_opt->count > 0) {
        opts.has_indent = true;
        opts.indent = indent_opt->sval[0];
    }
    if (entry_opt->count > 0) {
        opts.has_entry = true;
        opts.entry = entry_opt->sval[0];
    }
    if (lines_opt->count > 0) {
        opts.lines_per_record = lines_opt->ival[0];
    }
    opts.print_head = (head_opt->count > 0);

    FILE* out = stdout;
    FILE* out_file = nullptr;
    if (opts.has_output_file) {
        out_file = fopen(opts.output_file.c_str(), "w");
        if (out_file == nullptr) {
            (void)fprintf(stderr, "column: %s: %s\n", opts.output_file.c_str(),
                          strerror(errno));
            return 1;
        }
        out = out_file;
    }

    auto process = [&](FILE* fp) {
        std::vector<std::string> const lines = read_all_lines(fp);
        if (opts.table) {
            emit_table(&opts, lines, out);
        } else {
            emit_fill(lines, opts.width, opts.indent, opts.has_indent, out);
        }
    };

    if (file_arg->count == 0) {
        process(stdin);
    } else {
        for (int i = 0; i < file_arg->count; ++i) {
            const char* const fname = file_arg->filename[i];
            if (strcmp(fname, "-") == 0) {
                process(stdin);
            } else {
                FILE* fp = fopen(fname, "r");
                if (fp == nullptr) {
                    (void)fprintf(stderr, "column: %s: %s\n", fname, strerror(errno));
                    continue;
                }
                process(fp);
                (void)fclose(fp);
            }
        }
    }

    if (out_file != nullptr) {
        (void)fclose(out_file);
    }
    return 0;
}

REGISTER_COMMAND("column", column_command, "Format data into columns");
