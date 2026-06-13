#ifndef SEARCH_COMMON_H
#define SEARCH_COMMON_H

#include <glib.h>

typedef enum {
    SEARCH_COLOR_NEVER,
    SEARCH_COLOR_ALWAYS,
    SEARCH_COLOR_AUTO
} SearchColorMode;

#define SEARCH_REGEX_FLAGS_DEFAULT (G_REGEX_OPTIMIZE)
#define SEARCH_REGEX_FLAGS_CASELESS (G_REGEX_CASELESS | G_REGEX_OPTIMIZE)

#define SEARCH_WORD_BOUNDARY_BEFORE "(?<![[:alnum:]_])"
#define SEARCH_WORD_BOUNDARY_AFTER "(?![[:alnum:]_])"

GRegex *search_compile_pattern(const char *pattern,
                                int word_regexp,
                                int line_regexp,
                                GRegexCompileFlags flags,
                                GError **error);

int search_should_color(SearchColorMode mode);

gboolean search_check_word_boundary(const gchar *haystack, gsize match_start,
                                     gsize match_end, gsize haystack_len);

gboolean search_check_line_boundary(gsize match_start, gsize match_end,
                                     gsize line_len);

gboolean search_fixed_loop(const gchar *haystack, const gchar *pattern,
                            gsize haystack_len, gsize *match_start,
                            gsize *match_end,
                            int word_regexp, int line_regexp);

gboolean search_match_fixed(const gchar *pattern, const gchar *line,
                             gsize line_len, int ignore_case,
                             int word_regexp, int line_regexp);

void search_print_match(const gchar *line, gsize line_len, int show_ln,
                         int ln, const gchar *prefix, int use_color,
                         const GRegex *re, const gchar *pattern,
                         int only_matching, int is_fixed);

#endif
