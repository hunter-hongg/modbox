#include <cstdio>
#include <cstring>
#include <cctype>
#include <regex>
#include <string>
#include <unistd.h>

#include "commands/search_common.hpp"

std::regex search_compile_pattern(const std::string& pattern,
                                  int word_regexp,
                                  int line_regexp,
                                  std::regex::flag_type flags) {
    // Note: word_regexp word boundaries are checked manually by callers
    // because std::regex does not support PCRE lookahead/lookbehind.
    std::string effective;
    if (line_regexp) {
        effective = "^(?:" + pattern + ")$";
    } else {
        effective = pattern;
    }
    return std::regex(effective, flags);
}

int search_should_color(SearchColorMode mode) {
    switch (mode) {
    case SearchColorMode::ALWAYS:
        return 1;
    case SearchColorMode::AUTO:
        return isatty(STDOUT_FILENO);
    default:
        return 0;
    }
}

bool search_check_word_boundary(const char* haystack, std::size_t match_start,
                                std::size_t match_end, std::size_t haystack_len) {
    if (match_start > 0) {
        unsigned char prev = (unsigned char)haystack[match_start - 1];
        if (std::isalnum(prev) || prev == '_') {
            return false;
        }
    }
    if (match_end < haystack_len) {
        unsigned char next = (unsigned char)haystack[match_end];
        if (std::isalnum(next) || next == '_') {
            return false;
        }
    }
    return true;
}

bool search_check_line_boundary(std::size_t match_start, std::size_t match_end,
                                std::size_t line_len) {
    return (match_start == 0 && match_end == line_len);
}

bool search_fixed_loop(const char* haystack, const char* pattern,
                       std::size_t haystack_len, std::size_t* match_start,
                       std::size_t* match_end,
                       int word_regexp, int line_regexp) {
    std::size_t pat_len = strlen(pattern);
    const char* found = haystack;
    while ((found = strstr(found, pattern)) != nullptr) {
        *match_start = (std::size_t)(found - haystack);
        *match_end = *match_start + pat_len;
        int ok = 1;
        if (word_regexp) {
            ok = ok && search_check_word_boundary(haystack, *match_start,
                                                   *match_end, haystack_len);
        }
        if (line_regexp) {
            ok = ok && search_check_line_boundary(*match_start, *match_end,
                                                   haystack_len);
        }
        if (ok) {
            return true;
        }
        found++;
    }
    return false;
}

bool search_match_fixed(const char* pattern, const char* line,
                        std::size_t line_len, int ignore_case,
                        int word_regexp, int line_regexp) {
    if (ignore_case) {
        // Manual ASCII-only tolower for comparison (no UTF-8 dependency)
        std::string lower_line(line, line_len);
        std::string lower_pat(pattern);
        for (auto& ch : lower_line) {
            ch = (char)std::tolower((unsigned char)ch);
        }
        for (auto& ch : lower_pat) {
            ch = (char)std::tolower((unsigned char)ch);
        }
        std::size_t ms = 0;
        std::size_t me = 0;
        return search_fixed_loop(lower_line.c_str(), lower_pat.c_str(), line_len,
                                  &ms, &me, word_regexp, line_regexp);
    }
    std::size_t ms = 0;
    std::size_t me = 0;
    return search_fixed_loop(line, pattern, line_len,
                              &ms, &me, word_regexp, line_regexp);
}

void search_print_match(const char* line, std::size_t line_len, int show_ln,
                         int ln, const char* prefix, int use_color,
                         const std::regex* re, const std::string& pattern,
                         int only_matching, int is_fixed,
                         int word_regexp)
{
    if (only_matching) {
        if (is_fixed) {
            if (use_color) {
                printf("\033[01;31m%s\033[0m\n", pattern.c_str());
            } else {
                printf("%s\n", pattern.c_str());
            }
        } else if (re) {
            std::string s(line, line_len);
            std::smatch m;
            std::string::const_iterator search_start(s.cbegin());
            while (std::regex_search(search_start, s.cend(), m, *re)) {
                std::size_t abs_pos = (std::size_t)(m.position(0) + (search_start - s.cbegin()));
                if (!word_regexp || search_check_word_boundary(line, abs_pos, abs_pos + m.length(0), line_len)) {
                    if (use_color)
                        printf("\033[01;31m%.*s\033[0m\n", (int)m.length(0), line + m.position(0));
                    else
                        printf("%.*s\n", (int)m.length(0), line + m.position(0));
                }
                search_start = m.suffix().first;
            }
        }
        return;
    }

    if (prefix != nullptr) {
        printf("%s:", prefix);
    }
    if (show_ln) {
        printf("%d:", ln);
    }

    if (use_color && !is_fixed && re != nullptr) {
        std::string s(line, line_len);
        std::smatch m;
        std::string::const_iterator search_start(s.cbegin());
        std::size_t last_end = 0;
        while (std::regex_search(search_start, s.cend(), m, *re)) {
            std::size_t abs_pos = (std::size_t)(m.position(0) + (search_start - s.cbegin()));
            if (word_regexp && !search_check_word_boundary(line, abs_pos, abs_pos + m.length(0), line_len)) {
                // Skip this match - doesn't satisfy word boundary
                printf("%.*s", (int)m.length(0), line + m.position(0));
                last_end = abs_pos + m.length(0);
                search_start = m.suffix().first;
                continue;
            }
            // Print text between matches in plain
            printf("%.*s", (int)(abs_pos - last_end), line + last_end);
            // Print match in color
            printf("\033[01;31m%.*s\033[0m", (int)m.length(0), line + m.position(0));
            last_end = abs_pos + m.length(0);
            search_start = m.suffix().first;
        }
        printf("%s\n", line + last_end);
    } else {
        printf("%.*s\n", (int)line_len, line);
    }
}
