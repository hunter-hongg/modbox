#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>

#include <argtable3.h>

#include "commands/cut.hpp"

struct Range {
    int64_t start; // 0-based, inclusive
    int64_t end;   // 0-based, inclusive; -1 = unbounded (to end of line)
};

// Parse a LIST argument like "1,3-5,7-" into a vector of Ranges.
// Ranges are 1-based in the input, converted to 0-based internally.
static std::vector<Range> parse_list(const char* str) {
    std::vector<Range> ranges;
    const char* p = str;

    while (*p != '\0') {
        // Skip leading commas
        while (*p == ',') ++p;
        if (*p == '\0') break;

        Range r;
        r.start = 0;
        r.end = -1;

        char* endptr = nullptr;
        int64_t n = strtoll(p, &endptr, 10);
        if (endptr == p) break; // no digits

        if (*endptr == '\0' || *endptr == ',') {
            // "N"
            r.start = n - 1;
            r.end = n - 1;
            p = endptr;
        } else if (*endptr == '-') {
            p = endptr + 1;
            if (*p == '\0' || *p == ',') {
                // "N-"
                r.start = (n > 0) ? n - 1 : 0;
                r.end = -1;
            } else {
                // "N-M"
                int64_t m = strtoll(p, &endptr, 10);
                r.start = n - 1;
                r.end = m - 1;
                p = endptr;
            }
        } else {
            break;
        }

        // --complement with "-M" means from start to M
        // Support "-M" notation (start=0 when start field empty)
        if (r.start < 0) r.start = 0;

        ranges.push_back(r);

        if (*p == ',') ++p;
    }

    return ranges;
}

// Sort ranges by start and merge overlapping/adjacent ones.
static void sort_and_merge_ranges(std::vector<Range>& ranges) {
    if (ranges.size() <= 1) return;

    std::sort(ranges.begin(), ranges.end(),
              [](const Range& a, const Range& b) { return a.start < b.start; });

    std::vector<Range> merged;
    merged.reserve(ranges.size());
    merged.push_back(ranges[0]);

    for (size_t i = 1; i < ranges.size(); ++i) {
        Range& last = merged.back();

        if (ranges[i].start <= last.end + 1 || last.end == -1) {
            // Overlapping or adjacent — extend last if needed
            if (ranges[i].end == -1)
                last.end = -1;
            else if (last.end != -1 && ranges[i].end > last.end)
                last.end = ranges[i].end;
        } else {
            merged.push_back(ranges[i]);
        }
    }

    ranges = std::move(merged);
}

// Replace unbounded end (-1) with the actual last index (count-1).
// Clamp out-of-bounds values.
static void resolve_ranges(std::vector<Range>& ranges, int64_t count) {
    for (auto& r : ranges) {
        if (r.start < 0) r.start = 0;
        if (r.end == -1 || r.end >= count) r.end = count - 1;
    }
}

// Invert the selected ranges.  Assumes ranges have been resolved (no -1 ends).
static std::vector<Range> complement_ranges(const std::vector<Range>& ranges,
                                            int64_t count) {
    std::vector<Range> result;

    if (ranges.empty()) {
        if (count > 0) result.push_back({0, count - 1});
        return result;
    }

    // Before first range
    if (ranges[0].start > 0)
        result.push_back({0, ranges[0].start - 1});

    // Between ranges
    for (size_t i = 1; i < ranges.size(); ++i) {
        int64_t gap_start = ranges[i - 1].end + 1;
        int64_t gap_end = ranges[i].start - 1;
        if (gap_start <= gap_end)
            result.push_back({gap_start, gap_end});
    }

    // After last range
    if (ranges.back().end < count - 1)
        result.push_back({ranges.back().end + 1, count - 1});

    return result;
}

// -------------------------------------------------------------------
// Field mode  (-f)
// -------------------------------------------------------------------

