#ifndef CAT_HIGHLIGHT_HPP
#define CAT_HIGHLIGHT_HPP

#include <cstdio>

const char* get_file_extension(const char* path);
void print_highlighted(const char* line, const char* ext, FILE* out);

#endif
