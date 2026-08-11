#include <cstdint>
#include <zstd.h>

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
#include "commands/zstd.hpp"

namespace {

constexpr uint8_t ZSTD_MAGIC[4] = {0x28, 0xb5, 0x2f, 0xfd};

// Read an entire stream (including stdin) into a buffer.
bool read_all(FILE* fp, std::vector<unsigned char>& out) {
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return !ferror(fp);
}

bool is_zstd_stream(const std::vector<unsigned char>& in) {
    return in.size() >= 4 &&
           std::memcmp(in.data(), ZSTD_MAGIC, 4) == 0;
}

bool zstd_compress(const std::vector<unsigned char>& in,
                   std::vector<unsigned char>& out, int level) {
    out.clear();
    size_t const buffOutSize = ZSTD_compressBound(in.size());
    std::vector<unsigned char> buffOut(buffOutSize);
    size_t const compressedSize =
        ZSTD_compress(buffOut.data(), buffOutSize, in.data(), in.size(),
                      level);
    if (ZSTD_isError(compressedSize)) {
        return false;
    }
    out.insert(out.end(), buffOut.begin(),
               buffOut.begin() + (std::ptrdiff_t)compressedSize);
    return true;
}

bool zstd_decompress(const std::vector<unsigned char>& in,
                     std::vector<unsigned char>& out) {
    if (!is_zstd_stream(in)) return false;
    out.clear();
    size_t const decompressedSize =
        ZSTD_getFrameContentSize(in.data(), in.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return false;
    }
    std::vector<unsigned char> buffIn(in.data(), in.data() + in.size());
    std::vector<unsigned char> buffOut(decompressedSize);
    size_t const decompressedSize2 = ZSTD_decompress(
        buffOut.data(), buffOut.size(), buffIn.data(), buffIn.size());
    if (ZSTD_isError(decompressedSize2)) {
        return false;
    }
    out.assign(buffOut.begin(),
               buffOut.begin() + (std::ptrdiff_t)decompressedSize2);
    return true;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string strip_zst(const std::string& p) {
    return ends_with(p, ".zst") ? p.substr(0, p.size() - 4) : p;
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
    printf("Compress or decompress FILEs with zstd.\n");
    printf("\n");
    printf("  -c, --stdout          write to stdout, keep original files\n");
    printf("  -d, --decompress      decompress\n");
    printf("  -k, --keep            keep (do not delete) input files\n");
    printf("  -f, --force           force overwrite of output file\n");
    printf("  -1..-22               compression level (default 3)\n");
    printf("  -q, --quiet           suppress warnings\n");
    printf("  -v, --verbose         print file name and compression ratio\n");
    printf("  --rm                  remove source file after compression\n");
    printf("  -l, --list            list zstd file information\n");
    printf("  -D, --dict-id ID      dictionary ID for decompression\n");
    printf("  --no-progress         disable progress bar\n");
    printf("  -h, --help            display this help and exit\n");
    printf("      --version         display version and exit\n");
    printf("\n");
    printf("With no FILE, or when FILE is -, read standard input.\n");
}

void print_ratio(const std::string& name, size_t in_size,
                 size_t out_size, const char* replaced_with) {
    double ratio =
        in_size == 0
            ? 100.0
            : (1.0 - (double)out_size / (double)in_size) * 100.0;
    if (replaced_with) {
        printf("%s: %5.1f%% -- replaced with %s\n", name.c_str(), ratio,
               replaced_with);
    } else {
        printf("%s: %5.1f%%\n", name.c_str(), ratio);
    }
}

int process_path(const ZstdOptions& opt, const std::string& path,
                 const char* prog) {
    bool stdin_mode = (path == "-");

    if (stdin_mode) {
        std::vector<unsigned char> in;
        if (!read_all(stdin, in)) {
            fprintf(stderr, "%s: stdin: %s\n", prog, strerror(errno));
            return 1;
        }
        if (opt.decompress) {
            if (!is_zstd_stream(in)) {
                fprintf(stderr, "%s: stdin: Compressed data is corrupt\n", prog);
                return 1;
            }
            std::vector<unsigned char> out;
            if (!zstd_decompress(in, out)) {
                fprintf(stderr, "%s: stdin: Compressed data is corrupt\n", prog);
                return 1;
            }
            fwrite(out.data(), 1, out.size(), stdout);
            if (opt.verbose)
                print_ratio("-", in.size(), out.size(), nullptr);
            return 0;
        }
        std::vector<unsigned char> out;
        if (!zstd_compress(in, out, opt.level)) {
            fprintf(stderr, "%s: stdin: Compression failed\n", prog);
            return 1;
        }
        fwrite(out.data(), 1, out.size(), stdout);
        if (opt.verbose)
            print_ratio("-", in.size(), out.size(), nullptr);
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
        fprintf(stderr, "%s: %s: %s\n", prog, path.c_str(),
                strerror(read_errno));
        return 1;
    }
    // --list: show file info and return (does not compress/decompress)
    if (opt.list_info) {
        if (stdin_mode) {
            fprintf(stderr, "%s: stdin: --list requires a file\n", prog);
            return 1;
        }
        if (!opt.decompress && !ends_with(path, ".zst")) {
            fprintf(stderr, "%s: %s: not a zstd file\n", prog, path.c_str());
            return 1;
        }
        size_t content_size =
            ZSTD_getFrameContentSize(in.data(), in.size());
        printf("  %zu  %s  %zu bytes\n", in.size(), path.c_str(),
               content_size == ZSTD_CONTENTSIZE_ERROR ? 0 : content_size);
        return 0;
    }

    // Check if file ends with .zst (skip compress for already-compressed files)
    if (!opt.decompress && ends_with(path, ".zst")) {
        if (!opt.quiet) {
            fprintf(stderr, "%s: %s already has .zst suffix -- nothing done\n", prog, path.c_str());
        }
        return 0;
    }

    if (opt.decompress) {
        if (!is_zstd_stream(in)) {
            fprintf(stderr, "%s: %s: not in zstd format\n", prog,
                    path.c_str());
            return 1;
        }
        std::vector<unsigned char> out;
        if (!zstd_decompress(in, out)) {
            fprintf(stderr, "%s: %s: Compressed data is corrupt\n", prog,
                    path.c_str());
            return 1;
        }
        if (opt.to_stdout) {
            fwrite(out.data(), 1, out.size(), stdout);
            if (opt.verbose)
                print_ratio(path, in.size(), out.size(), nullptr);
            return 0;
        }
        // -dk: keep original .zst, write decompressed alongside it
        // -d (no -k): replace .zst with decompressed file
        std::string outpath = strip_zst(path);
        if (write_output_file(out, outpath, opt.force, prog) != 0) {
            return 1;
        }
        if (!opt.keep) {
            std::remove(path.c_str());
        }
        if (opt.verbose)
            print_ratio(path, in.size(), out.size(), outpath.c_str());
        return 0;
    }

    // Compress
    std::vector<unsigned char> out;
    if (!zstd_compress(in, out, opt.level)) {
        fprintf(stderr, "%s: %s: Compression failed\n", prog, path.c_str());
        return 1;
    }
    std::string outpath = path + ".zst";
    if (opt.to_stdout) {
        fwrite(out.data(), 1, out.size(), stdout);
        if (opt.verbose)
            print_ratio(path, in.size(), out.size(), nullptr);
        return 0;
    }
    if (write_output_file(out, outpath, opt.force, prog) != 0) {
        return 1;
    }
    if (!opt.keep || opt.rm_source) {
        std::remove(path.c_str());
    }
    if (opt.verbose)
        print_ratio(path, in.size(), out.size(), outpath.c_str());
    return 0;
}

} // namespace

int zstd_command(int argc, char** argv) {
    const char* prog = argv[0];

    ZstdOptions opt;

    // argtable3 cannot represent `-1`..`-22` as short options, so extract the
    // numeric level flags ourselves and pass the rest to argtable3.
    int pre_level = ZSTD_DEFAULT_LEVEL;
    bool saw_level = false;
    std::vector<std::string> owned;
    std::vector<const char*> cargv;
    cargv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.size() >= 2 && a[0] == '-' && a[1] >= '1' && a[1] <= '9') {
            // Parse the full numeric level from the flag (e.g. -12, -22).
            int level_val = 0;
            size_t digit_end = 1;
            for (; digit_end < a.size(); digit_end++) {
                if (a[digit_end] >= '0' && a[digit_end] <= '9') {
                    level_val = level_val * 10 + (a[digit_end] - '0');
                } else {
                    break;
                }
            }
            if (level_val >= 1 && level_val <= 22) {
                pre_level = level_val;
                saw_level = true;
                // Append any remaining short flags (e.g. -12v -> -v).
                if (digit_end < a.size()) {
                    owned.push_back("-" + a.substr(digit_end));
                    cargv.push_back(owned.back().c_str());
                }
            } else {
                cargv.push_back(argv[i]);
            }
        } else {
            cargv.push_back(argv[i]);
        }
    }

    struct arg_lit* opt_c = arg_lit0("c", "stdout", "write to stdout");
    struct arg_lit* opt_d = arg_lit0("d", "decompress", "decompress");
    struct arg_lit* opt_k = arg_lit0("k", "keep", "keep original files");
    struct arg_lit* opt_f = arg_lit0("f", "force", "force overwrite");
    struct arg_lit* opt_q = arg_lit0("q", "quiet", "suppress warnings");
    struct arg_lit* opt_v = arg_lit0("v", "verbose", "print ratio");
    struct arg_lit* opt_h = arg_lit0("h", "help", "show help");
    struct arg_lit* opt_version = arg_lit0(nullptr, "version", "show version");
    struct arg_lit* opt_rm = arg_lit0(nullptr, "rm", "remove source file");
    struct arg_lit* opt_l = arg_lit0("l", "list", "list file information");
    struct arg_lit* opt_np =
        arg_lit0(nullptr, "no-progress", "disable progress");
    struct arg_int* opt_level =
        arg_int0(nullptr, "level", "N", "compression level (1-22)");
    struct arg_int* opt_did =
        arg_int0("D", "dict-id", "ID", "dictionary ID");
    struct arg_file* files = arg_filen(nullptr, nullptr, "FILE...", 0, 1000,
                                       "files");
    struct arg_end* end = arg_end(20);

    std::vector<void*> table = {(void*)opt_c,   (void*)opt_d, (void*)opt_k,
                                (void*)opt_f,   (void*)opt_q, (void*)opt_v,
                                (void*)opt_h,   (void*)opt_version,
                                (void*)opt_rm,  (void*)opt_l,
                                (void*)opt_np,  (void*)opt_level,
                                (void*)opt_did, (void*)files, (void*)end};

    ArgTable tbl(table);
    int errors = tbl.parse((int)cargv.size(), (char**)cargv.data());
    if (errors != 0) {
        tbl.print_errors(end, prog);
        return 1;
    }

    if (opt_h->count > 0) {
        print_help(prog);
        return 0;
    }

    if (opt_version->count > 0) {
        print_version(prog);
        return 0;
    }

    opt.decompress = (opt_d->count > 0);
    opt.keep = (opt_k->count > 0);
    opt.force = (opt_f->count > 0);
    opt.quiet = (opt_q->count > 0);
    opt.verbose = (opt_v->count > 0);
    opt.to_stdout = (opt_c->count > 0);
    opt.rm_source = (opt_rm->count > 0);
    opt.list_info = (opt_l->count > 0);
    // --no-progress is a no-op: ZSTD_compress API has no progress callback.
    opt.no_progress = (opt_np->count > 0);
    // -D dict_id is stored but not wired to ZSTD_compress_usingDict (v2).
    if (saw_level) {
        opt.level = pre_level;
    } else if (opt_level->count > 0) {
        int lvl = opt_level->ival[0];
        if (lvl < 1 || lvl > 22) {
            fprintf(stderr, "%s: invalid compression level: %d\n", prog, lvl);
            return 1;
        }
        opt.level = lvl;
    }

    if (opt_did->count > 0) {
        opt.dict_id = opt_did->ival[0];
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

REGISTER_COMMAND("zstd", zstd_command, "Compress or decompress files with zstd");
