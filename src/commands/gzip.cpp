#include <zlib.h>

#include <argtable3.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "commands/arg_util.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/gzip.hpp"
#include "commands/version_util.hpp"

namespace {

// gzip container constants
constexpr unsigned char GZ_ID1 = 0x1f;
constexpr unsigned char GZ_ID2 = 0x8b;
constexpr unsigned char GZ_CM_DEFLATE = 8;
constexpr unsigned char GZ_FLG_FNAME = 0x08;
constexpr unsigned char GZ_FLG_FEXTRA = 0x04;
constexpr unsigned char GZ_FLG_FCOMMENT = 0x10;
constexpr unsigned char GZ_FLG_FHCRC = 0x02;
constexpr unsigned char GZ_OS_UNIX = 3;

// Read an entire stream (including stdin) into a buffer.
bool read_all(FILE* fp, std::vector<unsigned char>& out) {
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return !ferror(fp);
}

// Build a complete gzip container from `in` into `out`.
bool gzip_compress(const std::vector<unsigned char>& in,
                   std::vector<unsigned char>& out, int level,
                   const std::string& fname, unsigned long mtime) {
    out.clear();
    unsigned char xfl = (level == 9) ? 2 : (level == 1) ? 4 : 0;
    unsigned char flg = fname.empty() ? 0 : GZ_FLG_FNAME;

    out.push_back(GZ_ID1);
    out.push_back(GZ_ID2);
    out.push_back(GZ_CM_DEFLATE);
    out.push_back(flg);
    out.push_back((unsigned char)(mtime & 0xff));
    out.push_back((unsigned char)((mtime >> 8) & 0xff));
    out.push_back((unsigned char)((mtime >> 16) & 0xff));
    out.push_back((unsigned char)((mtime >> 24) & 0xff));
    out.push_back(xfl);
    out.push_back(GZ_OS_UNIX);
    if (flg & GZ_FLG_FNAME) {
        for (char c : fname) out.push_back((unsigned char)c);
        out.push_back(0);
    }

    z_stream strm{};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    // Raw deflate: negative windowBits (no zlib/gzip envelope) so we can write
    // the gzip header/trailer by hand.
    if (deflateInit2(&strm, level, Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }
    strm.next_in = (Bytef*)in.data();
    strm.avail_in = (uInt)in.size();
    unsigned char cbuf[65536];
    int rc;
    do {
        strm.next_out = cbuf;
        strm.avail_out = sizeof(cbuf);
        rc = deflate(&strm, Z_FINISH);
        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
            deflateEnd(&strm);
            return false;
        }
        size_t got = sizeof(cbuf) - strm.avail_out;
        out.insert(out.end(), cbuf, cbuf + got);
    } while (rc != Z_STREAM_END);
    deflateEnd(&strm);

    uLong crc = crc32(0L, in.data(), (uInt)in.size());
    unsigned long isize = (unsigned long)in.size();
    auto put32 = [&](unsigned long v) {
        out.push_back((unsigned char)(v & 0xff));
        out.push_back((unsigned char)((v >> 8) & 0xff));
        out.push_back((unsigned char)((v >> 16) & 0xff));
        out.push_back((unsigned char)((v >> 24) & 0xff));
    };
    put32(crc);
    put32(isize);
    return true;
}

// Decompression error classification so the caller can print the right message.
enum class GzErr { Ok, NotGzip, Truncated, Crc };

// Inflate a gzip container `in` into `out`, verifying CRC + ISIZE.
GzErr gzip_decompress(const std::vector<unsigned char>& in,
                      std::vector<unsigned char>& out) {
    if (in.size() < 10) return GzErr::NotGzip;
    if (in[0] != GZ_ID1 || in[1] != GZ_ID2 || in[2] != GZ_CM_DEFLATE) {
        return GzErr::NotGzip;
    }
    if (in.size() < 18) return GzErr::Truncated;

    unsigned char flg = in[3];
    size_t pos = 10;
    if (flg & GZ_FLG_FEXTRA) {
        if (in.size() < pos + 2) return GzErr::Truncated;
        unsigned xlen = (unsigned)in[pos] | ((unsigned)in[pos + 1] << 8);
        pos += 2 + xlen;
    }
    if (flg & GZ_FLG_FNAME) {
        while (pos < in.size() && in[pos] != 0) pos++;
        if (pos >= in.size()) return GzErr::Truncated;
        pos++;
    }
    if (flg & GZ_FLG_FCOMMENT) {
        while (pos < in.size() && in[pos] != 0) pos++;
        if (pos >= in.size()) return GzErr::Truncated;
        pos++;
    }
    if (flg & GZ_FLG_FHCRC) pos += 2;
    if (in.size() < pos + 8) return GzErr::Truncated;

    size_t trailer_pos = in.size() - 8;
    if (trailer_pos < pos) return GzErr::Truncated;

    z_stream strm{};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return GzErr::Truncated;
    strm.next_in = (Bytef*)in.data() + pos;
    strm.avail_in = (uInt)(trailer_pos - pos);
    std::vector<unsigned char> dec;
    unsigned char dbuf[65536];
    int rc;
    do {
        strm.next_out = dbuf;
        strm.avail_out = sizeof(dbuf);
        rc = inflate(&strm, Z_FINISH);
        size_t got = sizeof(dbuf) - strm.avail_out;
        // Copy this chunk (also covers the final chunk delivered on
        // Z_STREAM_END) before deciding whether we're done.
        dec.insert(dec.end(), dbuf, dbuf + got);
        if (rc == Z_STREAM_END) break;
        if (rc != Z_OK && rc != Z_BUF_ERROR) {
            inflateEnd(&strm);
            return GzErr::Truncated;
        }
        // Genuinely stuck: no input left to consume and no output produced on
        // this call means the stream is truncated (as opposed to a transient
        // output-buffer-full condition, where rc==Z_BUF_ERROR but got > 0).
        if (rc == Z_BUF_ERROR && strm.avail_in == 0 && got == 0) {
            inflateEnd(&strm);
            return GzErr::Truncated;
        }
    } while (true);



    inflateEnd(&strm);

    uLong crc = crc32(0L, dec.data(), (uInt)dec.size());
    unsigned long isize = (unsigned long)dec.size();
    unsigned long sc = (unsigned long)in[trailer_pos] |
                       ((unsigned long)in[trailer_pos + 1] << 8) |
                       ((unsigned long)in[trailer_pos + 2] << 16) |
                       ((unsigned long)in[trailer_pos + 3] << 24);
    unsigned long si = (unsigned long)in[trailer_pos + 4] |
                       ((unsigned long)in[trailer_pos + 5] << 8) |
                       ((unsigned long)in[trailer_pos + 6] << 16) |
                       ((unsigned long)in[trailer_pos + 7] << 24);
    if (crc != sc || isize != si) return GzErr::Crc;

    out = std::move(dec);
    return GzErr::Ok;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string base_name(const std::string& p) {
    size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

std::string strip_gz(const std::string& p) {
    return ends_with(p, ".gz") ? p.substr(0, p.size() - 3) : p;
}

// Write `data` to `outname` unless it exists and !force. Returns 0 on success.
int write_output_file(const std::vector<unsigned char>& data,
                      const std::string& outname, bool force,
                      const char* prog) {
    if (!force) {
        struct stat st;
        if (stat(outname.c_str(), &st) == 0) {
            fprintf(stderr, "%s: %s already exists; not overwritten\n", prog,
                    outname.c_str());
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

void print_ratio(const char* prog, const std::string& name, size_t in_size,
                 size_t out_size, const char* replaced_with) {
    double ratio = in_size == 0 ? 100.0 : (1.0 - (double)out_size / (double)in_size) * 100.0;
    if (replaced_with) {
        printf("%s: %5.1f%% -- replaced with %s\n", name.c_str(), ratio,
               replaced_with);
    } else {
        printf("%s: %5.1f%%\n", name.c_str(), ratio);
    }
    (void)prog;
}

int process_path(const GzipOptions& opt, const std::string& path,
                 const char* prog) {
    bool stdin_mode = (path == "-");

    if (stdin_mode) {
        std::vector<unsigned char> in;
        if (!read_all(stdin, in)) {
            fprintf(stderr, "%s: stdin: %s\n", prog, strerror(errno));
            return 1;
        }
        if (opt.decompress) {
            std::vector<unsigned char> out;
            GzErr e = gzip_decompress(in, out);
            if (e != GzErr::Ok) {
                const char* msg = e == GzErr::NotGzip ? "not in gzip format"
                                  : e == GzErr::Crc
                                      ? "invalid compressed data--crc error"
                                      : "unexpected end of file";
                fprintf(stderr, "%s: stdin: %s\n", prog, msg);
                return 1;
            }
            fwrite(out.data(), 1, out.size(), stdout);
            if (opt.verbose) print_ratio(prog, "-", in.size(), out.size(), nullptr);
            return 0;
        }
        std::vector<unsigned char> out;
        if (!gzip_compress(in, out, opt.level, "", 0)) {
            fprintf(stderr, "%s: stdin: compression failed\n", prog);
            return 1;
        }
        fwrite(out.data(), 1, out.size(), stdout);
        if (opt.verbose) print_ratio(prog, "-", in.size(), out.size(), nullptr);
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
        GzErr e = gzip_decompress(in, out);
        if (e != GzErr::Ok) {
            const char* msg = e == GzErr::NotGzip ? "not in gzip format"
                              : e == GzErr::Crc
                                  ? "invalid compressed data--crc error"
                                  : "unexpected end of file";
            fprintf(stderr, "%s: %s: %s\n", prog, path.c_str(), msg);
            return 1;
        }
        if (opt.to_stdout) {
            fwrite(out.data(), 1, out.size(), stdout);
            if (!opt.keep) std::remove(path.c_str());
            if (opt.verbose) print_ratio(prog, path, in.size(), out.size(), nullptr);
            return 0;
        }
        std::string outname = strip_gz(path);
        if (write_output_file(out, outname, opt.force, prog) != 0) return 1;
        if (!opt.keep) std::remove(path.c_str());
        if (opt.verbose) {
            print_ratio(prog, path, in.size(), out.size(), outname.c_str());
        }
        return 0;
    }

    // compress
    if (ends_with(path, ".gz")) {
        if (!opt.quiet) {
            fprintf(stderr, "%s: %s already has .gz suffix -- unchanged\n", prog,
                    path.c_str());
        }
        return 0;
    }
    struct stat st;
    unsigned long mtime = (stat(path.c_str(), &st) == 0)
                              ? (unsigned long)st.st_mtime
                              : 0;
    std::string fname = base_name(path);
    if (ends_with(fname, ".gz")) fname = fname.substr(0, fname.size() - 3);

    std::vector<unsigned char> out;
    if (!gzip_compress(in, out, opt.level, fname, mtime)) {
        fprintf(stderr, "%s: %s: compression failed\n", prog, path.c_str());
        return 1;
    }

    if (opt.to_stdout) {
        fwrite(out.data(), 1, out.size(), stdout);
        if (opt.verbose) {
            std::string outname = path + ".gz";
            print_ratio(prog, path, in.size(), out.size(), outname.c_str());
        }
        return 0;
    }

    std::string outname = path + ".gz";
    if (write_output_file(out, outname, opt.force, prog) != 0) return 1;
    if (!opt.keep) std::remove(path.c_str());
    if (opt.verbose) print_ratio(prog, path, in.size(), out.size(), outname.c_str());
    return 0;
}

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [FILE]...\n", prog);
    printf("Compress or decompress FILEs with gzip.\n");
    printf("\n");
    printf("  -c, --stdout      write to stdout, keep original files\n");
    printf("  -d, --decompress, --uncompress\n");
    printf("                    decompress\n");
    printf("  -k, --keep        keep (do not delete) input files\n");
    printf("  -f, --force       force overwrite of output file\n");
    printf("  -1..-9            compression level (1=fast, 9=best, default 6)\n");
    printf("      --fast        alias for level 1\n");
    printf("      --best        alias for level 9\n");
    printf("  -q, --quiet       suppress warnings\n");
    printf("  -v, --verbose     print file name and compression ratio\n");
    printf("  -h, --help        display this help and exit\n");
    printf("      --version     display version and exit\n");
    printf("\n");
    printf("With no FILE, or when FILE is -, read standard input.\n");
}

} // namespace

int gzip_command(int argc, char** argv) {
    struct arg_lit* opt_c = arg_lit0("c", "stdout", "write to stdout");
    struct arg_lit* opt_d = arg_lit0("d", "decompress,uncompress", "decompress");
    struct arg_lit* opt_k = arg_lit0("k", "keep", "keep input files");
    struct arg_lit* opt_f = arg_lit0("f", "force", "force overwrite");
    struct arg_lit* opt_q = arg_lit0("q", "quiet", "suppress warnings");
    struct arg_lit* opt_v = arg_lit0("v", "verbose", "verbose");
    struct arg_lit* opt_fast = arg_lit0(NULL, "fast", "alias for level 1");
    struct arg_lit* opt_best = arg_lit0(NULL, "best", "alias for level 9");
    struct arg_lit* opt_h = arg_lit0("h", "help", "show help");
    struct arg_lit* opt_ver = arg_lit0(NULL, "version", "show version");
    struct arg_file* files = arg_filen(NULL, NULL, "FILE...", 0, 1000, "files");
    struct arg_end* end = arg_end(20);

    std::vector<void*> table = {opt_c,  opt_d,  opt_k,   opt_f,   opt_q,
                                opt_v,  opt_fast, opt_best, opt_h,  opt_ver,
                                files, end};

    // argtable3 cannot represent `-1`..`-9` as short options, so extract the
    // numeric level flags ourselves and pass the rest to argtable3. A `-N`
    // followed by other short flags (e.g. `-9v`) is split into `-N` + `-v`.
    int pre_level = 6;
    bool saw_level = false;
    std::vector<std::string> owned;
    std::vector<const char*> cargv;
    cargv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
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
    int nerrors = at.parse((int)cargv.size(), (char**)cargv.data());

    if (opt_h->count > 0) {
        print_help(argv[0]);
        return 0;
    }
    if (opt_ver->count > 0) {
        print_version("gzip");
        return 0;
    }
    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    GzipOptions opt;
    opt.decompress = opt_d->count > 0;
    opt.keep = opt_k->count > 0;
    opt.to_stdout = opt_c->count > 0;
    opt.force = opt_f->count > 0;
    opt.quiet = opt_q->count > 0;
    opt.verbose = opt_v->count > 0;
    opt.level = 6;
    if (opt_best->count > 0) opt.level = 9;
    if (opt_fast->count > 0) opt.level = 1;
    if (saw_level) opt.level = pre_level;

    std::vector<std::string> paths;
    for (int i = 0; i < files->count; i++) {
        paths.push_back(files->filename[i]);
    }
    if (paths.empty()) paths.push_back("-");

    int status = 0;
    for (const auto& p : paths) {
        if (process_path(opt, p, argv[0]) != 0) status = 1;
    }
    return status;
}

REGISTER_COMMAND("gzip", gzip_command, "Compress or decompress files with gzip")
