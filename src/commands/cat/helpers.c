#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <regex.h>
#include <glib.h>

#include "commands/cat/helpers.h"
#include "commands/cat.h"

#define HEADER_BANNER_WIDTH 70
#define MODE_STR_LEN 11

#define ASCII_DEL 127
#define ASCII_CP1252_END 159
#define ASCII_160 160
#define ASCII_255 255

static void free_pipeline_line(gpointer data) {
    PipelineLine* pl = (PipelineLine*)data;
    g_free(pl->text);
    g_free(pl);
}

static PipelineLine* pipeline_line_copy(PipelineLine* orig) {
    PipelineLine* copy = g_malloc(sizeof(PipelineLine));
    copy->text = g_strdup(orig->text);
    copy->orig_index = orig->orig_index;
    return copy;
}

GPtrArray* read_file_to_lines(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) { return NULL; }

    GPtrArray* lines = g_ptr_array_new_with_free_func(free_pipeline_line);
    char buf[4096];
    int line_idx = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        PipelineLine* pl = g_malloc(sizeof(PipelineLine));
        pl->text = g_strdup(buf);
        pl->orig_index = line_idx++;
        g_ptr_array_add(lines, pl);
    }

    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fp);
    return lines;
}

GPtrArray* read_stdin_to_lines(void) {
    GPtrArray* lines = g_ptr_array_new_with_free_func(free_pipeline_line);
    char buf[4096];
    int line_idx = 0;

    while (fgets(buf, sizeof(buf), stdin)) {
        PipelineLine* pl = g_malloc(sizeof(PipelineLine));
        pl->text = g_strdup(buf);
        pl->orig_index = line_idx++;
        g_ptr_array_add(lines, pl);
    }

    return lines;
}

void free_pipeline_lines(GPtrArray* lines) {
    if (lines) { g_ptr_array_unref(lines); }
}

GPtrArray* slice_range(GPtrArray* lines, int start, int end) {
    GPtrArray* result = g_ptr_array_new_with_free_func(free_pipeline_line);
    // NOLINTNEXTLINE(misc-include-cleaner)
    int s = MAX(0, start - 1);
    // NOLINTNEXTLINE(misc-include-cleaner)
    int e = (end > 0) ? MIN((int)lines->len, end) : (int)lines->len;
    if (s >= e) { return result; }
    for (int i = s; i < e; i++) {
        g_ptr_array_add(result, pipeline_line_copy((PipelineLine*)g_ptr_array_index(lines, i)));
    }
    return result;
}

GPtrArray* slice_head(GPtrArray* lines, int n) {
    GPtrArray* result = g_ptr_array_new_with_free_func(free_pipeline_line);
    int limit = MIN(n, (int)lines->len);
    for (int i = 0; i < limit; i++) {
        g_ptr_array_add(result, pipeline_line_copy((PipelineLine*)g_ptr_array_index(lines, i)));
    }
    return result;
}

GPtrArray* slice_tail(GPtrArray* lines, int n) {
    GPtrArray* result = g_ptr_array_new_with_free_func(free_pipeline_line);
    int start = MAX(0, (int)lines->len - n);
    for (int i = start; i < (int)lines->len; i++) {
        g_ptr_array_add(result, pipeline_line_copy((PipelineLine*)g_ptr_array_index(lines, i)));
    }
    return result;
}

GPtrArray* squeeze_blank_lines(GPtrArray* lines) {
    GPtrArray* result = g_ptr_array_new_with_free_func(free_pipeline_line);
    int prev_blank = 0;
    for (unsigned int i = 0; i < lines->len; i++) {
        PipelineLine* pl = (PipelineLine*)g_ptr_array_index(lines, i);
        int blank = (pl->text[0] == '\n');
        if (blank && prev_blank) { continue; }
        prev_blank = blank;
        g_ptr_array_add(result, pipeline_line_copy(pl));
    }
    return result;
}

GArray* find_matching_indices(GPtrArray* lines, const char* pattern) {
    // NOLINTNEXTLINE(misc-include-cleaner)
    GArray* indices = g_array_new(FALSE, FALSE, sizeof(int));
    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) { return indices; }

    for (unsigned int i = 0; i < lines->len; i++) {
        PipelineLine* pl = (PipelineLine*)g_ptr_array_index(lines, i);
        if (regexec(&regex, pl->text, 0, NULL, 0) == 0) {
            g_array_append_val(indices, i);
        }
    }

    regfree(&regex);
    return indices;
}

GArray* expand_indices(GPtrArray* lines, GArray* match_indices, int context) {
    GArray* expanded = g_array_new(FALSE, FALSE, sizeof(int));
    int total_lines = (int)lines->len;
    if (total_lines == 0) { return expanded; }

    int last_added = -1;

    for (unsigned int i = 0; i < match_indices->len; i++) {
        // NOLINTNEXTLINE(bugprone-casting-through-void)
        int idx = g_array_index(match_indices, int, i);
        int start = MAX(0, idx - context);
        int end = MIN(total_lines - 1, idx + context);

        for (int j = start; j <= end; j++) {
            if (j > last_added) {
                g_array_append_val(expanded, j);
                last_added = j;
            }
        }
    }

    return expanded;
}

