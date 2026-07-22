#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <argtable3.h>

#include "commands/shuf.hpp"
#include "commands/command_macros.hpp"

static std::vector<std::string> read_lines(int file_count, const char** filenames) {
    std::vector<std::string> lines;
    if (file_count == 0) {
        char buf[8192];
        while (fgets(buf, (int)sizeof(buf), stdin)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }
            lines.push_back(std::string(buf));
        }
        return lines;
    }

    for (int i = 0; i < file_count; i++) {
        const char* fname = filenames[i];
        FILE* fp = stdin;
        if (strcmp(fname, "-") != 0) {
            fp = fopen(fname, "r");
            if (fp == nullptr) {
                fprintf(stderr, "shuf: %s: No such file or directory\n", fname);
                continue;
            }
        }

        char buf[8192];
        while (fgets(buf, (int)sizeof(buf), fp)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }
            lines.push_back(std::string(buf));
        }

        if (fp != stdin) {
            fclose(fp);
        }
    }

    return lines;
}

void shuf_command(int argc, char** argv) {
    struct arg_lit* echo_opt = arg_lit0("e", "echo", "treat each argument as an input line");
    struct arg_str* input_range_opt = arg_str0("i", "input-range", "LO-HI", "treat each number LO through HI as an input line");
    struct arg_int* head_count_opt = arg_int0("n", "head-count", "COUNT", "output at most COUNT lines");
    struct arg_lit* repeat_opt = arg_lit0("r", "repeat", "output lines can be repeated");
    struct arg_str* output_opt = arg_str0("o", "output", "FILE", "write result to FILE instead of standard output");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(nullptr, nullptr, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {echo_opt, input_range_opt, head_count_opt, repeat_opt, output_opt, help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Write a random permutation of the input lines to standard output.\n");
        printf("\n");
        printf("  -e, --echo                treat each argument as an input line\n");
        printf("  -i, --input-range=LO-HI   treat each number LO through HI as an input line\n");
        printf("  -n, --head-count=COUNT    output at most COUNT lines\n");
        printf("  -r, --repeat              output lines can be repeated\n");
        printf("  -o, --output=FILE         write result to FILE instead of standard output\n");
        printf("  -h, --help                display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (echo_opt->count > 0 && input_range_opt->count > 0) {
        fprintf(stderr, "shuf: cannot combine -e and -i\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (input_range_opt->count > 0 && file_arg->count > 0) {
        fprintf(stderr, "shuf: cannot combine -i with file arguments\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int repeat = (repeat_opt->count > 0);
    int head_count = (head_count_opt->count > 0) ? head_count_opt->ival[0] : -1;
    const char* output_file = (output_opt->count > 0) ? output_opt->sval[0] : nullptr;

    std::vector<std::string> lines;

    if (input_range_opt->count > 0) {
        const char* range = input_range_opt->sval[0];
        int lo = 0, hi = 0;
        if (sscanf(range, "%d-%d", &lo, &hi) != 2 || lo > hi) {
            fprintf(stderr, "shuf: invalid input range: %s\n", range);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        lines.reserve((size_t)(hi - lo + 1));
        for (int i = lo; i <= hi; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", i);
            lines.push_back(std::string(buf));
        }
    } else if (echo_opt->count > 0) {
        for (int i = 0; i < file_arg->count; i++) {
            lines.push_back(std::string(file_arg->filename[i]));
        }
        if (lines.empty()) {
            fprintf(stderr, "shuf: no input lines\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    } else {
        int file_count = file_arg->count;
        const char** filenames = (const char**)(file_arg->filename);
        lines = read_lines(file_count, filenames);
    }

    if (lines.empty()) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    FILE* out_fp = stdout;
    if (output_file) {
        out_fp = fopen(output_file, "w");
        if (out_fp == nullptr) {
            fprintf(stderr, "shuf: %s: Cannot open for writing: %s\n", output_file, strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());

    if (repeat) {
        std::uniform_int_distribution<size_t> dist(0, lines.size() - 1);
        if (head_count >= 0) {
            for (int i = 0; i < head_count; i++) {
                fprintf(out_fp, "%s\n", lines[dist(g)].c_str());
            }
        } else {
            while (true) {
                fprintf(out_fp, "%s\n", lines[dist(g)].c_str());
            }
        }
    } else {
        std::shuffle(lines.begin(), lines.end(), g);
        int count = (head_count >= 0 && head_count < (int)lines.size()) ? head_count : (int)lines.size();
        for (int i = 0; i < count; i++) {
            fprintf(out_fp, "%s\n", lines[i].c_str());
        }
    }

    if (out_fp != stdout) {
        fclose(out_fp);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("shuf", shuf_command, "Shuffle lines");
