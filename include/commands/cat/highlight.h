#ifndef CAT_HIGHLIGHT_H
#define CAT_HIGHLIGHT_H

#include <stdio.h>

const char* get_file_extension(const char* path);
void print_highlighted(const char* line, const char* ext, FILE* out);

#endif
