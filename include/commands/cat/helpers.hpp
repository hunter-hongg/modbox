#ifndef CAT_HELPERS_HPP
#define CAT_HELPERS_HPP

#include <cstdio>
#include <string>
#include <vector>
#include "commands/cat.hpp"

struct PipelineLine {
    std::string text;
    int orig_index;
};

std::vector<PipelineLine*> read_file_to_lines(const char* path);
std::vector<PipelineLine*> read_stdin_to_lines(void);
void free_pipeline_lines(std::vector<PipelineLine*>* lines);
std::vector<PipelineLine*>* slice_range(std::vector<PipelineLine*>* lines, int start, int end);
std::vector<PipelineLine*>* slice_head(std::vector<PipelineLine*>* lines, int n);
std::vector<PipelineLine*>* slice_tail(std::vector<PipelineLine*>* lines, int n);
std::vector<PipelineLine*>* squeeze_blank_lines(std::vector<PipelineLine*>* lines);
std::vector<unsigned int>* find_matching_indices(std::vector<PipelineLine*>* lines, const char* pattern);
std::vector<unsigned int>* expand_indices(std::vector<PipelineLine*>* lines, std::vector<unsigned int>* match_indices, int context);
std::vector<PipelineLine*>* extract_lines(std::vector<PipelineLine*>* lines, std::vector<unsigned int>* indices);
void print_stats(std::vector<PipelineLine*>* lines, FILE* out);
void print_header(const char* path, FILE* out);
void format_line_number(int num, int format, FILE* out);
void output_line_visual(const char* line, const CatOptions* opts, FILE* out);

#endif
