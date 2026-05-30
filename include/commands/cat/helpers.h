#ifndef CAT_HELPERS_H
#define CAT_HELPERS_H

#include <glib.h>
#include <stdio.h>
#include "commands/cat.h"

typedef struct {
    char* text;
    int orig_index;
} PipelineLine;

GPtrArray* read_file_to_lines(const char* path);
GPtrArray* read_stdin_to_lines(void);
void free_pipeline_lines(GPtrArray* lines);
GPtrArray* slice_range(GPtrArray* lines, int start, int end);
GPtrArray* slice_head(GPtrArray* lines, int n);
GPtrArray* slice_tail(GPtrArray* lines, int n);
GPtrArray* squeeze_blank_lines(GPtrArray* lines);
GArray* find_matching_indices(GPtrArray* lines, const char* pattern);
GArray* expand_indices(GPtrArray* lines, GArray* match_indices, int context);
GPtrArray* extract_lines(GPtrArray* lines, GArray* indices);
void print_stats(GPtrArray* lines, FILE* out);
void print_header(const char* path, FILE* out);
void format_line_number(int num, int format, FILE* out);
void output_line_visual(const char* line, const CatOptions* opts, FILE* out);

#endif
