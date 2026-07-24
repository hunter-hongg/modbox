#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <argtable3.h>
#include "commands/basenc.hpp"
#include "commands/command_macros.hpp"

// ── Base64 table (standard) ───────────────────────────────────────────────
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char base64url_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int8_t build_decode_table(const char* alphabet) {
    static int8_t table[128];
    memset(table, -1, sizeof(table));
    for (int i = 0; alphabet[i]; i++) {
        table[(int)alphabet[i]] = (int8_t)i;
    }
    table['='] = -2;  // padding marker
    return 0;
}

// ── Base16 (hex) ──────────────────────────────────────────────────────────
static const char base16_table[] = "0123456789ABCDEF";

static void base16_encode(FILE* in, FILE* out, int wrap_cols) {
    int col = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        fprintf(out, "%02X", (unsigned char)c);
        if (wrap_cols > 0) {
            col += 2;
            if (col >= wrap_cols) {
                fputc('\n', out);
                col = 0;
            }
        }
    }
    if (wrap_cols > 0 && col > 0) {
        fputc('\n', out);
    }
}

static bool base16_decode(FILE* in, FILE* out, bool ignore_garbage) {
    int nybble_hi = -1;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '\n' || c == '\r' || c == ' ') continue;
        int val = -1;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (!ignore_garbage) return false;
        if (val < 0) { if (!ignore_garbage) return false; continue; }

        if (nybble_hi < 0) {
            nybble_hi = val;
        } else {
            fputc((nybble_hi << 4) | val, out);
            nybble_hi = -1;
        }
    }
    return (nybble_hi < 0);  // no dangling nybble
}

