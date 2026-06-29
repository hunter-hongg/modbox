#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "commands/cat/diff.hpp"

void run_diff(const char* file1, const char* file2, FILE* out) {
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "diff -u %s %s 2>/dev/null", file1, file2);

    FILE* fp = popen(cmd, "r");
    if (!fp) { return; }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        fwrite(buf, 1, n, out);
    }

    pclose(fp);
}
