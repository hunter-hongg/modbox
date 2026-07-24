#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <string>
#include <vector>
#include <argtable3.h>

#include "commands/sum.hpp"
#include "commands/command_macros.hpp"

namespace {

static uint32_t sum_checksum16(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
        if (sum & 0x10000) {
            sum = (sum & 0xFFFF) + 1;
        }
    }
    return sum & 0xFFFF;
}

static uint32_t sum_checksum32(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static void sum_file(FILE* in, const char* filename, bool bsd_mode) {
    uint8_t buf[4096];
    uint32_t checksum = 0;
    unsigned long total_bytes = 0;

    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (n == 0) break;
        if (bsd_mode) {
            checksum = sum_checksum32(buf, n);
        } else {
            checksum = sum_checksum16(buf, n);
        }
        total_bytes += n;
    }

    if (ferror(in)) {
        fprintf(stderr, "sum: %s: read error: %s\n",
                filename ? filename : "-", strerror(errno));
        return;
    }

    unsigned long blocks = (total_bytes + 511) / 512;

    if (bsd_mode) {
        printf("%u %lu", checksum, blocks);
    } else {
        printf("%u %lu", checksum, blocks);
    }

    if (filename) {
        printf(" %s", filename);
    }
    printf("\n");
}

}

void sum_command(int argc, char** argv) {
    struct arg_lit* sysv_opt = arg_lit0(NULL, "sysv", "System V sum format (default)");
    struct arg_lit* bsd_opt = arg_lit0(NULL, "bsd", "BSD sum format");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "files to checksum");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {sysv_opt, bsd_opt, help_opt, files_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Print checksum and block count for each FILE.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -s, --sysv   use System V sum format (default)\n");
        printf("  -r, --bsd    use BSD sum format (32-bit checksum)\n");
        printf("  -h, --help   display this help and exit\n");
        printf("\n");
        printf("Output format: CHECKSUM BLOCKS FILE\n");
        printf("Block size is 512 bytes.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool bsd_mode = (bsd_opt->count > 0);

    if (files_arg->count == 0) {
        sum_file(stdin, nullptr, bsd_mode);
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            const char* filename = files_arg->filename[i];
            if (strcmp(filename, "-") == 0) {
                sum_file(stdin, nullptr, bsd_mode);
            } else {
                FILE* in = fopen(filename, "rb");
                if (!in) {
                    fprintf(stderr, "sum: %s: %s\n", filename, strerror(errno));
                    continue;
                }
                sum_file(in, filename, bsd_mode);
                fclose(in);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("sum", sum_command, "Print checksum and block count");
