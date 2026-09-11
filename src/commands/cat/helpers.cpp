#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <ctime>
#include <regex.h>
#include <vector>
#include <algorithm>

#include "commands/cat/helpers.hpp"
#include "commands/cat.hpp"

#define HEADER_BANNER_WIDTH 70
#define MODE_STR_LEN 11

#define ASCII_DEL 127
#define ASCII_CP1252_END 159
#define ASCII_160 160
#define ASCII_255 255

static PipelineLine* pipeline_line_copy(const PipelineLine* orig) {
    PipelineLine* copy = new PipelineLine;
    copy->text = orig->text;
    copy->orig_index = orig->orig_index;
    return copy;
}

std::vector<PipelineLine*> read_file_to_lines(const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == nullptr) { return {}; }

    std::vector<PipelineLine*> lines;
    char buf[4096];
    int line_idx = 0;

    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        PipelineLine* pl = new PipelineLine;
        pl->text = buf;
        pl->orig_index = line_idx++;
        lines.push_back(pl);
    }

    (void)fclose(fp);
    return lines;
}

std::vector<PipelineLine*> read_stdin_to_lines() {
    std::vector<PipelineLine*> lines;
    char buf[4096];
    int line_idx = 0;

    while (fgets(buf, sizeof(buf), stdin) != nullptr) {
        PipelineLine* pl = new PipelineLine;
        pl->text = buf;
        pl->orig_index = line_idx++;
        lines.push_back(pl);
    }

    return lines;
}

void free_pipeline_lines(std::vector<PipelineLine*>* lines) {
    if (lines != nullptr) {
        for (auto* pl : *lines) {
            delete pl;
        }
        delete lines;
    }
}

std::vector<PipelineLine*>* slice_range(std::vector<PipelineLine*>* lines, int start, int end) {
    auto* result = new std::vector<PipelineLine*>;
    int const s = std::max(0, start - 1);
    int const e = (end > 0) ? std::min(static_cast<int>(lines->size()), end) : static_cast<int>(lines->size());
    if (s >= e) { return result; }
    for (int i = s; i < e; i++) {
        result->push_back(pipeline_line_copy((*lines)[i]));
    }
    return result;
}

std::vector<PipelineLine*>* slice_head(std::vector<PipelineLine*>* lines, int n) {
    auto* result = new std::vector<PipelineLine*>;
    int const limit = std::min(n, static_cast<int>(lines->size()));
    for (int i = 0; i < limit; i++) {
        result->push_back(pipeline_line_copy((*lines)[i]));
    }
    return result;
}

std::vector<PipelineLine*>* slice_tail(std::vector<PipelineLine*>* lines, int n) {
    auto* result = new std::vector<PipelineLine*>;
    int const start = std::max(0, static_cast<int>(lines->size()) - n);
    for (int i = start; i < static_cast<int>(lines->size()); i++) {
        result->push_back(pipeline_line_copy((*lines)[i]));
    }
    return result;
}

std::vector<PipelineLine*>* squeeze_blank_lines(std::vector<PipelineLine*>* lines) {
    auto* result = new std::vector<PipelineLine*>;
    int prev_blank = 0;
    for (size_t i = 0; i < lines->size(); i++) {
        PipelineLine* pl = (*lines)[i];
        int const blank = static_cast<int>(pl->text.empty() || pl->text[0] == '\n');
        if ((blank != 0) && (prev_blank != 0)) { continue; }
        prev_blank = blank;
        result->push_back(pipeline_line_copy(pl));
    }
    return result;
}

std::vector<unsigned int>* find_matching_indices(std::vector<PipelineLine*>* lines, const char* pattern) {
    auto* indices = new std::vector<unsigned int>;
    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) { return indices; }

    for (size_t i = 0; i < lines->size(); i++) {
        PipelineLine *const pl = (*lines)[i];
        if (regexec(&regex, pl->text.c_str(), 0, NULL, 0) == 0) {
            indices->push_back(static_cast<unsigned int>(i));
        }
    }

    regfree(&regex);
    return indices;
}

std::vector<unsigned int>* expand_indices(std::vector<PipelineLine*>* lines,
                                           std::vector<unsigned int>* match_indices, int context) {
    auto* expanded = new std::vector<unsigned int>;
    int const total_lines = static_cast<int>(lines->size());
    if (total_lines == 0) { return expanded; }

    int last_added = -1;

    for (size_t i = 0; i < match_indices->size(); i++) {
        int const idx = static_cast<int>((*match_indices)[i]);
        int const start = std::max(0, idx - context);
        int const end = std::min(total_lines - 1, idx + context);

        for (int j = start; j <= end; j++) {
            if (j > last_added) {
                expanded->push_back(static_cast<unsigned int>(j));
                last_added = j;
            }
        }
    }

    return expanded;
}

std::vector<PipelineLine*>* extract_lines(std::vector<PipelineLine*>* lines,
                                           std::vector<unsigned int>* indices) {
    auto* result = new std::vector<PipelineLine*>;
    for (size_t i = 0; i < indices->size(); i++) {
        int const idx = static_cast<int>((*indices)[i]);
        result->push_back(pipeline_line_copy((*lines)[idx]));
    }
    return result;
}

void print_stats(std::vector<PipelineLine*>* lines, FILE* out) {
    int const line_count = static_cast<int>(lines->size());
    int word_count = 0;
    int char_count = 0;

    for (size_t i = 0; i < lines->size(); i++) {
        PipelineLine *const pl = (*lines)[i];
        char_count += static_cast<int>(pl->text.length());

        int in_word = 0;
        for (const char* p = pl->text.c_str(); (*p) != 0; p++) {
            if (*p == ' ' || *p == '\t' || *p == '\n') {
                in_word = 0;
            } else if (in_word == 0) {
                in_word = 1;
                word_count++;
            }
        }
    }

    (void)fprintf(out, "  %d lines  %d words  %d characters\n", line_count, word_count, char_count);
}