// ── Base32 ────────────────────────────────────────────────────────────────
static const char base32_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char base32hex_table[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

static void base32_encode_generic(FILE* in, FILE* out, int wrap_cols,
                                   const char* table) {
    uint8_t buf[5];
    int col = 0;

    while (true) {
        size_t n = fread(buf, 1, 5, in);
        if (n == 0) break;

        if (n < 5) {
            for (size_t i = n; i < 5; i++) buf[i] = 0;
        }

        uint8_t out_chars[8];
        out_chars[0] = table[buf[0] >> 3];
        out_chars[1] = table[((buf[0] & 0x07) << 2) | (buf[1] >> 6)];

        int out_len;
        if (n == 1) {
            out_chars[2] = '='; out_chars[3] = '=';
            out_chars[4] = '='; out_chars[5] = '=';
            out_chars[6] = '='; out_chars[7] = '=';
            out_len = 8;
        } else if (n == 2) {
            out_chars[2] = table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = '='; out_chars[5] = '=';
            out_chars[6] = '='; out_chars[7] = '=';
            out_len = 8;
        } else if (n == 3) {
            out_chars[2] = table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = table[((buf[2] & 0x0F) << 1) | (buf[3] >> 7)];
            out_chars[5] = '='; out_chars[6] = '=';
            out_chars[7] = '='; out_len = 8;
        } else if (n == 4) {
            out_chars[2] = table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = table[((buf[2] & 0x0F) << 1) | (buf[3] >> 7)];
            out_chars[5] = table[(buf[3] & 0x7C) >> 2];
            out_chars[6] = table[((buf[3] & 0x03) << 3) | (buf[4] >> 5)];
            out_chars[7] = '='; out_len = 8;
        } else {
            out_chars[2] = table[(buf[1] & 0x3E) >> 1];
            out_chars[3] = table[((buf[1] & 0x01) << 4) | (buf[2] >> 4)];
            out_chars[4] = table[((buf[2] & 0x0F) << 1) | (buf[3] >> 7)];
            out_chars[5] = table[(buf[3] & 0x7C) >> 2];
            out_chars[6] = table[((buf[3] & 0x03) << 3) | (buf[4] >> 5)];
            out_chars[7] = table[buf[4] & 0x1F];
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

static bool base32_decode_generic(FILE* in, FILE* out, const char* alphabet,
                                   bool ignore_garbage) {
    uint8_t buf[8];
    int buf_idx = 0;
    int padding = 0;
    int8_t decode_table[128];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; alphabet[i]; i++) {
        decode_table[(int)alphabet[i]] = (int8_t)i;
    }

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

    return (buf_idx == 0);
}

// ── Base64 encode ─────────────────────────────────────────────────────────
static void base64_encode_generic(FILE* in, FILE* out, int wrap_cols,
                                   const char* table) {
    uint8_t buf[3];
    int col = 0;

    while (true) {
        size_t n = fread(buf, 1, 3, in);
        if (n == 0) break;

        if (n < 3) {
            for (size_t i = n; i < 3; i++) buf[i] = 0;
        }

        uint8_t out_chars[4];
        out_chars[0] = table[buf[0] >> 2];
        out_chars[1] = table[((buf[0] & 0x03) << 4) | (buf[1] >> 4)];
        out_chars[2] = table[((buf[1] & 0x0F) << 2) | (buf[2] >> 6)];
        out_chars[3] = table[buf[2] & 0x3F];

        if (n == 1) {
            out_chars[2] = '=';
            out_chars[3] = '=';
        } else if (n == 2) {
            out_chars[3] = '=';
        }

        for (int i = 0; i < 4; i++) {
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

static bool base64_decode_generic(FILE* in, FILE* out, const char* alphabet,
                                   bool ignore_garbage) {
    uint8_t buf[4];
    int buf_idx = 0;
    int padding = 0;
    int8_t decode_table[128];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; alphabet[i]; i++) {
        decode_table[(int)alphabet[i]] = (int8_t)i;
    }

    while (true) {
        int c = fgetc(in);
        if (c == EOF) break;

        if (c == '\n' || c == '\r' || c == ' ') continue;

        if (c == '=') {
            padding++;
            buf[buf_idx++] = 0;
            if (buf_idx == 4) {
                uint8_t out_buf[3];
                out_buf[0] = (buf[0] << 2) | (buf[1] >> 4);
                out_buf[1] = (buf[1] << 4) | (buf[2] >> 2);
                out_buf[2] = (buf[2] << 6) | buf[3];

                int out_len;
                if (padding >= 2) out_len = 1;
                else if (padding >= 1) out_len = 2;
                else out_len = 3;

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

        if (buf_idx == 4) {
            uint8_t out_buf[3];
            out_buf[0] = (buf[0] << 2) | (buf[1] >> 4);
            out_buf[1] = (buf[1] << 4) | (buf[2] >> 2);
            out_buf[2] = (buf[2] << 6) | buf[3];

            fwrite(out_buf, 1, 3, out);
            buf_idx = 0;
        }
    }

    return (buf_idx == 0);
}

// ── Base2 MSB-first ──────────────────────────────────────────────────────
static void base2msbf_encode(FILE* in, FILE* out, int wrap_cols) {
    int col = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        for (int bit = 7; bit >= 0; bit--) {
            fputc((c & (1 << bit)) ? '1' : '0', out);
            if (wrap_cols > 0) {
                col++;
                if (col >= wrap_cols) {
                    fputc('\n', out);
                    col = 0;
                }
            }
        }
    }
    if (wrap_cols > 0 && col > 0) fputc('\n', out);
}

static bool base2msbf_decode(FILE* in, FILE* out, bool ignore_garbage) {
    int byte_val = 0;
    int bits = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '\n' || c == '\r' || c == ' ') continue;
        if (c == '0') {
            byte_val = (byte_val << 1);
            bits++;
        } else if (c == '1') {
            byte_val = (byte_val << 1) | 1;
            bits++;
        } else if (!ignore_garbage) {
            return false;
        }
        if (bits == 8) {
            fputc(byte_val, out);
            byte_val = 0;
            bits = 0;
        }
    }
    return (bits == 0);  // no dangling bits
}

// ── Base2 LSB-first ──────────────────────────────────────────────────────
static void base2lsbf_encode(FILE* in, FILE* out, int wrap_cols) {
    int col = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        for (int bit = 0; bit < 8; bit++) {
            fputc((c & (1 << bit)) ? '1' : '0', out);
            if (wrap_cols > 0) {
                col++;
                if (col >= wrap_cols) {
                    fputc('\n', out);
                    col = 0;
                }
            }
        }
    }
    if (wrap_cols > 0 && col > 0) fputc('\n', out);
}

static bool base2lsbf_decode(FILE* in, FILE* out, bool ignore_garbage) {
    int byte_val = 0;
    int bit_pos = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '\n' || c == '\r' || c == ' ') continue;
        if (c == '1') {
            byte_val |= (1 << bit_pos);
            bit_pos++;
        } else if (c == '0') {
            bit_pos++;
        } else if (!ignore_garbage) {
            return false;
        }
        if (bit_pos == 8) {
            fputc(byte_val, out);
            byte_val = 0;
            bit_pos = 0;
        }
    }
    return (bit_pos == 0);
}

// ── Encoding selection ────────────────────────────────────────────────────
enum class BaseNEncoding {
    BASE64,
    BASE64URL,
    BASE32,
    BASE32HEX,
    BASE16,
    BASE2MSBF,
    BASE2LSBF
};

static void encode_stream(FILE* in, FILE* out, BaseNEncoding enc, int wrap) {
    switch (enc) {
        case BaseNEncoding::BASE64:
            base64_encode_generic(in, out, wrap, base64_table);
            break;
        case BaseNEncoding::BASE64URL:
            base64_encode_generic(in, out, wrap, base64url_table);
            break;
        case BaseNEncoding::BASE32:
            base32_encode_generic(in, out, wrap, base32_table);
            break;
        case BaseNEncoding::BASE32HEX:
            base32_encode_generic(in, out, wrap, base32hex_table);
            break;
        case BaseNEncoding::BASE16:
            base16_encode(in, out, wrap);
            break;
        case BaseNEncoding::BASE2MSBF:
            base2msbf_encode(in, out, wrap);
            break;
        case BaseNEncoding::BASE2LSBF:
            base2lsbf_encode(in, out, wrap);
            break;
    }
}

static bool decode_stream(FILE* in, FILE* out, BaseNEncoding enc, bool ignore_garbage) {
    switch (enc) {
        case BaseNEncoding::BASE64:
            return base64_decode_generic(in, out, base64_table, ignore_garbage);
        case BaseNEncoding::BASE64URL:
            return base64_decode_generic(in, out, base64url_table, ignore_garbage);
        case BaseNEncoding::BASE32:
            return base32_decode_generic(in, out, base32_table, ignore_garbage);
        case BaseNEncoding::BASE32HEX:
            return base32_decode_generic(in, out, base32hex_table, ignore_garbage);
        case BaseNEncoding::BASE16:
            return base16_decode(in, out, ignore_garbage);
        case BaseNEncoding::BASE2MSBF:
            return base2msbf_decode(in, out, ignore_garbage);
        case BaseNEncoding::BASE2LSBF:
            return base2lsbf_decode(in, out, ignore_garbage);
    }
    return false;
}

void basenc_command(int argc, char** argv) {
    struct arg_lit* base64_opt = arg_lit0(NULL, "base64", "same as base64 (RFC 4648 section 4)");
    struct arg_lit* base64url_opt = arg_lit0(NULL, "base64url", "base64url (RFC 4648 section 5)");
    struct arg_lit* base32_opt = arg_lit0(NULL, "base32", "same as base32 (RFC 4648 section 6)");
    struct arg_lit* base32hex_opt = arg_lit0(NULL, "base32hex", "base32hex (RFC 4648 section 7)");
    struct arg_lit* base16_opt = arg_lit0(NULL, "base16", "base16 (RFC 4648 section 8)");
    struct arg_lit* base2msbf_opt = arg_lit0(NULL, "base2msbf", "base2 (most significant bit first)");
    struct arg_lit* base2lsbf_opt = arg_lit0(NULL, "base2lsbf", "base2 (least significant bit first)");
    struct arg_lit* decode_opt = arg_lit0("d", "decode", "decode data");
    struct arg_lit* ignore_garbage_opt = arg_lit0("i", "ignore-garbage", "when decoding, ignore non-alphabet characters");
    struct arg_int* wrap_opt = arg_int0("w", "wrap", "COLS", "wrap encoded lines after COLS character (default 76, 0 to disable)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 1, "input file");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {base64_opt, base64url_opt, base32_opt, base32hex_opt,
                        base16_opt, base2msbf_opt, base2lsbf_opt,
                        decode_opt, ignore_garbage_opt, wrap_opt, help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s ENCODING [OPTION]... [FILE]\n", argv[0]);
        printf("Encode or decode FILE, or standard input, to standard output.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("Encoding type (exactly one required):\n");
        printf("      --base64       same as base64 (RFC 4648 section 4)\n");
        printf("      --base64url    base64url (RFC 4648 section 5)\n");
        printf("      --base32       same as base32 (RFC 4648 section 6)\n");
        printf("      --base32hex    base32hex (RFC 4648 section 7)\n");
        printf("      --base16       base16 (RFC 4648 section 8)\n");
        printf("      --base2msbf    base2 (most significant bit first)\n");
        printf("      --base2lsbf    base2 (least significant bit first)\n");
        printf("\n");
        printf("  -d, --decode          decode data\n");
        printf("  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n");
        printf("  -w, --wrap=COLS       wrap encoded lines after COLS character (default 76).\n");
        printf("                        Use 0 to disable line wrapping\n");
        printf("  -h, --help            display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Determine encoding type
    int enc_count = base64_opt->count + base64url_opt->count + base32_opt->count +
                    base32hex_opt->count + base16_opt->count + base2msbf_opt->count +
                    base2lsbf_opt->count;
    if (enc_count != 1) {
        fprintf(stderr, "%s: exactly one encoding type must be specified\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    BaseNEncoding enc;
    if (base64_opt->count) enc = BaseNEncoding::BASE64;
    else if (base64url_opt->count) enc = BaseNEncoding::BASE64URL;
    else if (base32_opt->count) enc = BaseNEncoding::BASE32;
    else if (base32hex_opt->count) enc = BaseNEncoding::BASE32HEX;
    else if (base16_opt->count) enc = BaseNEncoding::BASE16;
    else if (base2msbf_opt->count) enc = BaseNEncoding::BASE2MSBF;
    else enc = BaseNEncoding::BASE2LSBF;

    bool decode = (decode_opt->count > 0);
    bool ignore_garbage = (ignore_garbage_opt->count > 0);
    int wrap_cols = 76;

    if (wrap_opt->count > 0) {
        if (wrap_opt->ival[0] < 0) {
            fprintf(stderr, "basenc: invalid wrap value: %d\n", wrap_opt->ival[0]);
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
            fprintf(stderr, "basenc: %s: No such file or directory\n", filename);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (decode) {
        if (!decode_stream(in, stdout, enc, ignore_garbage)) {
            fprintf(stderr, "basenc: invalid input\n");
        }
    } else {
        encode_stream(in, stdout, enc, wrap_cols);
    }

    if (!use_stdin) {
        fclose(in);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("basenc", basenc_command, "BaseN encode/decode");
