#include <argtable3.h>
#include <regex.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdint>

#include "commands/tac.hpp"

#define ARG_END_SIZE 20

struct TacRecord {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> sep_bytes;
};

static void write_records(const std::vector<TacRecord>& records, FILE* out) {
    for (int i = (int)records.size() - 1; i >= 0; i--) {
        const auto& rec = records[i];
        if (!rec.sep_bytes.empty()) {
            if (fwrite(rec.sep_bytes.data(), 1, rec.sep_bytes.size(), out) != rec.sep_bytes.size()) {
                fprintf(stderr, "tac: write error\n");
                return;
            }
        }
        if (!rec.bytes.empty()) {
            if (fwrite(rec.bytes.data(), 1, rec.bytes.size(), out) != rec.bytes.size()) {
                fprintf(stderr, "tac: write error\n");
                return;
            }
        }
    }
}

static std::vector<TacRecord> split_custom(const uint8_t* data, size_t total_len,
                                           const char* sep, int before_mode, int regex_mode) {
    std::vector<TacRecord> records;
    size_t sep_len = strlen(sep);

    if (regex_mode) {
        regex_t regex;
        if (regcomp(&regex, sep, REG_EXTENDED) != 0) {
            fprintf(stderr, "tac: invalid regex: %s\n", sep);
            return records;
        }

        size_t pos = 0;
        std::vector<uint8_t> pending_sep;
        while (pos < total_len) {
            regmatch_t match;
            int rc = regexec(&regex, (const char*)data + pos, 1, &match, 0);
            if (rc != 0 || match.rm_so == -1) {
                break;
            }

            size_t abs_match_start = pos + (size_t)match.rm_so;
            size_t abs_match_end = pos + (size_t)match.rm_eo;
            size_t cur_match_len = abs_match_end - abs_match_start;

            if (before_mode) {
                if (abs_match_start > pos) {
                    TacRecord rec;
                    rec.bytes.assign(data + pos, data + abs_match_start);
                    rec.sep_bytes = pending_sep;
                    records.push_back(std::move(rec));
                } else if (records.empty()) {
                    TacRecord rec;
                    rec.sep_bytes = pending_sep;
                    records.push_back(std::move(rec));
                }
                pending_sep.clear();
                pending_sep.insert(pending_sep.end(), data + abs_match_start, data + abs_match_end);
                pos = abs_match_end;
            } else {
                TacRecord rec;
                rec.bytes.assign(data + pos, data + abs_match_end);
                records.push_back(std::move(rec));
                pos = abs_match_end;
            }
        }

        if (pos < total_len) {
            TacRecord rec;
            rec.bytes.assign(data + pos, data + total_len);
            if (before_mode) {
                rec.sep_bytes = std::move(pending_sep);
            }
            records.push_back(std::move(rec));
        } else if (records.empty()) {
            records.emplace_back();
        }

        regfree(&regex);
    } else {
        size_t pos = 0;
        std::vector<uint8_t> pending_sep;
        while (pos < total_len) {
            size_t found = total_len;
            for (size_t i = pos; i + sep_len <= total_len; i++) {
                if (memcmp(data + i, sep, sep_len) == 0) {
                    found = i;
                    break;
                }
            }

            if (found == total_len) {
                break;
            }

            if (before_mode) {
                if (found > pos) {
                    TacRecord rec;
                    rec.bytes.assign(data + pos, data + found);
                    rec.sep_bytes = pending_sep;
                    records.push_back(std::move(rec));
                } else if (records.empty()) {
                    TacRecord rec;
                    rec.sep_bytes = pending_sep;
                    records.push_back(std::move(rec));
                }
                pending_sep.clear();
                pending_sep.insert(pending_sep.end(), data + found, data + found + sep_len);
                pos = found + sep_len;
            } else {
                TacRecord rec;
                rec.bytes.assign(data + pos, data + found + sep_len);
                records.push_back(std::move(rec));
                pos = found + sep_len;
            }
        }

        if (pos < total_len) {
            TacRecord rec;
            rec.bytes.assign(data + pos, data + total_len);
            if (before_mode) {
                rec.sep_bytes = std::move(pending_sep);
            }
            records.push_back(std::move(rec));
        } else if (records.empty()) {
            records.emplace_back();
        }
    }

    return records;
}

static std::vector<TacRecord> split_by_newline(const uint8_t* data, size_t total_len) {
    std::vector<TacRecord> records;
    size_t pos = 0;

    while (pos < total_len) {
        const uint8_t* nl_ptr = (const uint8_t*)memchr(data + pos, '\n', total_len - pos);
        if (!nl_ptr) {
            TacRecord rec;
            if (total_len > pos) {
                rec.bytes.assign(data + pos, data + total_len);
            }
            records.push_back(std::move(rec));
            break;
        }
        size_t chunk_len = (size_t)(nl_ptr - data) - pos + 1;
        TacRecord rec;
        rec.bytes.assign(data + pos, data + pos + chunk_len);
        records.push_back(std::move(rec));
        pos = (size_t)(nl_ptr - data) + 1;
    }

    return records;
}

static std::vector<TacRecord> split_input(const uint8_t* data, size_t total_len,
                                          const char* sep, int before_mode, int regex_mode) {
    if (!sep || strlen(sep) == 0) {
        return split_by_newline(data, total_len);
    }
    return split_custom(data, total_len, sep, before_mode, regex_mode);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void tac_command(int argc, char** argv) {
    struct arg_lit* before_opt = arg_lit0("b", "before",
        "put separator before each chunk");
    struct arg_lit* regex_opt = arg_lit0("r", "regex",
        "treat separator as a regex");
    struct arg_str* sep_opt = arg_str0("s", "separator", "STR",
        "use STR instead of newline");
    struct arg_lit* help_opt = arg_lit0("h", "help",
        "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100,
        "file to reverse");
    struct arg_end* end = arg_end(ARG_END_SIZE);

    void* argtable[] = { before_opt, regex_opt, sep_opt, help_opt,
                         file_arg, end };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Reverse a file line by line.\n");
        printf("\n");
        printf("  -b, --before          put separator before each chunk\n");
        printf("  -r, --regex           treat separator as a regex\n");
        printf("  -s, --separator=STR   use STR instead of newline\n");
        printf("  -h, --help            display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int before_mode = (before_opt->count > 0);
    int regex_mode = (regex_opt->count > 0);
    const char* sep = sep_opt->count > 0 ? sep_opt->sval[0] : NULL;

    int from_stdin = (file_arg->count == 0);

    if (from_stdin) {
        std::vector<uint8_t> buf;
        char tmp[4096];
        size_t n;
        while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
            buf.insert(buf.end(), tmp, tmp + n);
        }
        if (!buf.empty()) {
            auto records = split_input(buf.data(), buf.size(), sep, before_mode, regex_mode);
            write_records(records, stdout);
        }
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            FILE* fp = fopen(file_arg->filename[i], "rb");
            if (!fp) {
                fprintf(stderr, "tac: %s: No such file or directory\n",
                        file_arg->filename[i]);
                continue;
            }

            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (fsize > 0) {
                std::vector<uint8_t> data((size_t)fsize);
                size_t nread = fread(data.data(), 1, (size_t)fsize, fp);
                fclose(fp);

                auto records = split_input(data.data(), nread, sep, before_mode, regex_mode);
                write_records(records, stdout);
            } else {
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
