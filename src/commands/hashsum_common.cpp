#include "commands/hashsum_common.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <argtable3.h>

#include "commands/arg_util.hpp"

struct HashSumOptions {
    int binary = 0;
    int text = 0;
    int check = 0;
    int tag = 0;
    int zero = 0;
    int quiet = 0;
    int status = 0;
    int strict = 0;
    int warn = 0;
    int ignore_missing = 0;
    int length = 0;  // 0 = full digest; only used when spec.variable_length
};

// Streams the file (or stdin) through the digest; returns lowercase hex,
// truncated to `bits` when the algorithm supports variable length.
static std::string compute_hash(const HashAlgoSpec& spec, const std::string& filepath,
                                bool is_stdin, int bits) {
    const EVP_MD* md = spec.md();
    int digest_bytes = (int)EVP_MD_size(md);
    int actual_bits = (bits > 0 && bits <= digest_bytes * 8) ? bits : digest_bytes * 8;
    int actual_bytes = (actual_bits + 7) / 8;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    EVP_DigestInit_ex(ctx, md, nullptr);

    if (is_stdin) {
        char buffer[4096];
        size_t nread;
        while ((nread = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
            EVP_DigestUpdate(ctx, buffer, nread);
        }
    } else {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        char buffer[4096];
        while (file.read(buffer, sizeof(buffer))) {
            EVP_DigestUpdate(ctx, buffer, file.gcount());
        }
        if (file.gcount() > 0) {
            EVP_DigestUpdate(ctx, buffer, file.gcount());
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    if (actual_bytes < (int)hash_len) {
        hash_len = actual_bytes;
    }

    char hex_hash[EVP_MAX_MD_SIZE * 2 + 1];
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hex_hash + (i * 2), "%02x", hash[i]);
    }
    hex_hash[hash_len * 2] = '\0';

    return std::string(hex_hash);
}

static bool check_checksums(const HashAlgoSpec& spec, const std::string& checksum_file,
                            const HashSumOptions& opts) {
    std::ifstream file(checksum_file);
    if (!file) {
        fprintf(stderr, "%s: %s: No such file or directory\n", spec.prog,
                checksum_file.c_str());
        return false;
    }

    size_t fixed_hex_len = (size_t)EVP_MD_size(spec.md()) * 2;

    std::string line;
    int ok_count = 0;
    int failed_count = 0;
    int missing_count = 0;
    int format_error_count = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // "HASH  filename" or "HASH *filename"; fixed-width hash for the
        // md5/sha family, variable width (>= 8 hex chars) for b2sum.
        size_t space_pos = line.find(' ');
        bool bad_width = spec.variable_length
                             ? (space_pos == std::string::npos || space_pos < 8)
                             : (space_pos == std::string::npos || space_pos != fixed_hex_len);
        if (bad_width || space_pos + 2 >= line.length()) {
            if (opts.warn || opts.strict) {
                fprintf(stderr, "%s: %s: improperly formatted checksum line\n",
                        spec.prog, checksum_file.c_str());
            }
            if (opts.strict) {
                format_error_count++;
            }
            continue;
        }

        std::string expected_hash = line.substr(0, space_pos);
        std::string filename = line.substr(space_pos + 2);

        // Backslash escapes in filename
        size_t pos = 0;
        while ((pos = filename.find('\\', pos)) != std::string::npos) {
            if (pos + 1 < filename.length()) {
                char next_char = filename[pos + 1];
                if (next_char == '\\') {
                    filename.replace(pos, 2, "\\");
                } else if (next_char == 'n') {
                    filename.replace(pos, 2, "\n");
                } else if (next_char == 'r') {
                    filename.replace(pos, 2, "\r");
                }
                pos++;
            }
        }

        int bits = spec.variable_length ? (int)(expected_hash.length() * 4) : 0;

        std::string actual_hash = compute_hash(spec, filename, false, bits);
        if (actual_hash.empty()) {
            if (!opts.ignore_missing) {
                fprintf(stderr, "%s: %s: No such file or directory\n", spec.prog,
                        filename.c_str());
                missing_count++;
            }
            continue;
        }

        if (actual_hash == expected_hash) {
            if (!opts.quiet && !opts.status) {
                printf("%s: OK\n", filename.c_str());
            }
            ok_count++;
        } else {
            if (!opts.status) {
                printf("%s: FAILED\n", filename.c_str());
            }
            failed_count++;
        }
    }

    file.close();

    if (opts.status) {
        return (failed_count == 0 && missing_count == 0 && format_error_count == 0);
    }

    if (failed_count > 0 || missing_count > 0 || format_error_count > 0) {
        if (failed_count > 0) {
            fprintf(stderr, "%s: WARNING: %d computed checksum did NOT match\n",
                    spec.prog, failed_count);
        }
        if (missing_count > 0) {
            fprintf(stderr, "%s: WARNING: %d listed file could not be read\n",
                    spec.prog, missing_count);
        }
        if (format_error_count > 0) {
            fprintf(stderr, "%s: WARNING: %d line is improperly formatted\n",
                    spec.prog, format_error_count);
        }
        return false;
    }

    return true;
}

