#include <lzma.h>

#include <argtable3.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "commands/arg_util.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/xz.hpp"

namespace {

// xz magic bytes
constexpr uint8_t XZ_MAGIC[6] = {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00};

// Read an entire stream (including stdin) into a buffer.
bool read_all(FILE* fp, std::vector<unsigned char>& out) {
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return !ferror(fp);
}

// Check if data starts with xz magic.
bool is_xz_stream(const std::vector<unsigned char>& in) {
    return in.size() >= 6 &&
           std::memcmp(in.data(), XZ_MAGIC, 6) == 0;
}

// Compress data using liblzma.
bool xz_compress(const std::vector<unsigned char>& in,
                 std::vector<unsigned char>& out, int level) {
    out.clear();

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_easy_encoder(&strm, level, LZMA_CHECK_CRC64);
    if (ret != LZMA_OK) {
        lzma_end(&strm);
        return false;
    }

    strm.next_in = in.data();
    strm.avail_in = (uint32_t)in.size();

    unsigned char buf[65536];
    do {
        strm.next_out = buf;
        strm.avail_out = sizeof(buf);
        ret = lzma_code(&strm, LZMA_FINISH);
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            lzma_end(&strm);
            return false;
        }
        size_t got = sizeof(buf) - strm.avail_out;
        out.insert(out.end(), buf, buf + got);
    } while (ret != LZMA_STREAM_END);

    lzma_end(&strm);
    return true;
}

// Decompress data using liblzma.
bool xz_decompress(const std::vector<unsigned char>& in,
                   std::vector<unsigned char>& out) {
    if (!is_xz_stream(in)) return false;

    out.clear();

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
    if (ret != LZMA_OK) return false;

    strm.next_in = in.data();
    strm.avail_in = (uint32_t)in.size();

    unsigned char buf[65536];
    bool done = false;
    while (!done) {
        strm.next_out = buf;
        strm.avail_out = sizeof(buf);
        ret = lzma_code(&strm, LZMA_FINISH);

        size_t got = sizeof(buf) - strm.avail_out;
        if (got > 0) {
            out.insert(out.end(), buf, buf + got);
        }

        if (ret == LZMA_STREAM_END) {
            done = true;
        } else if (ret != LZMA_OK && ret != LZMA_BUF_ERROR) {
            lzma_end(&strm);
            return false;
        }

        // If no input left and no output produced, we're stuck
        if (ret == LZMA_BUF_ERROR && strm.avail_in == 0 && got == 0) {
            lzma_end(&strm);
            return false;
        }
    }

    lzma_end(&strm);
    return true;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string strip_xz(const std::string& p) {
    return ends_with(p, ".xz") ? p.substr(0, p.size() - 3) : p;
}

// Write `data` to `outname` unless it exists and !force. Returns 0 on success.
int write_output_file(const std::vector<unsigned char>& data,
                      const std::string& outname, bool force,
                      const char* prog) {
    if (!force) {
        struct stat st;
        if (stat(outname.c_str(), &st) == 0) {
            fprintf(stderr, "%s: %s: File exists\n", prog, outname.c_str());
            return 1;
        }
    }
    FILE* out = fopen(outname.c_str(), "wb");
    if (!out) {
        cmd_perror(prog, outname.c_str());
        return 1;
    }
    if (fwrite(data.data(), 1, data.size(), out) != data.size()) {
        fclose(out);
        cmd_perror(prog, outname.c_str());
        return 1;
    }
    if (fclose(out) != 0) {
        cmd_perror(prog, outname.c_str());
        return 1;
    }
    return 0;
}

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [FILE]...\n", prog);
    printf("Compress or decompress FILEs with xz.\n");
    printf("\n");
    printf("  -c, --stdout          write to stdout, keep original files\n");
    printf("  -d, --decompress, --uncompress\n");
    printf("                        decompress\n");
    printf("  -k, --keep            keep (do not delete) input files\n");
    printf("  -f, --force           force overwrite of output file\n");
    printf("  -0..-9                compression level (0=fast, 9=best, default 6)\n");
    printf("  -q, --quiet           suppress warnings\n");
    printf("  -v, --verbose         print file name and compression ratio\n");
    printf("  -h, --help            display this help and exit\n");
    printf("      --version         display version and exit\n");
    printf("\n");
    printf("With no FILE, or when FILE is -, read standard input.\n");
}

void print_ratio(const std::string& name, size_t in_size,
                 size_t out_size, const char* replaced_with) {
    double ratio = in_size == 0 ? 100.0 : (1.0 - (double)out_size / (double)in_size) * 100.0;
    if (replaced_with) {
        printf("%s: %5.1f%% -- replaced with %s\n", name.c_str(), ratio,
               replaced_with);
    } else {
        printf("%s: %5.1f%%\n", name.c_str(), ratio);
    }
}

