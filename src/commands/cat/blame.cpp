#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <vector>

#include "commands/cat/blame.hpp"

#define BLAME_COMMIT_LEN 9
#define BLAME_SHA_LEN 40
#define BLAME_SHORT_COMMIT 7
#define BLAME_AUTHOR_PREFIX_LEN 7
#define BLAME_AUTHOR_TIME_PREFIX_LEN 12

BlameInfo* parse_blame(const char* path, int* out_count) {
    // Use popen to run git blame
    char cmd[4096];
    (void)snprintf(cmd, sizeof(cmd), "git blame --porcelain %s 2>/dev/null", path);

    FILE* fp = popen(cmd, "r");
    if (fp == nullptr) {
        *out_count = 0;
        return NULL;
    }

    // Read all output into memory
    std::vector<char> buf;
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        buf.insert(buf.end(), tmp, tmp + n);
    }
    int const status = pclose(fp);
    if (status != 0 || buf.empty()) {
        *out_count = 0;
        return NULL;
    }
    buf.push_back('\0');

    std::vector<BlameInfo> infos;
    char* p = buf.data();
    char current_author[32] = "";
    char current_date[16] = "";
    char current_commit[BLAME_COMMIT_LEN] = "";
    int in_header = 0;

    while ((*p) != 0) {
        if (*p == '\t') {
            BlameInfo info;
            strncpy(info.commit, current_commit, BLAME_COMMIT_LEN - 1);
            info.commit[BLAME_COMMIT_LEN - 1] = '\0';
            strncpy(info.author, current_author, sizeof(info.author) - 1);
            info.author[sizeof(info.author) - 1] = '\0';
            strncpy(info.date, current_date, sizeof(info.date) - 1);
            info.date[sizeof(info.date) - 1] = '\0';
            infos.push_back(info);
            in_header = 0;
            while (((*p) != 0) && *p != '\n') { p++; }
            if (*p == '\n') { p++; }
            continue;
        }

        char* end = strchr(p, '\n');
        size_t const line_len = (end != nullptr) ? static_cast<size_t>(end - p) : strlen(p);

        int is_sha = 0;
        if (line_len >= BLAME_SHA_LEN) {
            is_sha = 1;
            for (size_t k = 0; k < BLAME_SHA_LEN; k++) {
                if (isxdigit(static_cast<unsigned char>(p[k])) == 0) { is_sha = 0; break; }
            }
        }

        if (is_sha != 0) {
            strncpy(current_commit, p, BLAME_SHORT_COMMIT);
            current_commit[BLAME_SHORT_COMMIT] = '\0';
            current_author[0] = '\0';
            current_date[0] = '\0';
            in_header = 1;
        } else if (in_header != 0) {
            if (line_len > BLAME_AUTHOR_PREFIX_LEN && memcmp(p, "author ", BLAME_AUTHOR_PREFIX_LEN) == 0) {
                size_t name_len = line_len - BLAME_AUTHOR_PREFIX_LEN;
                if (name_len >= sizeof(current_author)) { name_len = sizeof(current_author) - 1; }
                memcpy(current_author, p + BLAME_AUTHOR_PREFIX_LEN, name_len);
                current_author[name_len] = '\0';
            } else if (line_len > BLAME_AUTHOR_TIME_PREFIX_LEN && memcmp(p, "author-time ", BLAME_AUTHOR_TIME_PREFIX_LEN) == 0) {
                time_t const t = static_cast<time_t>(atol(p + 12));
                const struct tm* tm = localtime(&t);
                if (tm != nullptr) {
                    (void)strftime(current_date, sizeof(current_date), "%Y-%m-%d", tm);
                }
            }
        }

        if (end != nullptr) { p = end + 1; }
        else { break; }
    }

    *out_count = static_cast<int>(infos.size());

    // Copy to heap-allocated array
    BlameInfo* result = static_cast<BlameInfo*>(malloc(static_cast<size_t>(*out_count) * sizeof(BlameInfo)));
    if (result != nullptr) {
        memcpy(result, infos.data(), static_cast<size_t>(*out_count) * sizeof(BlameInfo));
    }
    return result;
}

void free_blame(BlameInfo* blame, int count) {
    (void)count;
    free(blame);
}
