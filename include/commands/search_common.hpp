#ifndef SEARCH_COMMON_HPP
#define SEARCH_COMMON_HPP

#include <cstddef>
#include <regex>
#include <string>

enum class SearchColorMode { NEVER, ALWAYS, AUTO };

#define SEARCH_WORD_BOUNDARY_BEFORE "(?<![[:alnum:]_])"
#define SEARCH_WORD_BOUNDARY_AFTER "(?![[:alnum:]_])"

std::regex search_compile_pattern(const std::string& pattern,
                                  int word_regexp,
                                  int line_regexp,
                                  std::regex::flag_type flags);

int search_should_color(SearchColorMode mode);

bool search_check_word_boundary(const char* haystack, std::size_t match_start,
                                std::size_t match_end, std::size_t haystack_len);

bool search_check_line_boundary(std::size_t match_start, std::size_t match_end,
                                std::size_t line_len);

bool search_fixed_loop(const char* haystack, const char* pattern,
                       std::size_t haystack_len, std::size_t* match_start,
                       std::size_t* match_end,
                       int word_regexp, int line_regexp);

bool search_match_fixed(const char* pattern, const char* line,
                        std::size_t line_len, int ignore_case,
                        int word_regexp, int line_regexp);

void search_print_match(const char* line, std::size_t line_len, int show_ln,
                        int ln, const char* prefix, int use_color,
                        const std::regex* re, const std::string& pattern,
                        int only_matching, int is_fixed,
                        int word_regexp = 0);

#endif