GPtrArray* extract_lines(GPtrArray* lines, GArray* indices) {
    GPtrArray* result = g_ptr_array_new_with_free_func(free_pipeline_line);
    for (unsigned int i = 0; i < indices->len; i++) {
        // NOLINTNEXTLINE(bugprone-casting-through-void)
        int idx = g_array_index(indices, int, i);
        g_ptr_array_add(result, pipeline_line_copy((PipelineLine*)g_ptr_array_index(lines, idx)));
    }
    return result;
}

void print_stats(GPtrArray* lines, FILE* out) {
    int line_count = (int)lines->len;
    int word_count = 0;
    int char_count = 0;

    for (unsigned int i = 0; i < lines->len; i++) {
        PipelineLine* pl = (PipelineLine*)g_ptr_array_index(lines, i);
        char_count += (int)strlen(pl->text);

        int in_word = 0;
        for (const char* p = pl->text; *p; p++) {
            if (*p == ' ' || *p == '\t' || *p == '\n') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                word_count++;
            }
        }
    }

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out, "  %d lines  %d words  %d characters\n", line_count, word_count, char_count);
}

void print_header(const char* path, FILE* out) {
    struct stat st;
    if (stat(path, &st) != 0) { return; }

    char timebuf[64];
    struct tm* tm = localtime(&st.st_mtime);
    if (!tm) { return; }
    (void)strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    char modebuf[MODE_STR_LEN] = "----------";
    if (S_ISREG(st.st_mode)) { modebuf[0] = '-'; }
    else if (S_ISDIR(st.st_mode)) { modebuf[0] = 'd'; }
    else if (S_ISLNK(st.st_mode)) { modebuf[0] = 'l'; }
    if (st.st_mode & S_IRUSR) { modebuf[1] = 'r'; }
    if (st.st_mode & S_IWUSR) { modebuf[2] = 'w'; }
    if (st.st_mode & S_IXUSR) { modebuf[3] = 'x'; }
    if (st.st_mode & S_IRGRP) { modebuf[4] = 'r'; }
    // NOLINTBEGIN(readability-magic-numbers)
    if (st.st_mode & S_IWGRP) { modebuf[5] = 'w'; }
    if (st.st_mode & S_IXGRP) { modebuf[6] = 'x'; }
    if (st.st_mode & S_IROTH) { modebuf[7] = 'r'; }
    if (st.st_mode & S_IWOTH) { modebuf[8] = 'w'; }
    if (st.st_mode & S_IXOTH) { modebuf[9] = 'x'; }
    // NOLINTEND(readability-magic-numbers)

    char sizestr[32];
    // NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    if (st.st_size < 1024) {
        (void)snprintf(sizestr, sizeof(sizestr), "%ld B", (long)st.st_size);
    } else if (st.st_size < (off_t)1024 * 1024) {
        (void)snprintf(sizestr, sizeof(sizestr), "%.1f KiB", (double)st.st_size / 1024);
    } else {
        (void)snprintf(sizestr, sizeof(sizestr), "%.1f MiB", (double)st.st_size / ((off_t)1024 * 1024));
    }
    // NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    int pathlen = (int)strlen(path);
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out, "-- %s ", path);
    for (int i = 4 + pathlen; i < HEADER_BANNER_WIDTH; i++) { (void)fputc('-', out); }
    (void)fputc('\n', out);
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out, "  Mode: %s   Size: %s   Modified: %s\n", modebuf, sizestr, timebuf);
    for (int i = 0; i < HEADER_BANNER_WIDTH; i++) { (void)fputc('-', out); }
    (void)fputc('\n', out);
}

void format_line_number(int num, int format, FILE* out) {
    switch (format) {
        // NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        case 1: (void)fprintf(out, "0x%04x  ", num); break;
        case 2: (void)fprintf(out, "%06o  ", num); break;
        default: (void)fprintf(out, "%6d  ", num); break;
        // NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    }
}

static void output_char_visual(unsigned char c, int show_tabs, int show_nonprinting, int show_ends, int is_last, FILE* out) {
    if (show_tabs && c == '\t') {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out, "^I");
        return;
    }

    if (show_nonprinting) {
        if (c == '\n' || c == '\t') {
            if (c == '\n' && show_ends && is_last) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out, "$\n");
                return;
            }
            (void)fputc(c, out);
            return;
        }
        if (c < 32) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "^%c", c + 64);
            return;
        }
        if (c == ASCII_DEL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "^?");
            return;
        }
        if (c >= 128 && c <= ASCII_CP1252_END) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "M-^%c", (c - 128) + 64);
            return;
        }
        if (c == ASCII_255) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "M-^?");
            return;
        }
        if (c >= ASCII_160) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out, "M-%c", c - 128);
            return;
        }
    }

    (void)fputc(c, out);
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
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out, "$\n");
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out, "\n");
            }
        }
    } else if (show_tabs) {
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '\t') {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out, "^I");
            } else {
                (void)fputc(line[j], out);
            }
        }
    } else if (show_ends && has_newline) {
        for (size_t j = 0; j < content_len; j++) {
            (void)fputc(line[j], out);
        }
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out, "$\n");
    } else {
        (void)fputs(line, out);
        if (!has_newline) { (void)fputc('\n', out); }
    }
}
