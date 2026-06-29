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
    if (!fp) { return {}; }

    std::vector<PipelineLine*> lines;
    char buf[4096];
    int line_idx = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        PipelineLine* pl = new PipelineLine;
        pl->text = buf;
        pl->orig_index = line_idx++;
        lines.push_back(pl);
    }

    fclose(fp);
    return lines;
}

std::vector<PipelineLine*> read_stdin_to_lines(void) {
    std::vector<PipelineLine*> lines;
    char buf[4096];
    int line_idx = 0;

    while (fgets(buf, sizeof(buf), stdin)) {
        PipelineLine* pl = new PipelineLine;
        pl->text = buf;
        pl->orig_index = line_idx++;
        lines.push_back(pl);
    }

    return lines;
}

void free_pipeline_lines(std::vector<PipelineLine*>* lines) {
    if (lines) {
        for (auto* pl : *lines) {
            delete pl;
        }
        delete lines;
    }
}

std::vector<PipelineLine*>* slice_range(std::vector<PipelineLine*>* lines, int start, int end) {
    auto* result = new std::vector<PipelineLine*>;
    int s = std::max(0, start - 1);
    int e = (end > 0) ? std::min((int)lines->size(), end) : (int)lines->size();
    if (s >= e) { return result; }
    for (int i = s; i < e; i++) {
        result->push_back(pipeline_line_copy((*lines)[i]));
    }
    return result;
}

std::vector<PipelineLine*>* slice_head(std::vector<PipelineLine*>* lines, int n) {
    auto* result = new std::vector<PipelineLine*>;
    int limit = std::min(n, (int)lines->size());
    for (int i = 0; i < limit; i++) {
        result->push_back(pipeline_line_copy((*lines)[i]));
    }
    return result;
}

std::vector<PipelineLine*>* slice_tail(std::vector<PipelineLine*>* lines, int n) {
    auto* result = new std::vector<PipelineLine*>;
    int start = std::max(0, (int)lines->size() - n);
    for (int i = start; i < (int)lines->size(); i++) {
        result->push_back(pipeline_line_copy((*lines)[i]));
    }
    return result;
}

std::vector<PipelineLine*>* squeeze_blank_lines(std::vector<PipelineLine*>* lines) {
    auto* result = new std::vector<PipelineLine*>;
    int prev_blank = 0;
    for (size_t i = 0; i < lines->size(); i++) {
        PipelineLine* pl = (*lines)[i];
        int blank = (pl->text.empty() || pl->text[0] == '\n');
        if (blank && prev_blank) { continue; }
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
        PipelineLine* pl = (*lines)[i];
        if (regexec(&regex, pl->text.c_str(), 0, NULL, 0) == 0) {
            indices->push_back((unsigned int)i);
        }
    }

    regfree(&regex);
    return indices;
}