// Split a line into fields by delimiter.
static std::vector<std::string> split_line(const std::string& line, char delim) {
    std::vector<std::string> fields;
    size_t start = 0;

    for (;;) {
        size_t pos = line.find(delim, start);
        if (pos == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }

    return fields;
}

// Join selected fields with the given delimiter string.
static std::string join_fields(const std::vector<std::string>& fields,
                               const std::string& delim) {
    if (fields.empty()) return {};

    std::string out = fields[0];
    for (size_t i = 1; i < fields.size(); ++i) {
        out += delim;
        out += fields[i];
    }
    return out;
}

static void process_fields(FILE* fp, const std::vector<Range>& list_ranges,
                           char delim, int complement, int only_delimited,
                           const std::string& output_delim) {
    std::string line;
    int c;
    bool done = false;

    while (!done) {
        line.clear();
        while ((c = fgetc(fp)) != EOF && c != '\n') {
            line += static_cast<char>(c);
        }
        if (c == EOF && line.empty()) break;

        bool has_delim = (line.find(delim) != std::string::npos);

        if (!has_delim) {
            if (only_delimited) {
                if (c == EOF) break;
                continue;
            }
            // no delimiter and not only-delimited → pass through
            printf("%s\n", line.c_str());
            if (c == EOF) break;
            continue;
        }

        auto fields = split_line(line, delim);
        int64_t nfields = static_cast<int64_t>(fields.size());

        auto ranges = list_ranges;
        resolve_ranges(ranges, nfields);
        sort_and_merge_ranges(ranges);

        if (complement) {
            ranges = complement_ranges(ranges, nfields);
        }

        std::vector<std::string> selected;
        for (const auto& r : ranges) {
            for (int64_t i = r.start; i <= r.end && i < nfields; ++i) {
                selected.push_back(fields[i]);
            }
        }

        std::string out = join_fields(selected, output_delim);
        printf("%s\n", out.c_str());

        if (c == EOF) break;
    }
}

// -------------------------------------------------------------------
// Byte / character mode  (-b / -c  — identical for single-byte locale)
// -------------------------------------------------------------------

static void process_chars_or_bytes(FILE* fp, const std::vector<Range>& list_ranges,
                                   int complement) {
    std::string line;
    int c;
    bool done = false;

    while (!done) {
        line.clear();
        while ((c = fgetc(fp)) != EOF && c != '\n') {
            line += static_cast<char>(c);
        }
        if (c == EOF && line.empty()) break;

        int64_t len = static_cast<int64_t>(line.size());

        auto ranges = list_ranges;
        resolve_ranges(ranges, len);
        sort_and_merge_ranges(ranges);

        if (complement) {
            ranges = complement_ranges(ranges, len);
        }

        for (const auto& r : ranges) {
            for (int64_t i = r.start; i <= r.end && i < len; ++i) {
                putchar(static_cast<unsigned char>(line[i]));
            }
        }
        putchar('\n');

        if (c == EOF) break;
    }
}

// -------------------------------------------------------------------
// Zero-terminated mode  (-z)
// -------------------------------------------------------------------

static void process_fields_z(FILE* fp, const std::vector<Range>& list_ranges,
                              char delim, int complement, int only_delimited,
                              const std::string& output_delim) {
    std::vector<char> buf;
    int c;

    while ((c = fgetc(fp)) != EOF) {
        buf.clear();
        while (c != EOF && c != '\0') {
            buf.push_back(static_cast<char>(c));
            c = fgetc(fp);
        }
        // buf contains the record (may be empty for consecutive NULs)

        bool has_delim = (std::find(buf.begin(), buf.end(), delim) != buf.end());

        if (!has_delim) {
            if (only_delimited) {
                continue; // skip
            }
            // pass through
            for (char ch : buf) putchar(ch);
            putchar('\0');
            continue;
        }

        std::string line(buf.begin(), buf.end());
        auto fields = split_line(line, delim);
        int64_t nfields = static_cast<int64_t>(fields.size());

        auto ranges = list_ranges;
        resolve_ranges(ranges, nfields);
        sort_and_merge_ranges(ranges);

        if (complement) {
            ranges = complement_ranges(ranges, nfields);
        }

        std::vector<std::string> selected;
        for (const auto& r : ranges) {
            for (int64_t i = r.start; i <= r.end && i < nfields; ++i) {
                selected.push_back(fields[i]);
            }
        }

        std::string out = join_fields(selected, output_delim);
        printf("%s", out.c_str());
        putchar('\0');
    }
}

static void process_chars_or_bytes_z(FILE* fp, const std::vector<Range>& list_ranges,
                                     int complement) {
    std::vector<char> buf;
    int c;

    while ((c = fgetc(fp)) != EOF) {
        buf.clear();
        while (c != EOF && c != '\0') {
            buf.push_back(static_cast<char>(c));
            c = fgetc(fp);
        }

        int64_t len = static_cast<int64_t>(buf.size());
        auto ranges = list_ranges;
        resolve_ranges(ranges, len);
        sort_and_merge_ranges(ranges);

        if (complement) {
            ranges = complement_ranges(ranges, len);
        }

        for (const auto& r : ranges) {
            for (int64_t i = r.start; i <= r.end && i < len; ++i) {
                putchar(buf[i]);
            }
        }
        putchar('\0');
    }
}

// -------------------------------------------------------------------
// Helpers to extract a single character from a delimiter string,
// handling common escape sequences.
// -------------------------------------------------------------------

static char parse_delimiter_char(const char* s) {
    if (s == nullptr || *s == '\0') return '\t';
    // Two-char escape sequences
    if (s[0] == '\\') {
        switch (s[1]) {
            case 't': return '\t';
            case 'n': return '\n';
            case '0': return '\0';
            case '\\': return '\\';
            default: break;
        }
    }
    return s[0];
}

// -------------------------------------------------------------------
// Entry point
// -------------------------------------------------------------------

void cut_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_str* byte_opt  = arg_str0("b", "bytes", "LIST", "select only these bytes");
    struct arg_str* char_opt  = arg_str0("c", "characters", "LIST", "select only these characters");
    struct arg_str* delim_opt = arg_str0("d", "delimiter", "DELIM", "use DELIM instead of TAB for field delimiter");
    struct arg_str* field_opt = arg_str0("f", "fields", "LIST", "select only these fields");
    struct arg_str* out_delim_opt = arg_str0(nullptr, "output-delimiter", "STRING", "use STRING as the output delimiter");
    struct arg_lit* complement_opt = arg_lit0(nullptr, "complement", "complement the set of selected bytes, characters or fields");
    struct arg_lit* only_delimited_opt = arg_lit0("s", "only-delimited", "do not print lines not containing delimiters");
    struct arg_lit* zero_opt = arg_lit0("z", "zero-terminated", "line delimiter is NUL, not newline");
    struct arg_lit* n_opt = arg_lit0("n", nullptr, "(ignored)");
    struct arg_file* file_arg = arg_filen(nullptr, nullptr, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {help_opt, byte_opt, char_opt, delim_opt, field_opt,
                        out_delim_opt, complement_opt, only_delimited_opt,
                        zero_opt, n_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s OPTION... [FILE]...\n", argv[0]);
        printf("Print selected parts of lines from each FILE to standard output.\n");
        printf("\n");
        printf("Mandatory arguments to long options are mandatory for short options too.\n");
        printf("  -b, --bytes=LIST        select only these bytes\n");
        printf("  -c, --characters=LIST   select only these characters\n");
        printf("  -d, --delimiter=DELIM   use DELIM instead of TAB for field delimiter\n");
        printf("  -f, --fields=LIST       select only these fields\n");
        printf("  -n                      (ignored)\n");
        printf("      --complement        complement the set of selected bytes, characters or fields\n");
        printf("  -s, --only-delimited    do not print lines not containing delimiters\n");
        printf("      --output-delimiter=STRING  use STRING as the output delimiter\n");
        printf("  -z, --zero-terminated   line delimiter is NUL, not newline\n");
        printf("      --help     display this help and exit\n");
        printf("\n");
        printf("Use one, and only one of -b, -c or -f.  Each LIST is made up of one\n");
        printf("range, or many ranges separated by commas.  Selected input is written\n");
        printf("in the same order that it is read, and is written exactly once.\n");
        printf("Each range is one of:\n");
        printf("\n");
        printf("  N     N'th byte, character or field, counted from 1\n");
        printf("  N-    from N'th byte, character or field, to end of line\n");
        printf("  N-M   from N'th to M'th (included) byte, character or field\n");
        printf("  -M    from first to M'th (included) byte, character or field\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Exactly one of -b, -c, -f is required
    int mode_count = static_cast<int>(byte_opt->count > 0) +
                     static_cast<int>(char_opt->count > 0) +
                     static_cast<int>(field_opt->count > 0);

    if (mode_count == 0) {
        fprintf(stderr, "%s: you must specify a list of bytes, characters, or fields\n",
                argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (mode_count > 1) {
        fprintf(stderr, "%s: only one type of list may be specified\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // -d requires -f
    if (delim_opt->count > 0 && field_opt->count == 0) {
        fprintf(stderr, "%s: an input delimiter may be specified only when operating on fields\n",
                argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // -s requires -f
    if (only_delimited_opt->count > 0 && field_opt->count == 0) {
        fprintf(stderr, "%s: an input delimiter may be specified only when operating on fields\n",
                argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Parse list
    const char* list_str = nullptr;
    bool is_field_mode = (field_opt->count > 0);

    if (byte_opt->count > 0)
        list_str = byte_opt->sval[0];
    else if (char_opt->count > 0)
        list_str = char_opt->sval[0];
    else if (field_opt->count > 0)
        list_str = field_opt->sval[0];

    std::vector<Range> ranges;
    if (list_str != nullptr)
        ranges = parse_list(list_str);

    char delim = '\t';
    if (delim_opt->count > 0)
        delim = parse_delimiter_char(delim_opt->sval[0]);

    int complement = (complement_opt->count > 0) ? 1 : 0;
    int only_delimited = (only_delimited_opt->count > 0) ? 1 : 0;
    int zero_terminated = (zero_opt->count > 0) ? 1 : 0;

    // Determine output delimiter
    std::string output_delim;
    if (out_delim_opt->count > 0) {
        output_delim = out_delim_opt->sval[0];
    } else if (is_field_mode) {
        output_delim = std::string(1, delim);
    }

    // Process input
    auto process_fn = [&](FILE* fp) {
        if (is_field_mode) {
            if (zero_terminated)
                process_fields_z(fp, ranges, delim, complement, only_delimited, output_delim);
            else
                process_fields(fp, ranges, delim, complement, only_delimited, output_delim);
        } else {
            if (zero_terminated)
                process_chars_or_bytes_z(fp, ranges, complement);
            else
                process_chars_or_bytes(fp, ranges, complement);
        }
    };

    if (file_arg->count == 0) {
        process_fn(stdin);
    } else {
        for (int i = 0; i < file_arg->count; ++i) {
            const char* fname = file_arg->filename[i];
            if (strcmp(fname, "-") == 0) {
                process_fn(stdin);
            } else {
                FILE* fp = fopen(fname, "r");
                if (fp == nullptr) {
                    fprintf(stderr, "%s: %s: %s\n", argv[0], fname, strerror(errno));
                    continue;
                }
                process_fn(fp);
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
