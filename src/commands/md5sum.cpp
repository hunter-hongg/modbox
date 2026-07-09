#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <openssl/md5.h>
#include <argtable3.h>
#include "commands/md5sum.hpp"

struct Md5SumOptions {
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
};

static std::string compute_md5(const std::string& filepath, bool is_stdin) {
    MD5_CTX md5;
    MD5_Init(&md5);

    if (is_stdin) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), stdin)) {
            MD5_Update(&md5, buffer, strlen(buffer));
        }
    } else {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            return "";
        }
        char buffer[4096];
        while (file.read(buffer, sizeof(buffer))) {
            MD5_Update(&md5, buffer, file.gcount());
        }
        if (file.gcount() > 0) {
            MD5_Update(&md5, buffer, file.gcount());
        }
    }

    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5);

    char hex_hash[MD5_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hex_hash + (i * 2), "%02x", hash[i]);
    }
    hex_hash[MD5_DIGEST_LENGTH * 2] = '\0';

    return std::string(hex_hash);
}

static bool check_checksums(const std::string& checksum_file, const Md5SumOptions& opts) {
    std::ifstream file(checksum_file);
    if (!file) {
        fprintf(stderr, "md5sum: %s: No such file or directory\n", checksum_file.c_str());
        return false;
    }

    std::string line;
    int ok_count = 0;
    int failed_count = 0;
    int missing_count = 0;
    int format_error_count = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Parse line: expected format is "HASH  filename" or "HASH *filename"
        size_t space_pos = line.find(' ');
        if (space_pos == std::string::npos || space_pos != 32) {
            if (opts.warn || opts.strict) {
                fprintf(stderr, "md5sum: %s: improperly formatted checksum line\n", 
                        checksum_file.c_str());
            }
            if (opts.strict) {
                format_error_count++;
            }
            continue;
        }

        std::string expected_hash = line.substr(0, 32);
        char mode_char = line[33]; // Should be ' ' or '*'
        std::string filename = line.substr(34);

        // Handle backslash escapes in filename
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

        std::string actual_hash = compute_md5(filename, false);
        if (actual_hash.empty()) {
            if (!opts.ignore_missing) {
                fprintf(stderr, "md5sum: %s: No such file or directory\n", filename.c_str());
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
            fprintf(stderr, "md5sum: WARNING: %d computed checksum did NOT match\n", failed_count);
        }
        if (missing_count > 0) {
            fprintf(stderr, "md5sum: WARNING: %d listed file could not be read\n", missing_count);
        }
        if (format_error_count > 0) {
            fprintf(stderr, "md5sum: WARNING: %d line is improperly formatted\n", format_error_count);
        }
        return false;
    }

    return true;
}

void md5sum_command(int argc, char** argv) {
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
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 1000, "files to hash");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {binary_opt, text_opt, check_opt, tag_opt, zero_opt, quiet_opt, 
                        status_opt, strict_opt, warn_opt, ignore_missing_opt, help_opt, files_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Print or check MD5 (128-bit) checksums.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("  -b, --binary          read in binary mode\n");
        printf("  -c, --check           read checksums from FILEs and check them\n");
        printf("      --tag             create a BSD-style checksum\n");
        printf("  -t, --text            read in text mode (default)\n");
        printf("  -z, --zero            end each output line with NUL, not newline\n");
        printf("\n");
        printf("The following five options are useful only when verifying checksums:\n");
        printf("      --ignore-missing  don't fail or report status for missing files\n");
        printf("      --quiet           don't print OK for each successfully verified file\n");
        printf("      --status          don't output anything, status code shows success\n");
        printf("      --strict          exit non-zero for improperly formatted checksum lines\n");
        printf("  -w, --warn            warn about improperly formatted checksum lines\n");
        printf("\n");
        printf("  -h, --help            display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    Md5SumOptions opts;
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

    // Default to text mode if neither binary nor text specified
    if (!opts.binary && !opts.text) {
        opts.text = 1;
    }

    // Check mode
    if (opts.check) {
        if (files_arg->count == 0) {
            fprintf(stderr, "md5sum: no files specified for check mode\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        bool all_ok = true;
        for (int i = 0; i < files_arg->count; i++) {
            if (!check_checksums(files_arg->filename[i], opts)) {
                all_ok = false;
            }
        }

        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Regular hash mode
    bool use_stdin = (files_arg->count == 0);
    std::vector<std::string> files_to_hash;

    if (use_stdin) {
        files_to_hash.push_back("-");
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            files_to_hash.push_back(files_arg->filename[i]);
        }
    }

    for (const auto& file : files_to_hash) {
        bool is_stdin = (file == "-");
        std::string hash = compute_md5(is_stdin ? "" : file, is_stdin);

        if (hash.empty() && !is_stdin) {
            fprintf(stderr, "md5sum: %s: No such file or directory\n", file.c_str());
            continue;
        }

        char mode_char = opts.binary ? '*' : ' ';
        std::string filename = is_stdin ? "-" : file;

        if (opts.tag) {
            printf("MD5 (%s) = %s", filename.c_str(), hash.c_str());
        } else {
            printf("%s %c%s", hash.c_str(), mode_char, filename.c_str());
        }

        if (opts.zero) {
            printf("\0");
        } else {
            printf("\n");
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