void print_header(const char* path, FILE* out) {
    struct stat st;
    if (stat(path, &st) != 0) { return; }

    char timebuf[64];
    const struct tm* tm = localtime(&st.st_mtime);
    if (tm == nullptr) { return; }
    (void)strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    char modebuf[MODE_STR_LEN] = "----------";
    if (S_ISREG(st.st_mode)) { modebuf[0] = '-'; }
    else if (S_ISDIR(st.st_mode)) { modebuf[0] = 'd'; }
    else if (S_ISLNK(st.st_mode)) { modebuf[0] = 'l'; }
    if ((st.st_mode & S_IRUSR) != 0u) { modebuf[1] = 'r'; }
    if ((st.st_mode & S_IWUSR) != 0u) { modebuf[2] = 'w'; }
    if ((st.st_mode & S_IXUSR) != 0u) { modebuf[3] = 'x'; }
    if ((st.st_mode & S_IRGRP) != 0u) { modebuf[4] = 'r'; }
    if ((st.st_mode & S_IWGRP) != 0u) { modebuf[5] = 'w'; }
    if ((st.st_mode & S_IXGRP) != 0u) { modebuf[6] = 'x'; }
    if ((st.st_mode & S_IROTH) != 0u) { modebuf[7] = 'r'; }
    if ((st.st_mode & S_IWOTH) != 0u) { modebuf[8] = 'w'; }
    if ((st.st_mode & S_IXOTH) != 0u) { modebuf[9] = 'x'; }

    char sizestr[32];
    if (st.st_size < 1024) {
        (void)snprintf(sizestr, sizeof(sizestr), "%ld B", static_cast<long>(st.st_size));
    } else if (st.st_size < static_cast<off_t>(1024) * 1024) {
        (void)snprintf(sizestr, sizeof(sizestr), "%.1f KiB", static_cast<double>(st.st_size) / 1024);
    } else {
        (void)snprintf(sizestr, sizeof(sizestr), "%.1f MiB", static_cast<double>(st.st_size) / (static_cast<off_t>(1024) * 1024));
    }

    int const pathlen = static_cast<int>(strlen(path));
    (void)fprintf(out, "-- %s ", path);
    for (int i = 4 + pathlen; i < HEADER_BANNER_WIDTH; i++) { (void)fputc('-', out); }
    (void)fputc('\n', out);
    (void)fprintf(out, "  Mode: %s   Size: %s   Modified: %s\n", modebuf, sizestr, timebuf);
    for (int i = 0; i < HEADER_BANNER_WIDTH; i++) { (void)fputc('-', out); }
    (void)fputc('\n', out);
}

void format_line_number(int num, int format, FILE* out) {
    switch (format) {
        case 1: (void)fprintf(out, "0x%04x  ", num); break;
        case 2: (void)fprintf(out, "%06o  ", num); break;
        default: (void)fprintf(out, "%6d  ", num); break;
    }
}

static void output_char_visual(unsigned char c, int show_tabs, int show_nonprinting,
                                int show_ends, int is_last, FILE* out) {
    if ((show_tabs != 0) && c == '\t') {
        (void)fprintf(out, "^I");
        return;
    }

    if (show_nonprinting != 0) {
        if (c == '\n' || c == '\t') {
            if (c == '\n' && (show_ends != 0) && (is_last != 0)) {
                (void)fprintf(out, "$\n");
                return;
            }
            (void)fputc(c, out);
            return;
        }
        if (c < 32) {
            (void)fprintf(out, "^%c", c + 64);
            return;
        }
        if (c == ASCII_DEL) {
            (void)fprintf(out, "^?");
            return;
        }
        if (c >= 128 && c <= ASCII_CP1252_END) {
            (void)fprintf(out, "M-^%c", (c - 128) + 64);
            return;
        }
        if (c == ASCII_255) {
            (void)fprintf(out, "M-^?");
            return;
        }
        if (c >= ASCII_160) {
            (void)fprintf(out, "M-%c", c - 128);
            return;
        }
    }

    (void)fputc(c, out);
}

void output_line_visual(const char* line, const CatOptions* opts, FILE* out) {
    size_t const len = strlen(line);
    int const has_newline = static_cast<int>(len > 0 && line[len - 1] == '\n');
    size_t const content_len = (has_newline != 0) ? len - 1 : len;

    int const show_nonprinting = opts->show_nonprinting;
    int const show_tabs = opts->show_tabs;
    int const show_ends = opts->show_ends;

    if (show_nonprinting != 0) {
        for (size_t j = 0; j < content_len; j++) {
            output_char_visual(static_cast<unsigned char>(line[j]), show_tabs, 1, show_ends, 0, out);
        }
        if (has_newline != 0) {
            if (show_ends != 0) {
                (void)fprintf(out, "$\n");
            } else {
                (void)fprintf(out, "\n");
            }
        }
    } else if (show_tabs != 0) {
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '\t') {
                (void)fprintf(out, "^I");
            } else {
                (void)fputc(line[j], out);
            }
        }
    } else if ((show_ends != 0) && (has_newline != 0)) {
        for (size_t j = 0; j < content_len; j++) {
            (void)fputc(line[j], out);
        }
        (void)fprintf(out, "$\n");
    } else {
        (void)fputs(line, out);
        if (has_newline == 0) { (void)fputc('\n', out); }
    }
}