int hashsum_main(int argc, char** argv, const HashAlgoSpec& spec) {
    struct arg_lit* binary_opt = arg_lit0("b", "binary", "read in binary mode");
    struct arg_lit* text_opt = arg_lit0("t", "text", "read in text mode (default)");
    struct arg_lit* check_opt = arg_lit0("c", "check", "read checksums from FILEs and check them");
    struct arg_lit* tag_opt = arg_lit0(NULL, "tag", "create a BSD-style checksum");
    struct arg_lit* zero_opt = arg_lit0("z", "zero", "end each output line with NUL, not newline");
    struct arg_lit* quiet_opt = arg_lit0(NULL, "quiet", "don't print OK for each successfully verified file");
    struct arg_lit* status_opt = arg_lit0(NULL, "status", "don't output anything, status code shows success");
    struct arg_lit* strict_opt = arg_lit0(NULL, "strict", "exit non-zero for improperly formatted checksum lines");
    struct arg_lit* warn_opt = arg_lit0("w", "warn", "warn about improperly formatted checksum lines");
    struct arg_lit* ignore_missing_opt = arg_lit0(NULL, "ignore-missing", "don't fail or report status for missing files");
    struct arg_int* length_opt = spec.variable_length
        ? arg_int0("l", "length", "BITS", "digest length in bits (default 512); must not exceed 512")
        : nullptr;
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 1000, "files to hash");
    struct arg_end* end = arg_end(20);

    std::vector<void*> table = {binary_opt, text_opt, check_opt, tag_opt, zero_opt,
                                quiet_opt, status_opt, strict_opt, warn_opt,
                                ignore_missing_opt};
    if (length_opt) table.push_back(length_opt);
    table.push_back(help_opt);
    table.push_back(files_arg);
    table.push_back(end);

    ArgTable at(std::move(table));

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("%s\n", spec.blurb);
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -b, --binary          read in binary mode\n");
        printf("  -c, --check           read checksums from FILEs and check them\n");
        printf("      --tag             create a BSD-style checksum\n");
        printf("  -t, --text            read in text mode (default)\n");
        printf("  -z, --zero            end each output line with NUL, not newline\n");
        if (spec.variable_length) {
            printf("  -l, --length=BITS     digest length in bits (default 512);\n");
            printf("                        must not exceed the maximum for the algorithm\n");
        }
        printf("\n");
        printf("The following five options are useful only when verifying checksums:\n");
        printf("      --ignore-missing  don't fail or report status for missing files\n");
        printf("      --quiet           don't print OK for each successfully verified file\n");
        printf("      --status          don't output anything, status code shows success\n");
        printf("      --strict          exit non-zero for improperly formatted checksum lines\n");
        printf("  -w, --warn            warn about improperly formatted checksum lines\n");
        printf("\n");
        printf("  -h, --help            display this help and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    HashSumOptions opts;
    opts.binary = (binary_opt->count > 0);
    opts.text = (text_opt->count > 0);
    opts.check = (check_opt->count > 0);
    opts.tag = (tag_opt->count > 0);
    opts.zero = (zero_opt->count > 0);
    opts.quiet = (quiet_opt->count > 0);
    opts.status = (status_opt->count > 0);
    opts.strict = (strict_opt->count > 0);
    opts.warn = (warn_opt->count > 0);
    opts.ignore_missing = (ignore_missing_opt->count > 0);
    opts.length = (length_opt && length_opt->count > 0) ? length_opt->ival[0] : 0;

    if (opts.length > 0 && (opts.length < 8 || opts.length > 512)) {
        fprintf(stderr, "%s: invalid length: %d (must be 8-512)\n", spec.prog, opts.length);
        return 1;
    }

    if (!opts.binary && !opts.text) {
        opts.text = 1;
    }

    if (opts.check) {
        if (files_arg->count == 0) {
            fprintf(stderr, "%s: no files specified for check mode\n", spec.prog);
            return 1;
        }

        bool all_ok = true;
        for (int i = 0; i < files_arg->count; i++) {
            if (!check_checksums(spec, files_arg->filename[i], opts)) {
                all_ok = false;
            }
        }
        return all_ok ? 0 : 1;
    }

    bool use_stdin = (files_arg->count == 0);
    std::vector<std::string> files_to_hash;

    if (use_stdin) {
        files_to_hash.push_back("-");
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            files_to_hash.push_back(files_arg->filename[i]);
        }
    }

    int status = 0;
    for (const auto& file : files_to_hash) {
        bool is_stdin = (file == "-");
        std::string hash = compute_hash(spec, is_stdin ? "" : file, is_stdin, opts.length);

        if (hash.empty() && !is_stdin) {
            fprintf(stderr, "%s: %s: No such file or directory\n", spec.prog, file.c_str());
            status = 1;
            continue;
        }

        char mode_char = opts.binary ? '*' : ' ';
        std::string filename = is_stdin ? "-" : file;

        if (opts.tag) {
            printf("%s (%s) = %s", spec.tag, filename.c_str(), hash.c_str());
        } else {
            printf("%s %c%s", hash.c_str(), mode_char, filename.c_str());
        }

        if (opts.zero) {
            fputc('\0', stdout);
        } else {
            printf("\n");
        }
    }

    return status;
}
