#ifndef INPUT_FILES_HPP
#define INPUT_FILES_HPP

#include <cstdio>
#include <functional>

// Invokes fn(stream, display_name) for each FILE argument; "-" or an empty
// list means stdin (display name "-"). On open failure prints
// "prog: path: <strerror(errno)>" and continues with the next file.
// Returns 0 if every file opened and every fn call returned 0, else 1.
int for_each_input(const char* prog, const char* const* files, int nfiles,
                   const std::function<int(FILE*, const char*)>& fn);

#endif
