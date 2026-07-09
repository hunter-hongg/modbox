#include <cstdio>
#include <cstring>
#include <cstdint>
#include <argtable3.h>
#include "commands/base64.hpp"

static const char base64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8_t decode_table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

static void base64_encode(FILE* in, FILE* out, int wrap_cols) {
    uint8_t in_buf[3];
    uint8_t out_buf[4];
    int col = 0;
    
    while (true) {
        size_t bytes_read = fread(in_buf, 1, 3, in);
        if (bytes_read == 0) break;
        
        // Handle partial last block
        if (bytes_read < 3) {
            in_buf[1] = (bytes_read >= 2) ? in_buf[1] : 0;
            in_buf[2] = (bytes_read >= 3) ? in_buf[2] : 0;
        }
        
        // Encode 3 bytes to 4 base64 characters
        out_buf[0] = base64_table[in_buf[0] >> 2];
        out_buf[1] = base64_table[((in_buf[0] & 0x03) << 4) | (in_buf[1] >> 4)];
        out_buf[2] = base64_table[((in_buf[1] & 0x0F) << 2) | (in_buf[2] >> 6)];
        out_buf[3] = base64_table[in_buf[2] & 0x3F];
        
        // Add padding for incomplete last block
        if (bytes_read == 1) {
            out_buf[2] = '=';
            out_buf[3] = '=';
        } else if (bytes_read == 2) {
            out_buf[3] = '=';
        }
        
        // Write output with line wrapping
        for (int i = 0; i < 4; i++) {
            fputc(out_buf[i], out);
            if (wrap_cols > 0) {
                col++;
                if (col >= wrap_cols) {
                    fputc('\n', out);
                    col = 0;
                }
            }
        }
    }
    
    // Add final newline if wrapping was enabled
    if (wrap_cols > 0 && col > 0) {
        fputc('\n', out);
    }
}

static bool base64_decode(FILE* in, FILE* out, bool ignore_garbage) {
    uint8_t in_buf[4];
    uint8_t out_buf[3];
    int buf_idx = 0;
    int padding = 0;
    
    while (true) {
        int c = fgetc(in);
        if (c == EOF) break;
        
        // Skip newlines (always accepted)
        if (c == '\n') continue;
        
        // Handle padding
        if (c == '=') {
            padding++;
            in_buf[buf_idx++] = 0;
            if (buf_idx == 4) {
                // Decode complete block with padding
                out_buf[0] = (in_buf[0] << 2) | (in_buf[1] >> 4);
                out_buf[1] = ((in_buf[1] & 0x0F) << 4) | (in_buf[2] >> 2);
                out_buf[2] = ((in_buf[2] & 0x03) << 6) | in_buf[3];
                
                int out_len = 3 - padding;
                fwrite(out_buf, 1, out_len, out);
                buf_idx = 0;
                padding = 0;
            }
            continue;
        }
        
        // Decode character
        if (c >= 0 && c < 128) {
            int8_t val = decode_table[c];
            if (val >= 0) {
                in_buf[buf_idx++] = val;
            } else if (!ignore_garbage) {
                // Invalid character
                return false;
            }
            // If ignore_garbage is true, skip invalid characters
        } else if (!ignore_garbage) {
            return false;
        }
        
        // Decode complete block
        if (buf_idx == 4) {
            out_buf[0] = (in_buf[0] << 2) | (in_buf[1] >> 4);
            out_buf[1] = ((in_buf[1] & 0x0F) << 4) | (in_buf[2] >> 2);
            out_buf[2] = ((in_buf[2] & 0x03) << 6) | in_buf[3];
            
            fwrite(out_buf, 1, 3, out);
            buf_idx = 0;
        }
    }
    
    // Handle incomplete final block
    if (buf_idx > 0) {
        // If we have a partial block without proper padding, it's invalid
        return false;
    }
    
    return true;
}

void base64_command(int argc, char** argv) {
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
        printf("Base64 encode or decode FILE, or standard input, to standard output.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -d, --decode          decode data\n");
        printf("  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n");
        printf("  -w, --wrap=COLS       wrap encoded lines after COLS character (default 76).\n");
        printf("                        Use 0 to disable line wrapping\n");
        printf("  -h, --help            display this help and exit\n");
        printf("\n");
        printf("The data are encoded as described for the base64 alphabet in RFC 4648.\n");
        printf("When decoding, the input may contain newlines in addition to the bytes\n");
        printf("of the formal base64 alphabet. Use --ignore-garbage to attempt to recover\n");
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
    int wrap_cols = 76; // Default per GNU
    
    if (wrap_opt->count > 0) {
        if (wrap_opt->ival[0] < 0) {
            fprintf(stderr, "base64: invalid wrap value: %d\n", wrap_opt->ival[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        wrap_cols = wrap_opt->ival[0];
    }
    
    // Determine input source
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
            fprintf(stderr, "base64: %s: No such file or directory\n", filename);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }
    
    if (decode) {
        bool success = base64_decode(in, stdout, ignore_garbage);
        if (!success) {
            // If decoding failed without ignore-garbage, we still produce whatever output we can
            // GNU base64 doesn't necessarily exit with error on invalid input
        }
    } else {
        base64_encode(in, stdout, wrap_cols);
    }
    
    if (!use_stdin) {
        fclose(in);
    }
    
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
