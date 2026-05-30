#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <glib.h>

#include "commands/cat/blame.h"

#define BLAME_COMMIT_LEN 9
#define BLAME_SHA_LEN 40
#define BLAME_SHORT_COMMIT 7
#define BLAME_AUTHOR_PREFIX_LEN 7
#define BLAME_AUTHOR_TIME_PREFIX_LEN 12

BlameInfo* parse_blame(const char* path, int* out_count) {
    gchar *stdout_buf = NULL;
    GError *error = NULL;

    gchar *argv[] = {(gchar*)"git", (gchar*)"blame", (gchar*)"--porcelain", (gchar*)path, NULL};

    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, &stdout_buf, NULL, NULL, &error)) {
        if (error) { g_error_free(error); }
        *out_count = 0;
        return NULL;
    }

    if (!stdout_buf || stdout_buf[0] == '\0') {
        g_free(stdout_buf);
        *out_count = 0;
        return NULL;
    }

    // NOLINTNEXTLINE(misc-include-cleaner)
    GArray* infos = g_array_new(FALSE, TRUE, sizeof(BlameInfo));

    char *p = stdout_buf;
    char current_author[32] = "";
    char current_date[16] = "";
    char current_commit[BLAME_COMMIT_LEN] = "";
    int in_header = 0;

    while (*p) {
        if (*p == '\t') {
            BlameInfo info;
            g_strlcpy(info.commit, current_commit, BLAME_COMMIT_LEN);
            g_strlcpy(info.author, current_author, 32);
            g_strlcpy(info.date, current_date, 16);
            g_array_append_val(infos, info);
            in_header = 0;
            while (*p && *p != '\n') { p++; }
            if (*p == '\n') { p++; }
            continue;
        }

        char *end = strchr(p, '\n');
        size_t line_len = end ? (size_t)(end - p) : strlen(p);

        int is_sha = 0;
        if (line_len >= BLAME_SHA_LEN) {
            is_sha = 1;
            for (size_t k = 0; k < BLAME_SHA_LEN; k++) {
                if (!isxdigit((unsigned char)p[k])) { is_sha = 0; break; }
            }
        }

        if (is_sha) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            strncpy(current_commit, p, BLAME_SHORT_COMMIT);
            current_commit[BLAME_SHORT_COMMIT] = '\0';
            current_author[0] = '\0';
            current_date[0] = '\0';
            in_header = 1;
        } else if (in_header) {
            if (line_len > BLAME_AUTHOR_PREFIX_LEN && memcmp(p, "author ", BLAME_AUTHOR_PREFIX_LEN) == 0) {
                size_t name_len = line_len - BLAME_AUTHOR_PREFIX_LEN;
                if (name_len >= sizeof(current_author)) { name_len = sizeof(current_author) - 1; }
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                memcpy(current_author, p + BLAME_AUTHOR_PREFIX_LEN, name_len);
                current_author[name_len] = '\0';
            } else if (line_len > BLAME_AUTHOR_TIME_PREFIX_LEN && memcmp(p, "author-time ", BLAME_AUTHOR_TIME_PREFIX_LEN) == 0) {
                // NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion,cert-err34-c)
                time_t t = (time_t)atol(p + 12);
                struct tm *tm = localtime(&t);
                if (tm) {
                    (void)strftime(current_date, sizeof(current_date), "%Y-%m-%d", tm);
                }
            }
        }

        if (end) { p = end + 1; }
        else { break; }
    }

    g_free(stdout_buf);
    *out_count = (int)infos->len;
    return (BlameInfo*)g_array_free(infos, FALSE);
}

void free_blame(BlameInfo* blame, int count) {
    (void)count;
    g_free(blame);
}
