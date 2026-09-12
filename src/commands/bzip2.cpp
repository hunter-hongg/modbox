#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <bzlib.h>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/bzip2.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/compress_util.hpp"
#include "commands/version_util.hpp"

namespace {

// Incremental input buffer size for the streaming codec.
constexpr int BZ_READ_CHUNK = 65536;
// Incremental output buffer size; must hold one decoded block plus slop.
constexpr unsigned BZ_WRITE_CHUNK = 1U << 20U;
constexpr int BZ_BLOCK_SIZE_100K = 1;

std::string base_name_of(const std::string& p) {
    size_t const slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

// Compress `in` into `out` using libbz2's streaming encoder. `level` is the
// classic bzip2 block size 1..9 (100k..900k), clamped to a safe range.
bool bz_compress(const std::vector<unsigned char>& in,
                 std::vector<unsigned char>& out, int level) {
    level = std::clamp(level, 1, 9);

    bz_stream strm{};
    if (BZ2_bzCompressInit(&strm, level, 0, 0) != BZ_OK) { return false; }

    out.clear();
    strm.next_in =
        reinterpret_cast<char*>(const_cast<unsigned char*>(in.data()));
    strm.avail_in = static_cast<unsigned int>(in.size());

    std::vector<char> buf(BZ_WRITE_CHUNK);
    int rc = BZ_RUN_OK;
    do {
        strm.next_out = buf.data();
        strm.avail_out = BZ_WRITE_CHUNK;
        rc = BZ2_bzCompress(&strm, BZ_FINISH);
        if (rc != BZ_FINISH_OK && rc != BZ_STREAM_END) {
            BZ2_bzCompressEnd(&strm);
            return false;
        }
        size_t const got = BZ_WRITE_CHUNK - strm.avail_out;
        out.insert(out.end(), buf.data(), buf.data() + got);
    } while (rc != BZ_STREAM_END);

    BZ2_bzCompressEnd(&strm);
    return true;
}

// Decompress a (possibly concatenated) bzip2 stream from `in` into `out`.
// Returns BZ_OK on success, or the libbz2 error code. Concatenated streams —
// which upstream bunzip2 joins — are handled by reinitializing the decoder when
// a stream ends with input still pending.
int bz_decompress(const std::vector<unsigned char>& in,
                  std::vector<unsigned char>& out) {
    out.clear();
    size_t consumed = 0;

    while (consumed < in.size()) {
        bz_stream strm{};
        int const init = BZ2_bzDecompressInit(&strm, 0, 0);
        if (init != BZ_OK) { return init; }

        strm.next_in =
            reinterpret_cast<char*>(const_cast<unsigned char*>(in.data()) +
                                    consumed);
        strm.avail_in = static_cast<unsigned int>(in.size() - consumed);

        std::vector<char> buf(BZ_WRITE_CHUNK);
        int rc = BZ_OK;
        do {
            strm.next_out = buf.data();
            strm.avail_out = BZ_WRITE_CHUNK;
            rc = BZ2_bzDecompress(&strm);
            size_t const got = BZ_WRITE_CHUNK - strm.avail_out;
            out.insert(out.end(), buf.data(), buf.data() + got);
            if (rc != BZ_OK && rc != BZ_STREAM_END) {
                BZ2_bzDecompressEnd(&strm);
                return rc;
            }
        } while (rc != BZ_STREAM_END);

        // bz_stream reports how much input remains; derive how much of the
        // whole buffer has been consumed, then try again in case more
        // concatenated streams follow.
        consumed = in.size() - strm.avail_in;
        BZ2_bzDecompressEnd(&strm);

        // Trailing bytes that are not another stream (e.g. padding) would make
        // the next init fail; stop when only whitespace-like slack remains.
        if (consumed >= in.size()) { break; }
    }

    return BZ_OK;
}

// Translate a libbz2 error code into the diagnostic upstream bzip2 prints.
const char* bz_error_message(int rc) {
    switch (rc) {
        case BZ_DATA_ERROR:
            return "data integrity (CRC) error in data";
        case BZ_DATA_ERROR_MAGIC:
            return "not a bzip2 file";
        case BZ_UNEXPECTED_EOF:
            return "unexpected end of file";
        case BZ_MEM_ERROR:
            return "out of memory";
        case BZ_CONFIG_ERROR:
            return "library configuration error";
        default:
            return "decompression failed";
    }
}

struct Bzip2Options {
    bool decompress = false;
    bool keep = false;
    bool to_stdout = false;
    bool force = false;
    bool quiet = false;
    bool verbose = false;
    bool test = false;
    int level = 9;
};

bool ends_with_bz2(const std::string& p) {
    return compress_util::ends_with(p, ".bz2");
}

std::string strip_bz2(const std::string& p) {
    return compress_util::strip_suffix(p, ".bz2");
}

int report(int rc, const char* prog, const std::string& label) {
    (void)fprintf(stderr, "%s: %s: %s\n", prog, label.c_str(),
                  bz_error_message(rc));
    return 1;
}

// Process a single input path ("-" means stdin). Returns 0 on success.
bool read_input(const std::string& path, bool stdin_mode,
                std::vector<unsigned char>& in, const char* prog) {
    if (stdin_mode) {
        if (!compress_util::read_all(stdin, in)) {
            (void)fprintf(stderr, "%s: stdin: %s\n", prog, strerror(errno));
            return false;
        }
        return true;
    }
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        cmd_perror(prog, path.c_str());
        return false;
    }
    bool const ok = compress_util::read_all(fp, in);
    int const read_errno = errno;
    (void)fclose(fp);
    if (!ok) {
        (void)fprintf(stderr, "%s: %s: %s\n", prog, path.c_str(),
                      strerror(read_errno));
        return false;
    }
    return true;
}

// Emit `data` to stdout (used by -c, stdin, and bzcat).
void write_stdout(const std::vector<unsigned char>& data) {
    (void)fwrite(data.data(), 1, data.size(), stdout);
}

int do_decompress(const Bzip2Options& opt, const std::string& path,
                  bool stdin_mode, const std::vector<unsigned char>& in,
                  const char* prog) {
    std::vector<unsigned char> out;
    int const rc = bz_decompress(in, out);
    if (rc != BZ_OK) {
        return report(rc, prog, stdin_mode ? "(stdin)" : path);
    }
    if (opt.test) {
        if (opt.verbose) {
            printf("  %s: ok\n", stdin_mode ? "(stdin)" : path.c_str());
        }
        return 0;
    }
    if (stdin_mode || opt.to_stdout) {
        write_stdout(out);
        return 0;
    }
    std::string outname;
    if (ends_with_bz2(path)) {
        outname = strip_bz2(path);
    } else {
        outname = path + ".out";
        if (!opt.quiet) {
            (void)fprintf(stderr,
                          "%s: Can't guess original name for %s -- using %s\n",
                          prog, path.c_str(), outname.c_str());
        }
    }
    if (compress_util::write_output_file(out, outname, opt.force, prog) != 0) {
        return 1;
    }
    if (!opt.keep) { (void)std::remove(path.c_str()); }
    if (opt.verbose) {
        compress_util::print_ratio(path, in.size(), out.size(),
                                   outname.c_str());
    }
    return 0;
}

int do_compress(const Bzip2Options& opt, const std::string& path,
                bool stdin_mode, const std::vector<unsigned char>& in,
                const char* prog) {
    // A .bz2 input is already compressed; refuse (matches upstream).
    if (!stdin_mode && ends_with_bz2(path)) {
        if (!opt.quiet) {
            (void)fprintf(stderr,
                          "%s: Input file %s already has .bz2 suffix.\n", prog,
                          path.c_str());
        }
        return 1;
    }
    std::vector<unsigned char> out;
    if (!bz_compress(in, out, opt.level)) {
        (void)fprintf(stderr, "%s: %s: compression failed\n", prog,
                      stdin_mode ? "(stdin)" : path.c_str());
        return 1;
    }
    if (stdin_mode || opt.to_stdout) {
        write_stdout(out);
        if (opt.verbose) {
            compress_util::print_ratio(stdin_mode ? "-" : path, in.size(),
                                       out.size(), nullptr);
        }
        return 0;
    }
    std::string const outname = path + ".bz2";
    if (compress_util::write_output_file(out, outname, opt.force, prog) != 0) {
        return 1;
    }
    if (!opt.keep) { (void)std::remove(path.c_str()); }
    if (opt.verbose) {
        compress_util::print_ratio(path, in.size(), out.size(),
                                   outname.c_str());
    }
    return 0;
}

int process_path(const Bzip2Options& opt, const std::string& path,
                 const char* prog) {
    bool const stdin_mode = (path == "-");
    std::vector<unsigned char> in;
    if (!read_input(path, stdin_mode, in, prog)) { return 1; }
    return opt.decompress
               ? do_decompress(opt, path, stdin_mode, in, prog)
               : do_compress(opt, path, stdin_mode, in, prog);
}

void print_help(const char* prog, const std::string& name) {
    printf("Usage: %s [OPTION]... [FILE]...\n", prog);
    printf("Compress or decompress FILEs with bzip2.\n");
    printf("\n");
    printf("  -d, --decompress   force decompression\n");
    printf("  -z, --compress     force compression\n");
    printf("  -c, --stdout       write to standard output, keep input files\n");
    printf("  -k, --keep         keep (do not delete) input files\n");
    printf("  -f, --force        overwrite existing output files\n");
    printf("  -t, --test         test compressed file integrity\n");
    printf("  -q, --quiet        suppress noncritical error messages\n");
    printf("  -v, --verbose      be verbose\n");
    printf("  -1 .. -9           set block size to 100k .. 900k (default 9)\n");
    printf("      --fast         alias for -1\n");
    printf("      --best         alias for -9\n");
    printf("  -h, --help         display this help and exit\n");
    printf("      --version      output version information and exit\n");
    printf("\n");
    printf("With no FILE, or when FILE is -, read standard input.\n");
    const char* action = "compression";
    if (name == "bunzip2") {
        action = "decompression";
    } else if (name == "bzcat") {
        action = "decompression to standard output";
    }
    printf("If invoked as '%s', the default action is %s.\n", name.c_str(),
           action);
}

// Derive the default action from the invocation name (argv[0] basename), so the
// bzip2/bunzip2/bzcat multi-call convention works via symlinks.
void apply_name_defaults(const std::string& name, Bzip2Options& opt) {
    if (name == "bunzip2") {
        opt.decompress = true;
    } else if (name == "bzcat") {
        opt.decompress = true;
        opt.to_stdout = true;
    }
}

} // namespace

