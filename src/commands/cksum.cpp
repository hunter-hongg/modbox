#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "commands/cksum.hpp"
#include "commands/command_macros.hpp"

static uint32_t crc32_posix_bit(const uint8_t* data, size_t len, uint32_t crc) {
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint32_t)data[i]) << 24;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ 0x04C11DB7;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void cksum_file(FILE* in, const char* filename, bool verbose) {
    uint32_t crc = 0;
    unsigned long total_bytes = 0;
    uint8_t buf[4096];

    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (n == 0) break;
        crc = crc32_posix_bit(buf, n, crc);
        total_bytes += n;
    }

    if (ferror(in)) {
        fprintf(stderr, "cksum: %s: read error: %s\n",
                filename ? filename : "-", strerror(errno));
        return;
    }

    // POSIX requires CRC of data, then CRC of length (variable bytes, LSB first)
    unsigned long remaining = total_bytes;
    while (remaining != 0) {
        uint8_t c = (uint8_t)(remaining & 0xFF);
        remaining >>= 8;
        crc = crc32_posix_bit(&c, 1, crc);
    }
    crc ^= 0xFFFFFFFF;

    if (filename) {
        printf("%u %lu %s\n", crc, total_bytes, filename);
    } else {
        printf("%u %lu\n", crc, total_bytes);
    }
}

void cksum_command(int argc, char** argv) {
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "output a diagnostic for every file processed");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 1000, "files to checksum");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose_opt, help_opt, files_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Display CRC checksum and byte count of each FILE.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -v, --verbose   output a diagnostic for every file processed\n");
        printf("  -h, --help      display this help and exit\n");
        printf("\n");
        printf("The output format is:\n");
        printf("  CRC byte_count FILE_NAME\n");
        printf("or:\n");
        printf("  CRC byte_count -\n");
        printf("for standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool verbose = (verbose_opt->count > 0);

    if (files_arg->count == 0) {
        // Read from stdin
        cksum_file(stdin, nullptr, verbose);
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            const char* filename = files_arg->filename[i];
            if (strcmp(filename, "-") == 0) {
                cksum_file(stdin, nullptr, verbose);
            } else {
                FILE* in = fopen(filename, "rb");
                if (!in) {
                    fprintf(stderr, "cksum: %s: No such file or directory\n", filename);
                    continue;
                }
                cksum_file(in, filename, verbose);
                fclose(in);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("cksum", cksum_command, "CRC checksum and byte count");
