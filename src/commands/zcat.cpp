#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/compress_util.hpp"
#include "commands/version_util.hpp"
#include "commands/zcat.hpp"

// Gzip container constants (copied from gzip.cpp)
constexpr unsigned char GZ_ID1 = 0x1f;
constexpr unsigned char GZ_ID2 = 0x8b;
constexpr unsigned char GZ_CM_DEFLATE = 8;
constexpr unsigned char GZ_FLG_FNAME = 0x08;
constexpr unsigned char GZ_FLG_FEXTRA = 0x04;
constexpr unsigned char GZ_FLG_FCOMMENT = 0x10;
constexpr unsigned char GZ_FLG_FHCRC = 0x02;

#include <zlib.h>

enum class GzErr { Ok, NotGzip, Truncated, Crc };

// Decompress a gzip container `in` into `out`, verifying CRC + ISIZE.
GzErr gzip_decompress(const std::vector<unsigned char>& in,
                      std::vector<unsigned char>& out) {
    if (in.size() < 10) { return GzErr::NotGzip; }
    if (in[0] != GZ_ID1 || in[1] != GZ_ID2 || in[2] != GZ_CM_DEFLATE) {
        return GzErr::NotGzip;
    }
    if (in.size() < 18) { return GzErr::Truncated; }

    unsigned char const flg = in[3];
    size_t pos = 10;
    if ((flg & GZ_FLG_FEXTRA) != 0) {
        if (in.size() < pos + 2) { return GzErr::Truncated; }
        unsigned const xlen = static_cast<unsigned>(in[pos]) | (static_cast<unsigned>(in[pos + 1]) << 8);
        pos += 2 + xlen;
    }
    if ((flg & GZ_FLG_FNAME) != 0) {
        while (pos < in.size() && in[pos] != 0) { pos++; }
        if (pos >= in.size()) { return GzErr::Truncated; }
        pos++;
    }
    if ((flg & GZ_FLG_FCOMMENT) != 0) {
        while (pos < in.size() && in[pos] != 0) { pos++; }
        if (pos >= in.size()) { return GzErr::Truncated; }
        pos++;
    }
    if ((flg & GZ_FLG_FHCRC) != 0) { pos += 2; }
    if (in.size() < pos + 8) { return GzErr::Truncated; }

    size_t const trailer_pos = in.size() - 8;
    if (trailer_pos < pos) { return GzErr::Truncated; }

    z_stream strm{};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) { return GzErr::Truncated; }
    strm.next_in = const_cast<Bytef*>(in.data()) + pos;
    strm.avail_in = static_cast<uInt>(trailer_pos - pos);
    std::vector<unsigned char> dec;
    unsigned char dbuf[65536];
    int rc;
    do {
        strm.next_out = dbuf;
        strm.avail_out = sizeof(dbuf);
        rc = inflate(&strm, Z_FINISH);
        size_t const got = sizeof(dbuf) - strm.avail_out;
        dec.insert(dec.end(), dbuf, dbuf + got);
        if (rc == Z_STREAM_END) { break; }
        if (rc != Z_OK && rc != Z_BUF_ERROR) {
            inflateEnd(&strm);
            return GzErr::Truncated;
        }
        if (rc == Z_BUF_ERROR && strm.avail_in == 0 && got == 0) {
            inflateEnd(&strm);
            return GzErr::Truncated;
        }
    } while (true);

    inflateEnd(&strm);

    uLong const crc = crc32(0L, dec.data(), static_cast<uInt>(dec.size()));
    unsigned long const isize = static_cast<unsigned long>(dec.size());
    unsigned long const sc = static_cast<unsigned long>(in[trailer_pos]) |
                       (static_cast<unsigned long>(in[trailer_pos + 1]) << 8) |
                       (static_cast<unsigned long>(in[trailer_pos + 2]) << 16) |
                       (static_cast<unsigned long>(in[trailer_pos + 3]) << 24);
    unsigned long const si = static_cast<unsigned long>(in[trailer_pos + 4]) |
                       (static_cast<unsigned long>(in[trailer_pos + 5]) << 8) |
                       (static_cast<unsigned long>(in[trailer_pos + 6]) << 16) |
                       (static_cast<unsigned long>(in[trailer_pos + 7]) << 24);
    if (crc != sc || isize != si) { return GzErr::Crc; }

    out = std::move(dec);
    return GzErr::Ok;
}

void print_help(const char* prog) {
    printf("Usage: %s [FILE]...\n", prog);
    printf("Decompress gzip files to standard output.\n");
    printf("\n");
    printf("  -h, --help    display this help and exit\n");
    printf("      --version display version and exit\n");
    printf("\n");
    printf("With no FILE, or when FILE is -, read standard input.\n");
}

int process_file(const std::string& path, const char* prog) {
    bool const stdin_mode = (path == "-");

    if (stdin_mode) {
        std::vector<unsigned char> in;
        if (!compress_util::read_all(stdin, in)) {
            (void)fprintf(stderr, "%s: stdin: %s\n", prog, strerror(errno));
            return 1;
        }
        std::vector<unsigned char> out;
        GzErr const e = gzip_decompress(in, out);
        if (e != GzErr::Ok) {
            const char* msg = e == GzErr::NotGzip ? "not in gzip format"
                                  : e == GzErr::Crc
                                      ? "invalid compressed data--crc error"
                                      : "unexpected end of file";
            (void)fprintf(stderr, "%s: stdin: %s\n", prog, msg);
            return 1;
        }
        (void)fwrite(out.data(), 1, out.size(), stdout);
        return 0;
    }

    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        cmd_perror(prog, path.c_str());
        return 1;
    }
    std::vector<unsigned char> in;
    bool const read_ok = compress_util::read_all(fp, in);
    int const read_errno = errno;
    (void)fclose(fp);
    if (!read_ok) {
        (void)fprintf(stderr, "%s: %s: %s\n", prog, path.c_str(), strerror(read_errno));
        return 1;
    }

    std::vector<unsigned char> out;
    GzErr const e = gzip_decompress(in, out);
    if (e != GzErr::Ok) {
        const char* msg = e == GzErr::NotGzip ? "not in gzip format"
                              : e == GzErr::Crc
                                  ? "invalid compressed data--crc error"
                                  : "unexpected end of file";
        (void)fprintf(stderr, "%s: %s: %s\n", prog, path.c_str(), msg);
        return 1;
    }

    (void)fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}

int zcat_command(int argc, char** argv) {
    const char* prog = argv[0];

    // Parse arguments
    struct arg_lit* opt_h = arg_lit0("h", "help", "display help");
    struct arg_lit* opt_version = arg_lit0(nullptr, "version", "show version");
    struct arg_file* files = arg_filen(nullptr, nullptr, "FILE...", 0, 1000, "files");
    struct arg_end* end = arg_end(20);

    std::vector<void*> const table = {
        reinterpret_cast<void*>(opt_h),
        reinterpret_cast<void*>(opt_version),
        reinterpret_cast<void*>(files),
        reinterpret_cast<void*>(end)
    };

    ArgTable tbl(table);
    int const errors = tbl.parse(argc, argv);
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

    // Collect file paths
    std::vector<std::string> paths;
    for (int i = 0; i < files->count; i++) {
        if (files->filename[i] != nullptr) {
            paths.push_back(files->filename[i]);
        }
    }

    if (paths.empty()) {
        paths.push_back("-");
    }

    int status = 0;
    for (const auto& path : paths) {
        int const ret = process_file(path, prog);
        if (ret != 0) { status = 1; }
    }
    return status;
}

REGISTER_COMMAND("zcat", zcat_command, "Decompress gzip files to standard output");
