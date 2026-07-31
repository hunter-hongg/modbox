#ifndef CMD_ERROR_HPP
#define CMD_ERROR_HPP

// Prints "prog: path: <strerror(errno)>" to stderr. Returns 1.
int cmd_perror(const char* prog, const char* path);

// Prints "prog: <formatted message>" to stderr. Returns 1.
int cmd_error(const char* prog, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif
