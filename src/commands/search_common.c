#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>

#include "commands/search_common.h"

GRegex *search_compile_pattern(const char *pattern,
                                int word_regexp,
                                int line_regexp,
                                GRegexCompileFlags flags,
                                GError **error)
{
    gchar *effective_pattern = NULL;
    GRegex *re = NULL;

    if (line_regexp) {
        effective_pattern = g_strdup_printf("^(?:%s)$", pattern);
    } else if (word_regexp) {
        effective_pattern = g_strdup_printf(
            SEARCH_WORD_BOUNDARY_BEFORE "(?:%s)" SEARCH_WORD_BOUNDARY_AFTER,
            pattern);
    } else {
        effective_pattern = g_strdup(pattern);
    }

    re = g_regex_new(effective_pattern, flags, (GRegexMatchFlags)0, error);
    g_free(effective_pattern);
    return re;
}

int search_should_color(SearchColorMode mode)
{
    switch (mode) {
    case SEARCH_COLOR_ALWAYS:
        return 1;
    case SEARCH_COLOR_AUTO:
        return isatty(STDOUT_FILENO);
    default:
        return 0;
    }
}

// NOLINTNEXTLINE(misc-include-cleaner)
gboolean search_check_word_boundary(const gchar *haystack, gsize match_start,
                                      gsize match_end, gsize haystack_len)
{
    if (match_start > 0) {
        guchar prev = (guchar)haystack[match_start - 1];
        // NOLINTNEXTLINE(misc-include-cleaner)
        if (g_ascii_isalnum(prev) || prev == '_') {
            // NOLINTNEXTLINE(misc-include-cleaner)
            return FALSE;
        }
    }
    if (match_end < haystack_len) {
        guchar next = (guchar)haystack[match_end];
        // NOLINTNEXTLINE(misc-include-cleaner)
        if (g_ascii_isalnum(next) || next == '_') {
            // NOLINTNEXTLINE(misc-include-cleaner)
            return FALSE;
        }
    }
    // NOLINTNEXTLINE(misc-include-cleaner)
    return TRUE;
}

gboolean search_check_line_boundary(gsize match_start, gsize match_end,
                                     gsize line_len)
{
    return (match_start == 0 && match_end == line_len);
}

gboolean search_fixed_loop(const gchar *haystack, const gchar *pattern,
                            gsize haystack_len, gsize *match_start,
                            gsize *match_end,
                            int word_regexp, int line_regexp)
{
    gsize pat_len = strlen(pattern);
    const gchar *found = haystack;
    while ((found = strstr(found, pattern)) != NULL) {
        *match_start = (gsize)(found - haystack);
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
    return TRUE;
        }
        found++;
    }
    return FALSE;
}

gboolean search_match_fixed(const gchar *pattern, const gchar *line,
                             gsize line_len, int ignore_case,
                             int word_regexp, int line_regexp)
{
    if (ignore_case) {
        gchar *lower_line = g_utf8_strdown(line, (gssize)line_len); // NOLINT(misc-include-cleaner)
        gchar *lower_pat = g_utf8_strdown(pattern, -1);
        gsize ms = 0;
        gsize me = 0;
        gboolean found = search_fixed_loop(lower_line, lower_pat, line_len,
                                            &ms, &me, word_regexp, line_regexp);
        g_free(lower_line);
        g_free(lower_pat);
        return found;
    }
    gsize ms = 0;
    gsize me = 0;
    return search_fixed_loop(line, pattern, line_len,
                              &ms, &me, word_regexp, line_regexp);
}

void search_print_match(const gchar *line, gsize line_len, int show_ln,
                         int ln, const gchar *prefix, int use_color,
                         const GRegex *re, const gchar *pattern,
                         int only_matching, int is_fixed)
{
    if (only_matching) {
        if (is_fixed) {
            if (use_color) {
                printf("\033[01;31m%s\033[0m\n", pattern);
            } else {
                printf("%s\n", pattern);
            }
        } else {
            GMatchInfo *match_info;
            g_regex_match(re, line, (GRegexMatchFlags)0, &match_info);
            while (g_match_info_matches(match_info)) {
                gint start;
                gint end;
                g_match_info_fetch_pos(match_info, 0, &start, &end);
                int slen = (int)(end - start);
                if (use_color) {
                    printf("\033[01;31m%.*s\033[0m\n", slen, line + start);
                } else {
                    printf("%.*s\n", slen, line + start);
                }
                g_match_info_next(match_info, NULL);
            }
            g_match_info_free(match_info);
        }
        return;
    }

    if (prefix != NULL) {
        printf("%s:", prefix);
    }
    if (show_ln) {
        printf("%d:", ln);
    }

    if (use_color && !is_fixed && re != NULL) {
        GMatchInfo *match_info;
        g_regex_match(re, line, (GRegexMatchFlags)0, &match_info);
        gsize last_end = 0;
        while (g_match_info_matches(match_info)) {
            gint start;
            gint end;
            g_match_info_fetch_pos(match_info, 0, &start, &end);
            printf("%.*s\033[01;31m%.*s\033[0m",
                   (int)(start - (gint)last_end),
                   line + last_end, (int)(end - start), line + start);
            last_end = (gsize)end;
            g_match_info_next(match_info, NULL);
        }
        printf("%s\n", line + last_end);
        g_match_info_free(match_info);
    } else {
        printf("%.*s\n", (int)line_len, line);
    }
}
