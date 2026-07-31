#include "commands/cmd_error.hpp"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>

int cmd_perror(const char* prog, const char* path) {
    fprintf(stderr, "%s: %s: %s\n", prog, path, strerror(errno));
    return 1;
}

int cmd_error(const char* prog, const char* fmt, ...) {
    fprintf(stderr, "%s: ", prog);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    return 1;
}