std::vector<unsigned int>* expand_indices(std::vector<PipelineLine*>* lines,
                                           std::vector<unsigned int>* match_indices, int context) {
    auto* expanded = new std::vector<unsigned int>;
    int total_lines = (int)lines->size();
    if (total_lines == 0) { return expanded; }

    int last_added = -1;

    for (size_t i = 0; i < match_indices->size(); i++) {
        int idx = (int)(*match_indices)[i];
        int start = std::max(0, idx - context);
        int end = std::min(total_lines - 1, idx + context);

        for (int j = start; j <= end; j++) {
            if (j > last_added) {
                expanded->push_back((unsigned int)j);
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
        int idx = (int)(*indices)[i];
        result->push_back(pipeline_line_copy((*lines)[idx]));
    }
    return result;
}

void print_stats(std::vector<PipelineLine*>* lines, FILE* out) {
    int line_count = (int)lines->size();
    int word_count = 0;
    int char_count = 0;

    for (size_t i = 0; i < lines->size(); i++) {
        PipelineLine* pl = (*lines)[i];
        char_count += (int)pl->text.length();

        int in_word = 0;
        for (const char* p = pl->text.c_str(); *p; p++) {
            if (*p == ' ' || *p == '\t' || *p == '\n') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                word_count++;
            }
        }
    }

    fprintf(out, "  %d lines  %d words  %d characters\n", line_count, word_count, char_count);
}

void print_header(const char* path, FILE* out) {
    struct stat st;
    if (stat(path, &st) != 0) { return; }

    char timebuf[64];
    struct tm* tm = localtime(&st.st_mtime);
    if (!tm) { return; }
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    char modebuf[MODE_STR_LEN] = "----------";
    if (S_ISREG(st.st_mode)) { modebuf[0] = '-'; }
    else if (S_ISDIR(st.st_mode)) { modebuf[0] = 'd'; }
    else if (S_ISLNK(st.st_mode)) { modebuf[0] = 'l'; }
    if (st.st_mode & S_IRUSR) { modebuf[1] = 'r'; }
    if (st.st_mode & S_IWUSR) { modebuf[2] = 'w'; }
    if (st.st_mode & S_IXUSR) { modebuf[3] = 'x'; }
    if (st.st_mode & S_IRGRP) { modebuf[4] = 'r'; }
    if (st.st_mode & S_IWGRP) { modebuf[5] = 'w'; }
    if (st.st_mode & S_IXGRP) { modebuf[6] = 'x'; }
    if (st.st_mode & S_IROTH) { modebuf[7] = 'r'; }
    if (st.st_mode & S_IWOTH) { modebuf[8] = 'w'; }
    if (st.st_mode & S_IXOTH) { modebuf[9] = 'x'; }

    char sizestr[32];
    if (st.st_size < 1024) {
        snprintf(sizestr, sizeof(sizestr), "%ld B", (long)st.st_size);
    } else if (st.st_size < (off_t)1024 * 1024) {
        snprintf(sizestr, sizeof(sizestr), "%.1f KiB", (double)st.st_size / 1024);
    } else {
        snprintf(sizestr, sizeof(sizestr), "%.1f MiB", (double)st.st_size / ((off_t)1024 * 1024));
    }

    int pathlen = (int)strlen(path);
    fprintf(out, "-- %s ", path);
    for (int i = 4 + pathlen; i < HEADER_BANNER_WIDTH; i++) { fputc('-', out); }
    fputc('\n', out);
    fprintf(out, "  Mode: %s   Size: %s   Modified: %s\n", modebuf, sizestr, timebuf);
    for (int i = 0; i < HEADER_BANNER_WIDTH; i++) { fputc('-', out); }
    fputc('\n', out);
}

void format_line_number(int num, int format, FILE* out) {
    switch (format) {
        case 1: fprintf(out, "0x%04x  ", num); break;
        case 2: fprintf(out, "%06o  ", num); break;
        default: fprintf(out, "%6d  ", num); break;
    }
}

static void output_char_visual(unsigned char c, int show_tabs, int show_nonprinting,
                                int show_ends, int is_last, FILE* out) {
    if (show_tabs && c == '\t') {
        fprintf(out, "^I");
        return;
    }

    if (show_nonprinting) {
        if (c == '\n' || c == '\t') {
            if (c == '\n' && show_ends && is_last) {
                fprintf(out, "$\n");
                return;
            }
            fputc(c, out);
            return;
        }
        if (c < 32) {
            fprintf(out, "^%c", c + 64);
            return;
        }
        if (c == ASCII_DEL) {
            fprintf(out, "^?");
            return;
        }
        if (c >= 128 && c <= ASCII_CP1252_END) {
            fprintf(out, "M-^%c", (c - 128) + 64);
            return;
        }
        if (c == ASCII_255) {
            fprintf(out, "M-^?");
            return;
        }
        if (c >= ASCII_160) {
            fprintf(out, "M-%c", c - 128);
            return;
        }
    }

    fputc(c, out);
}

void output_line_visual(const char* line, const CatOptions* opts, FILE* out) {
    size_t len = strlen(line);
    int has_newline = (len > 0 && line[len - 1] == '\n');
    size_t content_len = has_newline ? len - 1 : len;

    int show_nonprinting = opts->show_nonprinting;
    int show_tabs = opts->show_tabs;
    int show_ends = opts->show_ends;

    if (show_nonprinting) {
        for (size_t j = 0; j < content_len; j++) {
            output_char_visual((unsigned char)line[j], show_tabs, 1, show_ends, 0, out);
        }
        if (has_newline) {
            if (show_ends) {
                fprintf(out, "$\n");
            } else {
                fprintf(out, "\n");
            }
        }
    } else if (show_tabs) {
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '\t') {
                fprintf(out, "^I");
            } else {
                fputc(line[j], out);
            }
        }
    } else if (show_ends && has_newline) {
        for (size_t j = 0; j < content_len; j++) {
            fputc(line[j], out);
        }
        fprintf(out, "$\n");
    } else {
        fputs(line, out);
        if (!has_newline) { fputc('\n', out); }
    }
}
