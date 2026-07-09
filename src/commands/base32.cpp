#include <cstdio>
#include <cstring>
#include <cstdint>
#include <argtable3.h>
#include "commands/base32.hpp"

static const char base32_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static const int8_t decode_table[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static void base32_encode(FILE* in, FILE* out, int wrap_cols) {
    uint8_t buf[5];
    int col = 0;

    while (true) {
        size_t n = fread(buf, 1, 5, in);
        if (n == 0) break;

        if (n < 5) {
            for (size_t i = n; i < 5; i++) buf[i] = 0;
        }

        uint8_t out_chars[8];
        out_chars[0] = base32_table[buf[0] >> 3];
        out_chars[1] = base32_table[((buf[0] & 0x07) << 2) | (buf[1] >> 6)];

        int out_len;
        if (n == 1) {
            out_chars[2] = '=';
            out_chars[3] = '=';
            out_chars[4] = '=';
            out_chars[5] = '=';
            out_chars[6] = '=';
            out_chars[7] = '=';
            out_len = 8;
        } else if (n == 2) {
            out_chars[2] = base32_table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = base32_table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = '=';
            out_chars[5] = '=';
            out_chars[6] = '=';
            out_chars[7] = '=';
            out_len = 8;
        } else if (n == 3) {
            out_chars[2] = base32_table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = base32_table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = base32_table[((buf[2] & 0x0F) << 1) | (buf[3] >> 7)];
            out_chars[5] = '=';
            out_chars[6] = '=';
            out_chars[7] = '=';
            out_len = 8;
        } else if (n == 4) {
            out_chars[2] = base32_table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = base32_table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = base32_table[((buf[2] & 0x0F) << 1) | (buf[3] >> 7)];
            out_chars[5] = base32_table[(buf[3] & 0x7C) >> 2];
            out_chars[6] = base32_table[((buf[3] & 0x03) << 3) | (buf[4] >> 5)];
            out_chars[7] = '=';
            out_len = 8;
        } else {
            out_chars[2] = base32_table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = base32_table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = base32_table[((buf[2] & 0x0F) << 1) | (buf[3] >> 7)];
            out_chars[5] = base32_table[(buf[3] & 0x7C) >> 2];
            out_chars[6] = base32_table[((buf[3] & 0x03) << 3) | (buf[4] >> 5)];
            out_chars[7] = base32_table[buf[4] & 0x1F];
            out_len = 8;
        }

        for (int i = 0; i < out_len; i++) {
            fputc(out_chars[i], out);
            if (wrap_cols > 0) {
                col++;
                if (col >= wrap_cols) {
                    fputc('\n', out);
                    col = 0;
                }
            }
        }
    }

    if (wrap_cols > 0 && col > 0) {
        fputc('\n', out);
    }
}

static bool base32_decode(FILE* in, FILE* out, bool ignore_garbage) {
    uint8_t buf[8];
    int buf_idx = 0;
    int padding = 0;

    while (true) {
        int c = fgetc(in);
        if (c == EOF) break;

        if (c == '\n' || c == '\r' || c == ' ') continue;

        if (c == '=') {
            padding++;
            buf[buf_idx++] = 0;
            if (buf_idx == 8) {
                uint8_t out_buf[5];
                out_buf[0] = (buf[0] << 3) | (buf[1] >> 2);
                out_buf[1] = ((buf[1] & 0x03) << 6) | (buf[2] << 1) | (buf[3] >> 4);
                out_buf[2] = ((buf[3] & 0x0F) << 4) | (buf[4] >> 1);
                out_buf[3] = ((buf[4] & 0x01) << 7) | (buf[5] << 2) | (buf[6] >> 3);
                out_buf[4] = ((buf[6] & 0x07) << 5) | buf[7];

                int out_len;
                if (padding >= 6) out_len = 1;
                else if (padding >= 4) out_len = 2;
                else if (padding >= 3) out_len = 3;
                else if (padding >= 1) out_len = 4;
                else out_len = 5;

                fwrite(out_buf, 1, out_len, out);
                buf_idx = 0;
                padding = 0;
            }
            continue;
        }

        if (c >= 0 && c < 128) {
            int8_t val = decode_table[c];
            if (val >= 0) {
                buf[buf_idx++] = val;
            } else if (!ignore_garbage) {
                return false;
            }
        } else if (!ignore_garbage) {
            return false;
        }

        if (buf_idx == 8) {
            uint8_t out_buf[5];
            out_buf[0] = (buf[0] << 3) | (buf[1] >> 2);
            out_buf[1] = ((buf[1] & 0x03) << 6) | (buf[2] << 1) | (buf[3] >> 4);
            out_buf[2] = ((buf[3] & 0x0F) << 4) | (buf[4] >> 1);
            out_buf[3] = ((buf[4] & 0x01) << 7) | (buf[5] << 2) | (buf[6] >> 3);
            out_buf[4] = ((buf[6] & 0x07) << 5) | buf[7];

            fwrite(out_buf, 1, 5, out);
            buf_idx = 0;
        }
    }

    if (buf_idx > 0) {
        return false;
    }

    return true;
}

void base32_command(int argc, char** argv) {
    struct arg_lit* decode_opt = arg_lit0("d", "decode", "decode data");
    struct arg_lit* ignore_garbage_opt = arg_lit0("i", "ignore-garbage", "when decoding, ignore non-alphabet characters");
    struct arg_int* wrap_opt = arg_int0("w", "wrap", "COLS", "wrap encoded lines after COLS character (default 76, 0 to disable)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 1, "input file");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {decode_opt, ignore_garbage_opt, wrap_opt, help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]\n", argv[0]);
        printf("Base32 encode or decode FILE, or standard input, to standard output.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -d, --decode          decode data\n");
        printf("  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n");
        printf("  -w, --wrap=COLS       wrap encoded lines after COLS character (default 76).\n");
        printf("                        Use 0 to disable line wrapping\n");
        printf("  -h, --help            display this help and exit\n");
        printf("\n");
        printf("The data are encoded as described for the base32 alphabet in RFC 4648.\n");
        printf("When decoding, the input may contain newlines in addition to the bytes\n");
        printf("of the formal base32 alphabet. Use --ignore-garbage to attempt to recover\n");
        printf("from any other non-alphabet bytes in the encoded stream.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool decode = (decode_opt->count > 0);
    bool ignore_garbage = (ignore_garbage_opt->count > 0);
    int wrap_cols = 76;

    if (wrap_opt->count > 0) {
        if (wrap_opt->ival[0] < 0) {
            fprintf(stderr, "base32: invalid wrap value: %d\n", wrap_opt->ival[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        wrap_cols = wrap_opt->ival[0];
    }

    const char* filename = nullptr;
    bool use_stdin = true;

    if (file_arg->count > 0) {
        filename = file_arg->filename[0];
        if (strcmp(filename, "-") != 0) {
            use_stdin = false;
        }
    }

    FILE* in = stdin;
    if (!use_stdin) {
        in = fopen(filename, "rb");
        if (in == nullptr) {
            fprintf(stderr, "base32: %s: No such file or directory\n", filename);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (decode) {
        base32_decode(in, stdout, ignore_garbage);
    } else {
        base32_encode(in, stdout, wrap_cols);
    }

    if (!use_stdin) {
        fclose(in);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
