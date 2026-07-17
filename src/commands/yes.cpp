#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands/yes.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [STRING]...\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Repeatedly output a line with all specified STRING(s), or 'y'.\n");
    printf("\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -V, --version  output version information and exit\n");
}

void yes_command(int argc, char** argv) {
    const char* prog = argv[0];
    std::vector<const char*> args;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(a, "-V") == 0 || strcmp(a, "--version") == 0) {
            printf("yes (modbox) 1.0\n");
            return;
        }
        args.push_back(a);
    }

    std::string line;
    if (args.empty()) {
        line = "y";
    } else {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) {
                line += ' ';
            }
            line += args[i];
        }
    }

    std::string buf = line + "\n";

    static char out[65536];
    size_t written = 0;
    while (true) {
        if (written + buf.size() > sizeof(out)) {
            size_t n = fwrite(out, 1, written, stdout);
            written -= n;
            memmove(out, out + n, written);
            if (ferror(stdout)) {
                break;
            }
        }
        memcpy(out + written, buf.data(), buf.size());
        written += buf.size();
    }
}