int process_path(const XzOptions& opt, const std::string& path,
                 const char* prog) {
    bool stdin_mode = (path == "-");

    if (stdin_mode) {
        std::vector<unsigned char> in;
        if (!read_all(stdin, in)) {
            fprintf(stderr, "%s: stdin: %s\n", prog, strerror(errno));
            return 1;
        }
        if (opt.decompress) {
            if (!is_xz_stream(in)) {
                fprintf(stderr, "%s: stdin: Compressed data is corrupt\n", prog);
                return 1;
            }
            std::vector<unsigned char> out;
            if (!xz_decompress(in, out)) {
                fprintf(stderr, "%s: stdin: Compressed data is corrupt\n", prog);
                return 1;
            }
            fwrite(out.data(), 1, out.size(), stdout);
            if (opt.verbose) print_ratio("-", in.size(), out.size(), nullptr);
            return 0;
        }
        std::vector<unsigned char> out;
        if (!xz_compress(in, out, opt.level)) {
            fprintf(stderr, "%s: stdin: Compression failed\n", prog);
            return 1;
        }
        fwrite(out.data(), 1, out.size(), stdout);
        if (opt.verbose) print_ratio("-", in.size(), out.size(), nullptr);
        return 0;
    }

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        cmd_perror(prog, path.c_str());
        return 1;
    }
    std::vector<unsigned char> in;
    bool read_ok = read_all(fp, in);
    int read_errno = errno;
    fclose(fp);
    if (!read_ok) {
        fprintf(stderr, "%s: %s: %s\n", prog, path.c_str(), strerror(read_errno));
        return 1;
    }

    if (opt.decompress) {
        std::vector<unsigned char> out;
        if (!xz_decompress(in, out)) {
            fprintf(stderr, "%s: %s: Compressed data is corrupt\n", prog, path.c_str());
            return 1;
        }
        if (opt.to_stdout) {
            fwrite(out.data(), 1, out.size(), stdout);
        } else {
            std::string outname = strip_xz(path);
            if (write_output_file(out, outname, opt.force, prog) != 0) {
                return 1;
            }
            if (!opt.keep) {
                std::remove(path.c_str());
            }
        }
        if (opt.verbose) print_ratio(path, in.size(), out.size(), nullptr);
        return 0;
    }

    // Compression
    if (ends_with(path, ".xz")) {
        if (!opt.quiet) {
            fprintf(stderr, "%s: %s: file already has .xz suffix\n", prog, path.c_str());
        }
        return 0;
    }

    std::vector<unsigned char> out;
    if (!xz_compress(in, out, opt.level)) {
        fprintf(stderr, "%s: %s: Compression failed\n", prog, path.c_str());
        return 1;
    }
    std::string outname = path + ".xz";
    if (opt.to_stdout) {
        fwrite(out.data(), 1, out.size(), stdout);
    } else {
        if (write_output_file(out, outname, opt.force, prog) != 0) {
            return 1;
        }
        if (!opt.keep) {
            std::remove(path.c_str());
        }
    }
    if (opt.verbose) print_ratio(path, in.size(), out.size(), outname.c_str());
    return 0;
}

} // namespace

int xz_command(int argc, char** argv) {
    const char* prog = argv[0];

    XzOptions opt;

    // argtable3 cannot represent `-1`..`-9` as short options, so extract the
    // numeric level flags ourselves and pass the rest to argtable3.
    int pre_level = 6;
    bool saw_level = false;
    std::vector<std::string> owned;
    std::vector<const char*> cargv;
    cargv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.size() >= 2 && a[0] == '-' && a[1] >= '0' && a[1] <= '9') {
            pre_level = a[1] - '0';
            saw_level = true;
            if (a.size() > 2) {
                owned.push_back("-" + a.substr(2));
                cargv.push_back(owned.back().c_str());
            }
        } else {
            cargv.push_back(argv[i]);
        }
    }

    struct arg_lit* decompress = arg_lit0("d", "decompress|uncompress", "Decompress");
    struct arg_lit* keep = arg_lit0("k", "keep", "Keep input files");
    struct arg_lit* force = arg_lit0("f", "force", "Force overwrite");
    struct arg_lit* quiet = arg_lit0("q", "quiet", "Suppress warnings");
    struct arg_lit* verbose = arg_lit0("v", "verbose", "Verbose output");
    struct arg_lit* stdout_ = arg_lit0("c", "stdout", "Write to stdout");
    struct arg_int* level = arg_int0(NULL, NULL, "LEVEL", "Compression level (0-9)");
    struct arg_file* files = arg_filen(NULL, NULL, "FILE...", 0, 1000, "files");
    struct arg_lit* help = arg_lit0("h", "help", "Show help");
    struct arg_lit* version = arg_lit0(NULL, "version", "Show version");
    struct arg_end* end = arg_end(20);

    std::vector<void*> table = {
        (void*)decompress, (void*)keep, (void*)force, (void*)quiet,
        (void*)verbose, (void*)stdout_, (void*)level, (void*)files,
        (void*)help, (void*)version, (void*)end,
    };

    ArgTable tbl(table);
    int errors = tbl.parse((int)cargv.size(), (char**)cargv.data());
    if (errors != 0) {
        return tbl.print_errors(end, prog);
    }

    if (help->count > 0) {
        print_help(prog);
        return 0;
    }

    if (version->count > 0) {
        print_version(prog);
        return 0;
    }

    opt.decompress = (decompress->count > 0);
    opt.keep = (keep->count > 0);
    opt.force = (force->count > 0);
    opt.quiet = (quiet->count > 0);
    opt.verbose = (verbose->count > 0);
    opt.to_stdout = (stdout_->count > 0);
    if (saw_level) opt.level = pre_level;

    if (level->count > 0) {
        int lvl = level->ival[0];
        if (lvl < 0 || lvl > 9) {
            fprintf(stderr, "%s: invalid compression level: %d\n", prog, lvl);
            return 1;
        }
        opt.level = lvl;
    }

    // Collect file paths
    std::vector<std::string> paths;
    for (int i = 0; i < files->count; i++) {
        if (files->filename[i]) {
            paths.push_back(files->filename[i]);
        }
    }

    if (paths.empty()) {
        paths.push_back("-");
    }

    int status = 0;
    for (const auto& path : paths) {
        int ret = process_path(opt, path, prog);
        if (ret != 0) status = 1;
    }
    return status;
}

REGISTER_COMMAND("xz", xz_command, "Compress or decompress files with xz");
