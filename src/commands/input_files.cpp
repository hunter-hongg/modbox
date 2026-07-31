#include "commands/input_files.hpp"

#include <cstring>

#include "commands/cmd_error.hpp"

int for_each_input(const char* prog, const char* const* files, int nfiles,
                   const std::function<int(FILE*, const char*)>& fn) {
    int status = 0;
    if (nfiles == 0) {
        if (fn(stdin, "-") != 0) {
            status = 1;
        }
        return status;
    }
    for (int i = 0; i < nfiles; i++) {
        const char* path = files[i];
        if (strcmp(path, "-") == 0) {
            if (fn(stdin, "-") != 0) {
                status = 1;
            }
            continue;
        }
        FILE* fp = fopen(path, "rb");
        if (!fp) {
            cmd_perror(prog, path);
            status = 1;
            continue;
        }
        if (fn(fp, path) != 0) {
            status = 1;
        }
        fclose(fp);
    }
    return status;
}