static int bzip2_command_impl(const char* prog, const std::string& name,
                              int argc, char** argv) {
    struct arg_lit* opt_d = arg_lit0("d", "decompress", "decompress");
    struct arg_lit* opt_z = arg_lit0("z", "compress", "compress");
    struct arg_lit* opt_c = arg_lit0("c", "stdout", "write to stdout");
    struct arg_lit* opt_k = arg_lit0("k", "keep", "keep input files");
    struct arg_lit* opt_f = arg_lit0("f", "force", "force overwrite");
    struct arg_lit* opt_t = arg_lit0("t", "test", "test integrity");
    struct arg_lit* opt_q = arg_lit0("q", "quiet", "suppress warnings");
    struct arg_lit* opt_v = arg_lit0("v", "verbose", "be verbose");
    struct arg_lit* opt_s = arg_lit0("s", "small", "use less memory");
    struct arg_lit* opt_fast = arg_lit0(nullptr, "fast", "alias for level 1");
    struct arg_lit* opt_best = arg_lit0(nullptr, "best", "alias for level 9");
    struct arg_lit* opt_h = arg_lit0("h", "help", "display help");
    struct arg_lit* opt_ver = arg_lit0(nullptr, "version", "show version");
    struct arg_file* files =
        arg_filen(nullptr, nullptr, "FILE...", 0, 1000, "files");
    struct arg_end* end = arg_end(20);

    std::vector<void*> const table = {
        opt_d,  opt_z,   opt_c,  opt_k,  opt_f,   opt_t,  opt_q,
        opt_v,  opt_s,   opt_fast, opt_best, opt_h, opt_ver, files,
        end};

    // argtable3 cannot express `-1`..`-9` as short options, so lift the numeric
    // level flags out by hand (mirrors gzip.cpp), splitting bundled forms like
    // `-9v` into `-9` + `-v`.
    int pre_level = 9;
    bool saw_level = false;
    std::vector<std::string> owned;
    std::vector<const char*> cargv;
    cargv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        std::string const a = argv[i];
        if (a.size() >= 2 && a[0] == '-' && a[1] >= '1' && a[1] <= '9') {
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

    ArgTable at(table);
    int const nerrors =
        at.parse(static_cast<int>(cargv.size()),
                 const_cast<char**>(cargv.data()));

    if (opt_h->count > 0) {
        print_help(prog, name);
        return 0;
    }
    if (opt_ver->count > 0) {
        print_version(prog);
        return 0;
    }
    if (nerrors > 0) {
        return print_arg_errors(end, prog);
    }

    Bzip2Options opt;
    apply_name_defaults(name, opt);
    // An explicit -d/-z overrides the name-derived default.
    if (opt_d->count > 0) { opt.decompress = true; }
    if (opt_z->count > 0) { opt.decompress = false; }
    opt.keep = opt_k->count > 0;
    opt.to_stdout = opt.to_stdout || opt_c->count > 0;
    opt.force = opt_f->count > 0;
    opt.test = opt_t->count > 0;
    // -t checks the compressed input, so it implies decompression; upstream
    // bzip2 treats it as an opaque "-d with no output" too.
    if (opt.test) { opt.decompress = true; }
    opt.quiet = opt_q->count > 0;
    opt.verbose = opt_v->count > 0;
    (void)opt_s; // accepted for compatibility; memory use is bounded anyway
    opt.level = 9;
    if (opt_fast->count > 0) { opt.level = 1; }
    if (opt_best->count > 0) { opt.level = 9; }
    if (saw_level) { opt.level = pre_level; }

    std::vector<std::string> paths;
    for (int i = 0; i < files->count; i++) {
        paths.emplace_back(files->filename[i]);
    }
    if (paths.empty()) { paths.emplace_back("-"); }

    int status = 0;
    for (const auto& p : paths) {
        if (process_path(opt, p, prog) != 0) { status = 1; }
    }
    return status;
}

// Thin wrappers give each shipped name a distinct function so the
// REGISTER_COMMAND macro's static initializer name is unique per name. The
// prog string is the name used in diagnostics; `name` selects the default
// action, so symlinked invocation (argv[0] == bunzip2) still behaves right.
int bzip2_command(int argc, char** argv) {
    std::string const invoked = base_name_of(argv[0]);
    if (invoked == "bunzip2") { return bzip2_command_impl("bunzip2", "bunzip2", argc, argv); }
    if (invoked == "bzcat") { return bzip2_command_impl("bzcat", "bzcat", argc, argv); }
    return bzip2_command_impl("bzip2", "bzip2", argc, argv);
}

static int bunzip2_command(int argc, char** argv) {
    return bzip2_command_impl("bunzip2", "bunzip2", argc, argv);
}

static int bzcat_command(int argc, char** argv) {
    return bzip2_command_impl("bzcat", "bzcat", argc, argv);
}

REGISTER_COMMAND("bzip2", bzip2_command, "Compress or decompress files with bzip2")
REGISTER_COMMAND("bunzip2", bunzip2_command, "Decompress bzip2 files")
REGISTER_COMMAND("bzcat", bzcat_command, "Decompress bzip2 files to standard output")
