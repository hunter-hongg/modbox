// modbox tar — create / extract / list tar archives.
// Formats: ustar, GNU (L/K longname), pax (x/g). Streaming compression -z/-j/-J.
// See docs/specs/tar-command.md and .scratch/tar/issues/

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <cstdarg>

#include <zlib.h>
#include <lzma.h>
#include <zstd.h>

#include "commands/command_macros.hpp"
#include "commands/tar.hpp"
#include "commands/version_util.hpp"
#include "zconf.h"

namespace fs = std::filesystem;

namespace {

constexpr size_t BLOCK_SIZE = 512;

enum class Op { None, Create, Extract, List };

struct TarOptions {
    Op op = Op::None;
    std::string file;
    bool compress_gz = false;
    bool compress_xz = false;
    bool compress_zst = false;
    bool preserve_owner = false;
    bool no_same_owner = false;
    bool format_pax = false;
    std::vector<std::string> chdirs;
    std::vector<std::string> sources;
    std::vector<std::string> patterns;
    std::string extract_dir;
};

struct TarHeader {
    char name[101] = {0};
    int mode = 0;
    int uid = 0;
    int gid = 0;
    long long size = 0;
    int mtime = 0;
    char typeflag = 0;
    char linkname[101] = {0};
    std::string path;
    std::string linkpath;
    long long pax_mtime = -1;
    std::string uname;
    std::string gname;
    long long pax_uid = -1;
    long long pax_gid = -1;

    [[nodiscard]] std::string member_name() const { return path.empty() ? name : path; }
    [[nodiscard]] std::string member_link() const { return linkpath.empty() ? linkname : linkpath; }
};

int tar_err(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    (void)fprintf(stderr, "tar: ");
    (void)vfprintf(stderr, fmt, ap);
    (void)fprintf(stderr, "\n");
    va_end(ap);
    return 1;
}
int tar_perr(const char* what) {
    (void)fprintf(stderr, "tar: %s: %s\n", what, std::strerror(errno));
    return 1;
}

long long field_get(const unsigned char* p, size_t len) {
    if (len == 0) { return 0;
}
    if ((p[0] & 0x80) != 0) {
        long long v = 0;
        for (size_t i = 0; i < len; i++) {
            unsigned char const c = p[i];
            if (c == 0) { break;
}
            v = (v << 8) | (c & 0x7f);
        }
        return v;
    }
    char buf[16];
    size_t n = 0;
    for (; n < len && n < 15 && p[n] != 0; n++) { buf[n] = static_cast<char>(p[n]);
}
    buf[n] = 0;
    if (n == 0) { return 0;
}
    return std::strtoll(buf, nullptr, 8);
}

void field_put(unsigned char* dst, size_t len, long long v) {
    v = std::max<long long>(v, 0);
    long long limit = (1LL << 33);
    if (len <= 8) { limit = (1LL << 29);
    } else if (len >= 12) { limit = (1LL << 39);
}
    if (v < limit) {
        (void)snprintf(reinterpret_cast<char*>(dst), len, "%0*lo", static_cast<int>(len - 1), static_cast<unsigned long>(v));
    } else {
        dst[0] = 0x80;
        for (size_t i = len; i-- > 1;) {
            dst[i] = static_cast<unsigned char>(v & 0x7f);
            v >>= 7;
        }
    }
    dst[len - 1] = 0;
}

void header_from_bytes(const unsigned char* b, TarHeader& h) {
    auto strn = [&](size_t off, size_t len, char* dst, size_t cap) {
        size_t n = 0;
        for (; n < len && n < cap - 1 && b[off + n] != 0; n++) { dst[n] = static_cast<char>(b[off + n]);
}
        dst[n] = 0;
    };
    strn(0, 100, h.name, sizeof(h.name));
    h.mode = static_cast<int>(field_get(b + 100, 8));
    h.uid = static_cast<int>(field_get(b + 108, 8));
    h.gid = static_cast<int>(field_get(b + 116, 8));
    h.size = field_get(b + 124, 12);
    h.mtime = static_cast<int>(field_get(b + 136, 12));
    h.typeflag = static_cast<char>(b[156]);
    strn(157, 100, h.linkname, sizeof(h.linkname));
    { char tmp[33]; strn(265, 32, tmp, sizeof(tmp)); h.uname = tmp; }
    { char tmp[33]; strn(297, 32, tmp, sizeof(tmp)); h.gname = tmp; }
    h.pax_mtime = -1;
}

void apply_prefix(const unsigned char* b, TarHeader& h) {
    char prefix[156] = {0};
    size_t n = 0;
    for (; n < 155 && b[345 + n] != 0; n++) { prefix[n] = static_cast<char>(b[345 + n]);
}
    prefix[n] = 0;
    if ((prefix[0] != 0) && std::memcmp(b + 257, "ustar", 5) == 0) {
        h.path = std::string(prefix) + "/" + h.name;
    }
}

unsigned int header_checksum(const unsigned char* b) {
    unsigned int sum = 0;
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        sum += (i >= 148 && i < 156) ? 0x20 : b[i];
    }
    return sum;
}

bool header_is_end(const unsigned char* b) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) { if (b[i] != 0) { return false;
}
}
    return true;
}

bool header_is_valid(const unsigned char* b, const std::string& file) {
    if (std::memcmp(b + 257, "ustar", 5) != 0) {
        (void)fprintf(stderr, "tar: %s: not a tar archive (bad magic)\n", file.c_str());
        return false;
    }
    unsigned int const sum = header_checksum(b);
    unsigned int const stored = static_cast<unsigned int>(field_get(b + 148, 8));
    if (sum != stored) {
        (void)fprintf(stderr, "tar: %s: header checksum mismatch\n", file.c_str());
        return false;
    }
    return true;
}

void header_to_bytes(const TarHeader& h, bool pax_format, unsigned char* b) {
    std::memset(b, 0, BLOCK_SIZE);
    std::string full = h.path.empty() ? std::string(h.name) : h.path;

    if (!pax_format && full.size() <= 100) {
        std::memcpy(b, full.data(), full.size());
    } else if (!pax_format && full.size() > 100) {
        // truncate for header, will emit L header separately
        size_t const split = std::min(full.size(), static_cast<size_t>(100));
        std::memcpy(b, full.data(), split);
    } else {
        size_t const n = std::min(full.size(), static_cast<size_t>(100));
        if (n > 0) { std::memcpy(b, full.data(), n);
}
    }

    field_put(b + 100, 8, h.mode);
    field_put(b + 108, 8, h.uid);
    field_put(b + 116, 8, h.gid);
    field_put(b + 124, 12, h.size);
    field_put(b + 136, 12, h.mtime);
    b[156] = h.typeflag;

    std::string lnk = h.linkpath.empty() ? std::string(h.linkname) : h.linkpath;
    if (lnk.size() <= 100) {
        size_t const n = std::min(lnk.size(), static_cast<size_t>(100));
        if (n > 0) { std::memcpy(b + 157, lnk.data(), n);
}
    }

    std::memcpy(b + 257, "ustar ", 6);
    if (!h.uname.empty()) {
        size_t const n = std::min(h.uname.size(), static_cast<size_t>(31));
        std::memcpy(b + 265, h.uname.data(), n);
    }
    if (!h.gname.empty()) {
        size_t const n = std::min(h.gname.size(), static_cast<size_t>(31));
        std::memcpy(b + 297, h.gname.data(), n);
    }

    unsigned int sum = 0;
    for (size_t i = 0; i < BLOCK_SIZE; i++) { sum += (i >= 148 && i < 156) ? 0x20 : b[i];
}
    (void)snprintf(reinterpret_cast<char*>(b) + 148, 7, "%06o", sum & 07777777);
}

// ---------------------------------------------------------------------------
// PAX support
// ---------------------------------------------------------------------------
bool parse_pax_records(const unsigned char* data, size_t len, TarHeader& out) {
    size_t pos = 0;
    while (pos < len) {
        size_t num_end = pos;
        while (num_end < len && (std::isdigit(data[num_end]) != 0)) { num_end++;
}
        if (num_end >= len || num_end == pos || data[num_end] != ' ') { break;
}
        std::string const numstr(reinterpret_cast<const char*>(data) + pos, num_end - pos);
        long long const rec_len = std::strtoll(numstr.c_str(), nullptr, 10);
        if (rec_len < 4 || pos + static_cast<size_t>(rec_len) > len) { break;
}
        size_t const kv_off = num_end + 1;
        size_t const kv_len = static_cast<size_t>(rec_len) - (num_end - pos) - 2;
        size_t eq_pos = kv_len;
        for (size_t i = 0; i < kv_len; i++) {
            if (data[kv_off + i] == '=') { eq_pos = i; break; }
        }
        if (eq_pos >= kv_len) { pos += static_cast<size_t>(rec_len); continue; }
        std::string const key(reinterpret_cast<const char*>(data) + kv_off, eq_pos);
        std::string const val(reinterpret_cast<const char*>(data) + kv_off + eq_pos + 1, kv_len - eq_pos - 1);
        if (key == "path") { { out.path = val;
        } } else if (key == "linkpath") { { out.linkpath = val;
        } } else if (key == "mtime") {
            char* endp = nullptr;
            double const d = std::strtod(val.c_str(), &endp);
            out.pax_mtime = (long long)std::llround(d);
        } else if (key == "uid") { { out.pax_uid = std::strtoll(val.c_str(), nullptr, 10);
        } } else if (key == "gid") { { out.pax_gid = std::strtoll(val.c_str(), nullptr, 10);
        } } else if (key == "uname") { { out.uname = val;
        } } else if (key == "gname") { { out.gname = val;
}
}
        pos += static_cast<size_t>(rec_len);
    }
    return true;
}

std::vector<unsigned char> build_pax_block(const TarHeader& h) {
    std::string body;
    auto append_record = [&](const std::string& kv) {
        std::string const record = kv + "\n";
        std::string prefix = "1";
        while (true) {
            std::string const candidate = std::to_string(record.size() + prefix.size() + 1);
            if (candidate == prefix) { break;
}
            prefix = candidate;
        }
        body += prefix + " " + record;
    };
    std::string const full = h.path.empty() ? std::string(h.name) : h.path;
    if (full.size() > 100) {
        std::string const kv = std::string("path=") + full;
        append_record(kv);
    }
    std::string const lnk = h.linkpath.empty() ? std::string(h.linkname) : h.linkpath;
    if ((h.typeflag == '2' || h.typeflag == '1') && lnk.size() > 100) {
        std::string const kv = std::string("linkpath=") + lnk;
        append_record(kv);
    }
    if (h.pax_mtime != -1) {
        char tb[64];
        (void)snprintf(tb, sizeof(tb), "%lld.000000000", h.pax_mtime);
        std::string const kv = std::string("mtime=") + tb;
        append_record(kv);
    }
    if (h.uid >= 0x7fffff) {
        std::string const kv = std::string("uid=") + std::to_string(h.uid);
        append_record(kv);
    }
    if (h.gid >= 0x7fffff) {
        std::string const kv = std::string("gid=") + std::to_string(h.gid);
        append_record(kv);
    }
    if (h.uname.size() > 31) {
        std::string const kv = std::string("uname=") + h.uname;
        append_record(kv);
    }
    if (h.gname.size() > 31) {
        std::string const kv = std::string("gname=") + h.gname;
        append_record(kv);
    }
    if (body.empty()) { return {};
}
    std::vector<unsigned char> out(body.begin(), body.end());
    while (out.size() % BLOCK_SIZE != 0) { out.push_back(0);
}
    return out;
}

// ---------------------------------------------------------------------------
// Streaming reader with optional decompression
// ---------------------------------------------------------------------------
class TarReader {
public:
    bool open(const std::string& path, bool sniff, const std::string& label) {
        if (path == "-") { fp_ = stdin; own_ = false; }
        else { fp_ = std::fopen(path.c_str(), "rb"); if (fp_ == nullptr) { tar_perr(path.c_str()); return false; } own_ = true; }
        mode_ = Mode::Raw;
        if (sniff) {
            unsigned char m[6] = {0};
            size_t const got = std::fread(m, 1, 6, fp_);
            bool const seekable = (std::fseek(fp_, 0, SEEK_CUR) == 0);
            if (seekable) {
                std::rewind(fp_);
            } else {
                // Pipes are not seekable: rewind is a no-op and would leave the
                // stream 6 bytes ahead, misaligning every later block read. Keep
                // the sniffed bytes and replay them from read_raw().
                std::memcpy(pending_, m, std::min(got, sizeof(pending_)));
                pending_len_ = std::min(got, sizeof(pending_));
            }
            if (got >= 2 && m[0] == 0x1f && m[1] == 0x8b) { { mode_ = Mode::Gzip;
            } } else if (got >= 4 && m[0] == 0xfd && m[1] == 0x37 && m[2] == 0x7a && m[3] == 0x58) { { mode_ = Mode::Xz;
            } } else if (got >= 4 && m[0] == 0x28 && m[1] == 0xb5 && m[2] == 0x2f && m[3] == 0xfd) { mode_ = Mode::Zstd; }
        }
        file_ = label;
        return init();
    }
    ~TarReader() {
        if (mode_ == Mode::Gzip) { inflateEnd(&gz_);
        } else if (mode_ == Mode::Xz) { lzma_end(&xz_);
        } else if (mode_ == Mode::Zstd) { ZSTD_freeDStream(zs_);
}
        if (own_) { (void)std::fclose(fp_);
}
    }
    bool read_block(unsigned char* blk) {
        if (mode_ == Mode::Raw) {
            size_t const got = read_raw(blk, BLOCK_SIZE);
            return got == BLOCK_SIZE;
        }
        size_t total = 0;
        while (total < BLOCK_SIZE) {
            if (buf_pos_ >= buf_len_) { if (!fill()) { return false; 
}}
            size_t const take = std::min(BLOCK_SIZE - total, buf_len_ - buf_pos_);
            std::memcpy(blk + total, buf_ + buf_pos_, take);
            buf_pos_ += take; total += take;
        }
        return true;
    }
    size_t read_some(unsigned char* dst, size_t n) {
        if (mode_ == Mode::Raw) { return read_raw(dst, n);
}
        size_t total = 0;
        while (total < n) {
            if (buf_pos_ >= buf_len_) { if (!fill()) { break; 
}}
            size_t const take = std::min(n - total, buf_len_ - buf_pos_);
            std::memcpy(dst + total, buf_ + buf_pos_, take);
            buf_pos_ += take; total += take;
        }
        return total;
    }
    [[nodiscard]] const std::string& file() const { return file_; }
private:
    enum class Mode { Raw, Gzip, Xz, Zstd };
    FILE* fp_ = nullptr;
    bool own_ = false;
    Mode mode_ = Mode::Raw;
    std::string file_ = "-";
    unsigned char buf_[65536];
    size_t buf_pos_ = 0, buf_len_ = 0;
    // Persistent input buffer: only refilled from fp_ when the decoder has
    // consumed all previously supplied bytes. Without this, streaming
    // decoders (xz in particular) reset their in-pointer every call and
    // silently drop unconsumed input, producing FORMAT_ERROR.
    unsigned char inbuf_[65536] = {0};
    size_t inbuf_len_ = 0;
    size_t inbuf_pos_ = 0;
    // Read-ahead bytes captured during magic sniffing when the stream is not
    // seekable (pipes): rewind would be a no-op, so they are replayed first.
    unsigned char pending_[6] = {0};
    size_t pending_len_ = 0;
    bool eof_ = false;
    z_stream gz_{};
    lzma_stream xz_{};
    ZSTD_DStream* zs_ = nullptr;
    // Read up to n raw bytes, draining pending_ before touching fp_.
    size_t read_raw(unsigned char* dst, size_t n) {
        size_t total = 0;
        while (total < n) {
            if (pending_len_ > 0) {
                size_t const take = std::min(n - total, pending_len_);
                std::memcpy(dst + total, pending_, take);
                total += take;
                std::memmove(pending_, pending_ + take, pending_len_ - take);
                pending_len_ -= take;
            } else {
                size_t const got = std::fread(dst + total, 1, n - total, fp_);
                total += got;
                if (got == 0) { break;
}
            }
        }
        return total;
    }

    // Read from fp_ into inbuf_, draining pending_ first. Sets eof_ when the
    // underlying stream is exhausted. Returns bytes available from inbuf_pos_.
    size_t top_up() {
        if (eof_) { return inbuf_len_ - inbuf_pos_;
}
        // Append new data after existing unconsumed bytes.
        size_t free = sizeof(inbuf_) - inbuf_len_;
        if (free == 0) {
            // Buffer full, compact to make room.
            size_t const avail = inbuf_len_ - inbuf_pos_;
            if (avail > 0) {
                std::memmove(inbuf_, inbuf_ + inbuf_pos_, avail);
            }
            inbuf_pos_ = 0;
            inbuf_len_ = avail;
            free = sizeof(inbuf_) - inbuf_len_;
        }
        size_t total = inbuf_len_;
        while (total < sizeof(inbuf_)) {
            if (pending_len_ > 0) {
                size_t const take = std::min(free, pending_len_);
                std::memcpy(inbuf_ + total, pending_, take);
                total += take;
                free -= take;
                std::memmove(pending_, pending_ + take, pending_len_ - take);
                pending_len_ -= take;
            } else {
                size_t const got = std::fread(inbuf_ + total, 1, free, fp_);
                total += got;
                if (got == 0) { eof_ = true; break; }
            }
        }
        inbuf_len_ = total;
        return inbuf_len_ - inbuf_pos_;
    }

    bool init() {
        buf_pos_ = buf_len_ = 0;
        inbuf_len_ = 0;
        inbuf_pos_ = 0;
        eof_ = false;
        if (mode_ == Mode::Gzip) { return inflateInit2(&gz_, 15 + 32) == Z_OK;
}
        if (mode_ == Mode::Xz) { return lzma_stream_decoder(&xz_, UINT64_MAX, 0) == LZMA_OK;
}
    if (mode_ == Mode::Zstd) {
        zs_ = ZSTD_createDStream();
        if (zs_ == nullptr) { (void)fprintf(stderr, "zstd create fail\n"); return false; }
        int const rc = ZSTD_initDStream(zs_);
        if (ZSTD_isError(rc) != 0u) { (void)fprintf(stderr, "zstd init fail %d %s\n", rc, ZSTD_getErrorName(rc)); return false; }
        return true;
    }

        return true;
    }
    bool fill() {
        // Top up the persistent input buffer if the decoder has consumed it
        // and the underlying stream is not yet exhausted.
        if (mode_ == Mode::Gzip) {
            if (gz_.avail_in == 0 && !eof_) {
                inbuf_pos_ = inbuf_len_;
                top_up();
                gz_.next_in = inbuf_ + inbuf_pos_;
                gz_.avail_in = static_cast<uInt>(inbuf_len_ - inbuf_pos_);
            }
            gz_.next_out = buf_;
            gz_.avail_out = static_cast<uInt>(sizeof(buf_));
            int const rc = inflate(&gz_, Z_NO_FLUSH);
            buf_len_ = sizeof(buf_) - gz_.avail_out; buf_pos_ = 0;
            if (buf_len_ > 0) { return true;
}
            if (rc == Z_STREAM_END) { return false;
}
            return rc == Z_OK || rc == Z_BUF_ERROR;
        }
        if (mode_ == Mode::Xz) {
            if (xz_.avail_in == 0 && !eof_) {
                inbuf_pos_ = inbuf_len_;
                top_up();
                xz_.next_in = inbuf_ + inbuf_pos_;
                xz_.avail_in = inbuf_len_ - inbuf_pos_;
            }
            xz_.next_out = buf_;
            xz_.avail_out = sizeof(buf_);
            lzma_ret const r = lzma_code(&xz_, LZMA_RUN);
            buf_len_ = sizeof(buf_) - xz_.avail_out; buf_pos_ = 0;
            if (buf_len_ > 0) { return true;
}
            if (r == LZMA_STREAM_END) { return false;
}
            return r == LZMA_OK;
        }
        if (mode_ == Mode::Zstd) {
            top_up();
            ZSTD_inBuffer in{.src=inbuf_ + inbuf_pos_, .size=inbuf_len_ - inbuf_pos_, .pos=0};
            ZSTD_outBuffer out{.dst=buf_, .size=sizeof(buf_), .pos=0};
            size_t const rem = ZSTD_decompressStream(zs_, &out, &in);
            inbuf_pos_ += in.pos;
            buf_len_ = out.pos; buf_pos_ = 0;
            if (ZSTD_isError(rem) != 0u) {
                (void)fprintf(stderr, "zstd error: %s\n", ZSTD_getErrorName(rem));
                return false;
            }
            if (buf_len_ > 0) { return true;
}
            if (rem == 0) { return false;
}
            return false;
        }
        return false;
    }
};

// 0 = raw, 1 = gzip, 2 = xz, 3 = zstd
enum class Comp : int { Raw = 0, Gzip = 1, Xz = 2, Zstd = 3 };

// ---------------------------------------------------------------------------
// Streaming writer with optional compression
// ---------------------------------------------------------------------------
class TarWriter {
public:
    bool open(const std::string& path, Comp comp, const std::string& label) {
        if (path == "-") { fp_ = stdout; own_ = false; }
        else { fp_ = std::fopen(path.c_str(), "wb"); if (fp_ == nullptr) { tar_perr(path.c_str()); return false; } own_ = true; }
        comp_ = comp; file_ = label;
        if (comp == Comp::Gzip) { return deflateInit2(&gz_, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK;
}
        if (comp == Comp::Xz) { return lzma_easy_encoder(&xz_, LZMA_PRESET_DEFAULT, LZMA_CHECK_NONE) == LZMA_OK;
}
        if (comp == Comp::Zstd) { zs_ = ZSTD_createCStream(); return (zs_ != nullptr) && ZSTD_initCStream(zs_, 0) == 0; }
        return true;
    }
    ~TarWriter() {
        if (comp_ == Comp::Gzip) { deflateEnd(&gz_);
        } else if (comp_ == Comp::Xz) { lzma_end(&xz_);
        } else if (comp_ == Comp::Zstd) { ZSTD_freeCStream(zs_);
}
        if (own_) { (void)std::fclose(fp_);
}
    }
    bool write_all(const unsigned char* data, size_t len) {
        size_t off = 0;
        while (off < len) {
            if (comp_ == Comp::Raw) {
                size_t const w = std::fwrite(data + off, 1, len - off, fp_);
                if (w == 0) { return false;
}
                off += w;
            } else {
                size_t const consumed = push_filter(data + off, len - off);
                if (consumed == 0) { return false;
}
                off += consumed;
            }
        }
        return true;
    }
    bool finish() {
        bool ok = true;
        if (comp_ == Comp::Gzip) {
            unsigned char zbuf[65536];
            gz_.next_in = nullptr; gz_.avail_in = 0;
            int rc;
            do {
                gz_.next_out = zbuf; gz_.avail_out = static_cast<uInt>(sizeof(zbuf));
                rc = deflate(&gz_, Z_FINISH);
                size_t const w = sizeof(zbuf) - gz_.avail_out;
                if (w > 0) { ok = std::fwrite(zbuf, 1, w, fp_) == w;
}
            } while (rc != Z_STREAM_END && ok);
            ok = ok && rc == Z_STREAM_END;
        } else if (comp_ == Comp::Xz) {
            unsigned char zbuf[65536];
            xz_.next_in = nullptr; xz_.avail_in = 0;
            lzma_ret r;
            do {
                xz_.next_out = zbuf; xz_.avail_out = sizeof(zbuf);
                r = lzma_code(&xz_, LZMA_FINISH);
                size_t const w = sizeof(zbuf) - xz_.avail_out;
                if (w > 0) { ok = std::fwrite(zbuf, 1, w, fp_) == w;
}
            } while (r == LZMA_OK && ok);
            ok = ok && r == LZMA_STREAM_END;
        } else if (comp_ == Comp::Zstd) {
            unsigned char zbuf[65536];
            ZSTD_inBuffer inb{.src=nullptr, .size=0, .pos=0};
            ZSTD_outBuffer outb{.dst=zbuf, .size=sizeof(zbuf), .pos=0};
            while (ok) {
                outb.pos = 0;
                size_t const rem = ZSTD_compressStream2(zs_, &outb, &inb, ZSTD_e_end);
                if (ZSTD_isError(rem) != 0u) { break;
}
                if (outb.pos > 0) { ok = std::fwrite(zbuf, 1, outb.pos, fp_) == outb.pos;
}
                if (rem == 0) { break;
}
            }
        }
        if (!own_) { (void)std::fflush(fp_);
}
        return ok;
    }
private:
    Comp comp_ = Comp::Raw;
    FILE* fp_ = nullptr;
    bool own_ = false;
    std::string file_;
    z_stream gz_{};
    lzma_stream xz_{};
    ZSTD_CStream* zs_ = nullptr;
    unsigned char fbuf_[65536];

    size_t push_filter(const unsigned char* in, size_t in_len) {
        if (comp_ == Comp::Gzip) {
            size_t off = 0;
            while (off < in_len) {
                size_t const chunk = std::min(in_len - off, static_cast<size_t>(65536));
                gz_.next_in = const_cast<Bytef*>(in + off);
                gz_.avail_in = static_cast<uInt>(chunk);
                for (;;) {
                    gz_.next_out = fbuf_;
                    gz_.avail_out = static_cast<uInt>(sizeof(fbuf_));
                    int const rc = deflate(&gz_, Z_NO_FLUSH);
                    size_t const w = sizeof(fbuf_) - gz_.avail_out;
                    if (w > 0 && std::fwrite(fbuf_, 1, w, fp_) != w) { return 0;
}
                    if (gz_.avail_in == 0) { break;
}
                    if (rc != Z_OK && rc != Z_BUF_ERROR) { return 0;
}
                }
                size_t const consumed = chunk - gz_.avail_in;
                if (consumed == 0) { return 0;
}
                off += consumed;
            }
            return in_len;
        }
        if (comp_ == Comp::Xz) {
            size_t off = 0;
            while (off < in_len) {
                size_t const chunk = std::min(in_len - off, static_cast<size_t>(65536));
                xz_.next_in = const_cast<uint8_t*>(in + off);
                xz_.avail_in = chunk;
                for (;;) {
                    xz_.next_out = fbuf_;
                    xz_.avail_out = sizeof(fbuf_);
                    lzma_ret const r = lzma_code(&xz_, LZMA_RUN);
                    size_t const w = sizeof(fbuf_) - xz_.avail_out;
                    if (w > 0 && std::fwrite(fbuf_, 1, w, fp_) != w) { return 0;
}
                    size_t const consumed = chunk - xz_.avail_in;
                    if (consumed > 0) { break;
}
                    if (r != LZMA_OK) { return 0;
}
                }
                size_t const consumed = chunk - xz_.avail_in;
                if (consumed == 0) { return 0;
}
                off += consumed;
            }
            return in_len;
        }
        if (comp_ == Comp::Zstd) {
            size_t off = 0;
            while (off < in_len) {
                ZSTD_inBuffer inb{.src=reinterpret_cast<void*>(const_cast<unsigned char*>(in) + off), .size=std::min(in_len - off, static_cast<size_t>(65536)), .pos=0};
                ZSTD_outBuffer outb{.dst=fbuf_, .size=sizeof(fbuf_), .pos=0};
                while (inb.pos < inb.size) {
                    outb.pos = 0;
                    size_t const rem = ZSTD_compressStream2(zs_, &outb, &inb, ZSTD_e_continue);
                    if (ZSTD_isError(rem) != 0u) { return 0;
}
                    if (outb.pos > 0 && std::fwrite(fbuf_, 1, outb.pos, fp_) != outb.pos) { return 0;
}
                    if (inb.pos == inb.size) { break;
}
                    if (rem == 0) { return 0;
}
                }
                if (inb.pos == 0) { return 0;
}
                off += inb.pos;
            }
            return in_len;
        }
        return in_len;
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool skip_body(TarReader& r, long long size) {
    unsigned char tmp[65536];
    long long left = size;
    while (left > 0) {
        size_t const want = static_cast<size_t>(std::min<long long>(left, sizeof(tmp)));
        size_t const got = r.read_some(tmp, want);
        if (got == 0) { return false;
}
        left -= static_cast<long long>(got);
    }
    // skip the block padding so the next header is aligned at a 512 boundary
    size_t const pad = (BLOCK_SIZE - static_cast<size_t>(size) % BLOCK_SIZE) % BLOCK_SIZE;
    long long leftp = static_cast<long long>(pad);
    while (leftp > 0) {
        size_t const want = static_cast<size_t>(std::min<long long>(leftp, sizeof(tmp)));
        size_t const got = r.read_some(tmp, want);
        if (got == 0) { return false;
}
        leftp -= static_cast<long long>(got);
    }
    return true;
}

// Read exactly n bytes of payload plus the trailing 512-byte padding, keeping
// the stream aligned on the next header boundary. Returns false on EOF.
bool read_payload_aligned(TarReader& r, unsigned char* dst, size_t n) {
    size_t off = 0;
    while (off < n) {
        size_t const got = r.read_some(dst + off, n - off);
        if (got == 0) { return false;
}
        off += got;
    }
    size_t const pad = (BLOCK_SIZE - n % BLOCK_SIZE) % BLOCK_SIZE;
    if (pad > 0) {
        unsigned char tmp[4096];
        size_t leftp = pad;
        while (leftp > 0) {
            size_t const got = r.read_some(tmp, std::min(sizeof(tmp), leftp));
            if (got == 0) { return false;
}
            leftp -= got;
        }
    }
    return true;
}

bool glob_match(const char* pat, const char* str) {
    const char * p = pat;
    const char *s = str;
    while ((*p) != 0) {
        if (*p == '*') {
            while (*p == '*') { p++;
}
            if (*p == '\0') { return true;
}
            const char* sp = s;
            for (;;) {
                if (glob_match(p, sp)) { return true;
}
                if (*sp == '\0') { return false;
}
                sp++;
            }
        } else if (*p == '?') { if (*s == '\0') { return false; 
}p++; s++; }
        else if (*p == '[') {
            const char* close = std::strchr(p + 1, ']');
            if (close == nullptr) { p++; s++; continue; }
            bool const neg = (p[1] == '!' || p[1] == '^');
            const char* c = p + 1 + (neg ? 1 : 0);
            bool matched = false;
            while (c < close && ((*s) != 0)) {
                if (c + 2 <= close && c[1] == '-') {
                    if (static_cast<unsigned char>(*s) >= static_cast<unsigned char>(c[0]) && static_cast<unsigned char>(*s) <= static_cast<unsigned char>(c[2])) { matched = true;
}
                    c += 3;
                } else { if (*c == *s) { matched = true; 
}c++; }
            }
            if (neg) { matched = !matched;
}
            if (!matched) { return false;
}
            p = close + 1; s++;
        } else { if (*p != *s) { return false; 
}p++; s++; }
    }
    return *s == '\0';
}

bool member_matches(const TarHeader& h, const std::string& pattern) {
    std::string name = h.member_name();
    if (pattern.find('/') == std::string::npos) {
        size_t const slash = name.find_last_of('/');
        name = (slash == std::string::npos) ? name : name.substr(slash + 1);
    }
    return glob_match(pattern.c_str(), name.c_str());
}

// ---------------------------------------------------------------------------
// CREATE
// ---------------------------------------------------------------------------
struct WalkItem { std::string path; std::string arcname; };

void walk_dir(const std::string& dir, const std::string& arc, std::vector<WalkItem>& out) {
    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (const auto& e : fs::directory_iterator(dir, ec)) { entries.push_back(e);
}
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.path().filename().string() < b.path().filename().string();
    });
    for (auto& e : entries) {
        std::string const subarc = arc + "/" + e.path().filename().string();
        out.push_back({.path=e.path().string(), .arcname=subarc});
        std::error_code sec;
        if (fs::is_directory(e.symlink_status(sec))) {
            walk_dir(e.path().string(), subarc, out);
        }
    }
}

int write_member(TarWriter& w, const std::string& path, const std::string& arcname, const TarOptions& opt,
                 std::map<std::pair<dev_t, ino_t>, std::string>* inode_map) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) { return tar_perr(path.c_str());
}

    TarHeader h;
    h.path = arcname;
    h.mode = static_cast<int>(st.st_mode & 07777);
    h.uid = static_cast<int>(st.st_uid); h.gid = static_cast<int>(st.st_gid);
    h.mtime = static_cast<int>(st.st_mtime);
    h.pax_mtime = opt.format_pax ? static_cast<long long>(st.st_mtime) : -1;
    { const ::passwd* pw = getpwuid(st.st_uid); if (pw != nullptr) { h.uname = pw->pw_name; 
}}
    { const struct group* gr = ::getgrgid(st.st_gid); if (gr != nullptr) { h.gname = gr->gr_name; 
}}

    if (S_ISREG(st.st_mode)) {
        h.typeflag = '0';
        h.size = static_cast<long long>(st.st_size);
    } else if (S_ISDIR(st.st_mode)) {
        h.typeflag = '5'; h.size = 0;
    } else if (S_ISLNK(st.st_mode)) {
        h.typeflag = '2';
        char buf[4096];
        ssize_t const n = readlink(path.c_str(), buf, sizeof(buf) - 1);
        if (n < 0) { return tar_perr(path.c_str());
}
        buf[n] = 0;
        std::string const lnk(buf);
        std::memset(h.linkname, 0, sizeof(h.linkname));
        if (lnk.size() < sizeof(h.linkname)) { std::strncpy(h.linkname, buf, sizeof(h.linkname) - 1);
        } else { h.linkpath = lnk;
}
        h.size = 0;
    } else if (S_ISFIFO(st.st_mode)) {
        h.typeflag = '6'; h.size = 0;
    } else if (S_ISBLK(st.st_mode)) {
        h.typeflag = '8'; h.size = 0;
    } else if (S_ISCHR(st.st_mode)) {
        h.typeflag = '7'; h.size = 0;
    } else {
        return tar_err("%s: unsupported file type (skipped)", path.c_str());
    }

    // Hardlink detection: if this inode was already recorded, emit typeflag '1'
    // referencing the first occurrence's arcname. Skip body in that case.
    if (S_ISREG(st.st_mode) && (inode_map != nullptr) && st.st_nlink > 1) {
        auto key = std::make_pair(st.st_dev, st.st_ino);
        auto it = inode_map->find(key);
        if (it != inode_map->end() && it->second != arcname) {
            h.typeflag = '1';
            h.size = 0;
            std::memset(h.linkname, 0, sizeof(h.linkname));
            if (it->second.size() < sizeof(h.linkname)) {
                std::strncpy(h.linkname, it->second.c_str(), sizeof(h.linkname) - 1);
            } else { h.linkpath = it->second;
}
        } else if (it == inode_map->end()) {
            inode_map->emplace(key, arcname);
        }
    }
    // L/K headers for GNU long names (non-pax)
    if (!opt.format_pax && h.path.size() > 100) {
        std::string payload = h.path + "\0";
        unsigned char lhb[BLOCK_SIZE];
        TarHeader lh; lh.typeflag = 'L'; lh.size = static_cast<long long>(payload.size()); lh.mtime = h.mtime;
        header_to_bytes(lh, false, lhb);
        if (!w.write_all(lhb, BLOCK_SIZE)) { return 1;
}
        if (!w.write_all(reinterpret_cast<const unsigned char*>(payload.data()), payload.size())) { return 1;
}
        size_t const pad = (BLOCK_SIZE - payload.size() % BLOCK_SIZE) % BLOCK_SIZE;
        if (pad != 0u) { unsigned char z[BLOCK_SIZE] = {0}; if (!w.write_all(z, pad)) { return 1; 
}}
    }
    if (!opt.format_pax && (h.typeflag == '2' || h.typeflag == '1') && h.linkpath.size() > 100) {
        std::string payload = h.linkpath + "\0";
        unsigned char lhb[BLOCK_SIZE];
        TarHeader lh; lh.typeflag = 'K'; lh.size = static_cast<long long>(payload.size()); lh.mtime = h.mtime;
        header_to_bytes(lh, false, lhb);
        if (!w.write_all(lhb, BLOCK_SIZE)) { return 1;
}
        if (!w.write_all(reinterpret_cast<const unsigned char*>(payload.data()), payload.size())) { return 1;
}
        size_t const pad = (BLOCK_SIZE - payload.size() % BLOCK_SIZE) % BLOCK_SIZE;
        if (pad != 0u) { unsigned char z[BLOCK_SIZE] = {0}; if (!w.write_all(z, pad)) { return 1; 
}}
    }

    // pax header
    if (opt.format_pax) {
        auto pax = build_pax_block(h);
        if (!pax.empty()) {
            unsigned char phb[BLOCK_SIZE];
            TarHeader ph; ph.typeflag = 'x'; ph.size = static_cast<long long>(pax.size()); ph.mtime = h.mtime;
            header_to_bytes(ph, true, phb);
            if (!w.write_all(phb, BLOCK_SIZE)) { return 1;
}
            if (!w.write_all(pax.data(), pax.size())) { return 1;
}
        }
    }

    // main header
    unsigned char hb[BLOCK_SIZE];
    header_to_bytes(h, opt.format_pax, hb);
    if (!w.write_all(hb, BLOCK_SIZE)) { return 1;
}

    // body
    if (S_ISREG(st.st_mode)) {
        if (h.size <= (1LL << 20)) {
            std::string data(static_cast<size_t>(h.size), '\0');
            FILE* f = std::fopen(path.c_str(), "rb");
            if (f == nullptr) { return tar_perr(path.c_str());
}
            size_t const got = (h.size != 0) ? std::fread(data.data(), 1, data.size(), f) : 0;
            (void)std::fclose(f);
            if (got != static_cast<size_t>(h.size)) { return tar_err("%s: read error", path.c_str());
}
            if (!w.write_all(reinterpret_cast<const unsigned char*>(data.data()), data.size())) { return 1;
}
            size_t const pad = (BLOCK_SIZE - data.size() % BLOCK_SIZE) % BLOCK_SIZE;
            if (pad != 0u) { unsigned char z[BLOCK_SIZE] = {0}; if (!w.write_all(z, pad)) { return 1; 
}}
        } else {
            FILE* f = std::fopen(path.c_str(), "rb");
            if (f == nullptr) { return tar_perr(path.c_str());
}
            unsigned char buf[65536];
            size_t total = 0;
            for (;;) {
                size_t const got = std::fread(buf, 1, sizeof(buf), f);
                if (got == 0) { break;
}
                if (!w.write_all(buf, got)) { (void)std::fclose(f); return 1; }
                total += got;
            }
            (void)std::fclose(f);
            size_t const pad = (BLOCK_SIZE - total % BLOCK_SIZE) % BLOCK_SIZE;
            if (pad != 0u) { unsigned char z[BLOCK_SIZE] = {0}; if (!w.write_all(z, pad)) { return 1; 
}}
        }
    } else {
        size_t const pad = (BLOCK_SIZE - static_cast<size_t>(h.size) % BLOCK_SIZE) % BLOCK_SIZE;
        if (pad != 0u) { unsigned char z[BLOCK_SIZE] = {0}; if (!w.write_all(z, pad)) { return 1; 
}}
    }
    return 0;
}

int do_create(const TarOptions* opt) {
    std::vector<WalkItem> all;
    std::string const cur = opt->chdirs.empty() ? "." : opt->chdirs.back();
    for (const auto& s : opt->sources) {
        // strip leading '/' like GNU tar
        std::string arcname = s;
        while (!arcname.empty() && arcname.front() == '/') { arcname.erase(0, 1);
}
        std::string const full = (s.empty() || s[0] == '/') ? s : cur + "/" + s;
        struct stat st;
        if (lstat(full.c_str(), &st) != 0) { return tar_perr(full.c_str());
}
        all.push_back({.path=full, .arcname=arcname});
        if (S_ISDIR(st.st_mode)) { walk_dir(full, arcname, all);
}
    }

    // Build inode->arcname map so that repeated inodes become hardlinks.
    std::map<std::pair<dev_t, ino_t>, std::string> inode_map;
    for (const auto& it : all) {
        struct stat st;
        if (lstat(it.path.c_str(), &st) != 0) { continue;
}
        auto key = std::make_pair(st.st_dev, st.st_ino);
        inode_map.emplace(key, it.arcname);
    }

    auto comp_of = [](bool gz, bool xz, bool zst) -> int {
        if (gz) { return 1; 
}if (xz) { return 2; 
}if (zst) { return 3; 
}return 0;
    };
    int const cm = comp_of(opt->compress_gz, opt->compress_xz, opt->compress_zst);

    TarWriter w;
    if (!w.open(opt->file, static_cast<Comp>(cm), opt->file)) { return 1;
}

    int rc = 0;
    for (const auto& it : all) {
        if (write_member(w, it.path, it.arcname, *opt, &inode_map) != 0) { rc = 1; break; }
    }
    unsigned char zero[BLOCK_SIZE] = {0};
    if (rc == 0) {
        if (!w.write_all(zero, BLOCK_SIZE)) { rc = 1;
}
        if (rc == 0 && !w.write_all(zero, BLOCK_SIZE)) { rc = 1;
}
    }
    if (rc == 0 && !w.finish()) { rc = tar_perr(opt->file.c_str());
}
    return rc;
}

// ---------------------------------------------------------------------------
// EXTRACT
// ---------------------------------------------------------------------------
int extract_member(TarReader& r, const TarHeader& h, const std::string& target,
                   const TarOptions& opt, bool preserve_owner) {
    std::error_code ec;
    size_t const lastslash = target.find_last_of('/');
    if (lastslash != std::string::npos && lastslash > 0) {
        fs::create_directories(target.substr(0, lastslash), ec);
}

    long long const t = (h.pax_mtime != -1) ? h.pax_mtime : static_cast<long long>(h.mtime);

    switch (h.typeflag) {
    case '0': case '\0': {
        FILE* f = std::fopen(target.c_str(), "wb");
        if (f == nullptr) { return tar_perr(target.c_str());
}
        int const fd = fileno(f);
        if (h.size > 0) {
            unsigned char buf[65536];
            long long left = h.size;
            while (left > 0) {
                size_t const want = static_cast<size_t>(std::min<long long>(left, sizeof(buf)));
                size_t const got = r.read_some(buf, want);
                if (got == 0) { (void)std::fclose(f); return tar_err("%s: unexpected EOF in archive", target.c_str()); }
                if (std::fwrite(buf, 1, got, f) != got) { (void)std::fclose(f); return tar_perr(target.c_str()); }
                left -= static_cast<long long>(got);
            }
        }
        size_t const pad = (BLOCK_SIZE - static_cast<size_t>(h.size) % BLOCK_SIZE) % BLOCK_SIZE;
        if (pad != 0u) {
            unsigned char tmp[65536];
            size_t leftp = pad;
            while (leftp > 0) {
                size_t const got = r.read_some(tmp, std::min(sizeof(tmp), leftp));
                if (got == 0) { break;
}
                leftp -= got;
            }
        }
        if (fchmod(fd, static_cast<mode_t>(h.mode)) != 0) {}
        if (preserve_owner) {
            if (geteuid() == 0) { fchown(fd, h.uid, h.gid);
}
        }
        (void)std::fflush(f);
        { struct timespec ts[2]; ts[0].tv_sec = static_cast<time_t>(t); ts[0].tv_nsec = 0; ts[1] = ts[0]; futimens(fd, ts); }
        (void)std::fclose(f);
        return 0;
    }
    case '5': {
        fs::create_directories(target, ec);
        // Only fchmod/futimens on directory; don't open with O_DIRECTORY
        // since target is the dir itself
        struct stat st2;
        if (stat(target.c_str(), &st2) == 0 && S_ISDIR(st2.st_mode)) {
            chmod(target.c_str(), static_cast<mode_t>(h.mode));
            if (preserve_owner) {
                if (geteuid() == 0) { chown(target.c_str(), h.uid, h.gid);
}
            }
            struct timespec ts[2];
            ts[0].tv_sec = static_cast<time_t>(t); ts[0].tv_nsec = 0; ts[1] = ts[0];
            utimensat(AT_FDCWD, target.c_str(), ts, 0);
        }
        return 0;
    }
    case '2': {
        std::string const lnk = h.member_link();
        ::unlink(target.c_str());
        if (symlink(lnk.c_str(), target.c_str()) != 0) { return tar_perr(target.c_str());
}
        return 0;
    }
    case '1': {
        // Hard link: link target is an archive member name. Strip leading '/'.
        std::string lnk = h.member_link();
        while (!lnk.empty() && lnk.front() == '/') { lnk.erase(0, 1);
}
        std::string const base = opt.chdirs.empty() ? "" : opt.chdirs.back() + "/";
        std::string const link_target = base + lnk;
        ::unlink(target.c_str());
        if (link(link_target.c_str(), target.c_str()) != 0) {
            // Fall back: copy the file if the referenced member doesn't exist
            return tar_perr(target.c_str());
        }
        return 0;
    }
    case '6': {
        if (mkfifo(target.c_str(), static_cast<mode_t>(h.mode)) != 0) { return tar_perr(target.c_str());
}
        return 0;
    }
    case '7': case '8': return 0;
    default: return tar_err("unknown file type '%c' for member %s", h.typeflag, h.member_name().c_str());
    }
}

int do_extract(const TarOptions& opt) {
    TarReader r;
    if (!r.open(opt.file, true, opt.file)) { return 1;
}

    bool preserve_owner = (opt.preserve_owner || (opt.op == Op::Extract && geteuid() == 0 && !opt.no_same_owner));
    if (opt.no_same_owner) { preserve_owner = false;
}

    TarHeader l_pending;
    l_pending.typeflag = '\0';
    TarHeader pax_pending;
    pax_pending.pax_mtime = -1;
    bool pax_set = false;

    int rc = 0;
    for (;;) {
        unsigned char blk[BLOCK_SIZE];
        if (!r.read_block(blk)) { break;
}
        if (header_is_end(blk)) { break;
}
        if (!header_is_valid(blk, r.file())) { rc = 1; break; }

        TarHeader h;
        header_from_bytes(blk, h);
        apply_prefix(blk, h);

        if (h.typeflag == 'L' || h.typeflag == 'K') {
            std::string data(static_cast<size_t>(h.size), '\0');
            if (!read_payload_aligned(r, reinterpret_cast<unsigned char*>(data.data()), data.size())) { rc = 1; break; }
            size_t const nul = data.find('\0');
            std::string const longstr = (nul == std::string::npos) ? data : data.substr(0, nul);
            if (h.typeflag == 'L') { l_pending.typeflag = 'L';
            } else { l_pending.typeflag = 'K';
}
            l_pending.path = longstr;
            continue;
        }
        if (h.typeflag == 'x') {
            std::string data(static_cast<size_t>(h.size), '\0');
            if (!read_payload_aligned(r, reinterpret_cast<unsigned char*>(data.data()), data.size())) { rc = 1; break; }
            pax_pending.typeflag = 'x';
            if (!parse_pax_records(reinterpret_cast<const unsigned char*>(data.data()), data.size(), pax_pending)) {
                rc = tar_err("%s: bad pax extension", r.file().c_str()); break;
            }
            pax_set = true;
            continue;
        }
        if (h.typeflag == 'g') {
            std::string data(static_cast<size_t>(h.size), '\0');
            if (!read_payload_aligned(r, reinterpret_cast<unsigned char*>(data.data()), data.size())) { rc = 1; break; }
            continue;
        }

        // apply pending
        if (l_pending.typeflag != '\0') {
            if (l_pending.typeflag == 'L') { h.path = l_pending.path;
            } else if (h.typeflag == '2' || h.typeflag == '1') { h.linkpath = l_pending.path;
}
            l_pending.typeflag = '\0';
        }
        if (pax_set) {
            if (!pax_pending.path.empty()) { h.path = pax_pending.path;
}
            if (!pax_pending.linkpath.empty()) { h.linkpath = pax_pending.linkpath;
}
            if (pax_pending.pax_mtime != -1) { h.pax_mtime = pax_pending.pax_mtime;
}
            if (pax_pending.pax_uid != -1) { h.uid = static_cast<int>(pax_pending.pax_uid);
}
            if (pax_pending.pax_gid != -1) { h.gid = static_cast<int>(pax_pending.pax_gid);
}
            if (!pax_pending.uname.empty()) { h.uname = pax_pending.uname;
}
            if (!pax_pending.gname.empty()) { h.gname = pax_pending.gname;
}
            pax_set = false;
        }

        // filter
        if (!opt.patterns.empty()) {
            bool match = false;
            for (const auto& p : opt.patterns) {
                if (member_matches(h, p)) { match = true; break; }
            }
            if (!match) {
                if (!skip_body(r, h.size)) { rc = 1; break; }
                continue;
            }
        }

        std::string member = h.member_name();
        // Strip leading '/' from absolute member names (like GNU tar does)
        while (!member.empty() && member.front() == '/') { member.erase(0, 1);
}
        // Path traversal check on the member name itself
        {
            fs::path const p(member);
            for (const auto &part : p) {
                if (part == "..") {
                    rc = tar_err("tar: %s: path traversal detected", h.member_name().c_str());
                    break;
                }
            }
            if (rc != 0) { break;
}
        }
        std::string target = member;
        // Apply -C options for extraction
        std::string const base_dir = opt.chdirs.empty() ? "." : opt.chdirs.back();
        if (!opt.chdirs.empty() && opt.op == Op::Extract) {
            target = base_dir + "/" + target;
        }
        // strip leading ./
        while (target.size() > 2 && target.starts_with("./")) { target.erase(0, 2);
}

        if (extract_member(r, h, target, opt, preserve_owner) != 0) { rc = 1; break; }
    }
    return rc;
}

// ---------------------------------------------------------------------------
// LIST
// ---------------------------------------------------------------------------
int do_list(const TarOptions& opt) {
    TarReader r;
    if (!r.open(opt.file, true, opt.file)) { return 1;
}

    TarHeader l_pending;
    l_pending.typeflag = '\0';
    TarHeader pax_pending;
    pax_pending.pax_mtime = -1;
    bool pax_set = false;

    int rc = 0;
    for (;;) {
        unsigned char blk[BLOCK_SIZE];
        if (!r.read_block(blk)) { break;
}
        if (header_is_end(blk)) { break;
}
        if (!header_is_valid(blk, r.file())) { rc = 1; break; }

        TarHeader h;
        header_from_bytes(blk, h);
        apply_prefix(blk, h);

        if (h.typeflag == 'L' || h.typeflag == 'K') {
            std::string data(static_cast<size_t>(h.size), '\0');
            if (!read_payload_aligned(r, reinterpret_cast<unsigned char*>(data.data()), data.size())) { rc = 1; break; }
            size_t const nul = data.find('\0');
            l_pending.path = (nul == std::string::npos) ? data : data.substr(0, nul);
            l_pending.typeflag = (h.typeflag == 'L') ? 'L' : 'K';
            continue;
        }
        if (h.typeflag == 'x') {
            std::string data(static_cast<size_t>(h.size), '\0');
            if (!read_payload_aligned(r, reinterpret_cast<unsigned char*>(data.data()), data.size())) { rc = 1; break; }
            if (!parse_pax_records(reinterpret_cast<const unsigned char*>(data.data()), data.size(), pax_pending)) {
                rc = tar_err("%s: bad pax extension", r.file().c_str()); break;
            }
            pax_set = true;
            continue;
        }
        if (h.typeflag == 'g') {
            std::string data(static_cast<size_t>(h.size), '\0');
            if (!read_payload_aligned(r, reinterpret_cast<unsigned char*>(data.data()), data.size())) { rc = 1; break; }
            continue;
        }

        if (l_pending.typeflag != '\0') {
            if (l_pending.typeflag == 'L') { h.path = l_pending.path;
            } else if (h.typeflag == '2' || h.typeflag == '1') { h.linkpath = l_pending.path;
}
            l_pending.typeflag = '\0';
        }
        if (pax_set) {
            if (!pax_pending.path.empty()) { h.path = pax_pending.path;
}
            if (!pax_pending.linkpath.empty()) { h.linkpath = pax_pending.linkpath;
}
            if (pax_pending.pax_mtime != -1) { h.pax_mtime = pax_pending.pax_mtime;
}
            pax_set = false;
        }

        // filter
        if (!opt.patterns.empty()) {
            bool match = false;
            for (const auto& p : opt.patterns) {
                if (member_matches(h, p)) { match = true; break; }
            }
            if (!match) {
                if (!skip_body(r, h.size)) { rc = 1; break; }
                continue;
            }
        }

        printf("%s\n", h.member_name().c_str());
        if (!skip_body(r, h.size)) { rc = 1; break; }
    }
    return rc;
}

// ---------------------------------------------------------------------------
// Argument parsing (manual for GNU-style combined shorts)
// ---------------------------------------------------------------------------
void print_help() {
    printf("Usage: tar [OPTION]... { -c | -x | -t } [FILE]\n");
    printf("Create, extract, or list a tar archive.\n\n");
    printf("  -c, --create              create a new archive\n");
    printf("  -x, -e, --extract         extract files from an archive\n");
    printf("  -t, --list                list archive contents\n");
    printf("  -f, --file=ARCHIVE        use archive file ARCHIVE ('-' for stdin/stdout)\n");
    printf("  -C, --directory=DIR       change to DIR before operating\n");
    printf("  -z, --gzip                filter the archive with gzip\n");
    printf("  -j, --bzip2               filter the archive with xz (liblzma)\n");
    printf("  -J, --zstd                filter the archive with zstd (libzstd)\n");
    printf("  -p, --preserve-permissions  extract with full permission sets\n");
    printf("  -o, --no-same-owner       extract as current user\n");
    printf("      --format=FORMAT       archive format: gnu (default) or pax\n");
    printf("  -h, --help                display this help and exit\n");
    printf("      --version             output version information and exit\n");
    printf("\nFormats: ustar, GNU (L/K longname), pax (x/g extension headers).\n");
    printf("On extract/list, gzip/xz/zstd are auto-detected by magic bytes.\n");
}

int parse_args(int argc, char** argv, TarOptions& opt) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--") {
            for (int j = i + 1; j < argc; j++) {
                if (opt.op == Op::Create) { opt.sources.push_back(argv[j]);
                } else { opt.patterns.push_back(argv[j]);
}
            }
            break;
        }
        if (a.size() > 2 && a[0] == '-' && a[1] == '-') {
            std::string name = a.substr(2);
            std::string val;
            size_t const eq = name.find('=');
            bool has_val = false;
            if (eq != std::string::npos) { val = name.substr(eq + 1); name = name.substr(0, eq); has_val = true; }
            if (name == "help") { print_help(); std::exit(0); }
            if (name == "version") { print_version("tar"); std::exit(0); }
            if (name == "create") { if (opt.op != Op::None) { tar_err("only one of -c, -x, -t can be specified"); return 1; } opt.op = Op::Create; }
            else if (name == "extract" || name == "get") { if (opt.op != Op::None) { tar_err("only one of -c, -x, -t can be specified"); return 1; } opt.op = Op::Extract; }
            else if (name == "list") { if (opt.op != Op::None) { tar_err("only one of -c, -x, -t can be specified"); return 1; } opt.op = Op::List; }
            else if (name == "file") {
                if (!has_val) { if (i + 1 >= argc) { tar_err("--file requires an argument"); return 1; } val = argv[++i]; }
                opt.file = val;
            }
            else if (name == "directory") {
                if (!has_val) { if (i + 1 >= argc) { tar_err("--directory requires an argument"); return 1; } val = argv[++i]; }
                opt.chdirs.push_back(val);
            }
            else if (name == "gzip" || name == "compress") { { opt.compress_gz = true;
            } } else if (name == "bzip2") { { opt.compress_xz = true;
            } } else if (name == "xz") { { opt.compress_xz = true;
            } } else if (name == "zstd") { { opt.compress_zst = true;
            } } else if (name == "preserve-permissions" || name == "same-permissions" || name == "same-owner") { { opt.preserve_owner = true;
            } } else if (name == "no-same-owner") { { opt.no_same_owner = true;
            } } else if (name == "format") {
                if (!has_val) { if (i + 1 >= argc) { tar_err("--format requires an argument"); return 1; } val = argv[++i]; }
                if (val == "pax" || val == "posix") { { opt.format_pax = true;
                } } else if (val != "gnu") { tar_err("invalid format '%s'", val.c_str()); return 1; }
            }
            else {
                // accept unknown long options silently per spec
            }
            continue;
        }
        if (a.size() >= 2 && a[0] == '-') {
            const char* p = a.c_str() + 1;
            while ((*p) != 0) {
                char const c = *p;
                switch (c) {
                case 'c': if (opt.op != Op::None) { tar_err("only one of -c, -x, -t can be specified"); return 1; } opt.op = Op::Create; break;
                case 'x': case 'e': if (opt.op != Op::None) { tar_err("only one of -c, -x, -t can be specified"); return 1; } opt.op = Op::Extract; break;
                case 't': if (opt.op != Op::None) { tar_err("only one of -c, -x, -t can be specified"); return 1; } opt.op = Op::List; break;
                case 'f':
                    if (p[1] != 0) { opt.file = p + 1; p = a.c_str() + a.size(); continue; }
                    if (i + 1 < argc) { opt.file = argv[++i]; p = a.c_str() + a.size(); continue; }
                    tar_err("option requires an argument -- 'f'"); return 1;
                case 'C':
                    if (p[1] != 0) { opt.chdirs.push_back(std::string(p + 1)); p = a.c_str() + a.size(); continue; }
                    if (i + 1 < argc) { opt.chdirs.push_back(std::string(argv[++i])); p = a.c_str() + a.size(); continue; }
                    tar_err("option requires an argument -- 'C'"); return 1;
                case 'z': case 'Z': case 'g': opt.compress_gz = true; break;
                case 'j': opt.compress_xz = true; break;
                case 'J': opt.compress_zst = true; break;
                case 'p': opt.preserve_owner = true; break;
                case 'o': opt.no_same_owner = true; break;
                case 'h': print_help(); std::exit(0);
                default: tar_err("unrecognized option '%c'", c); return 1;
                }
                p++;
            }
            continue;
        }
        if (opt.op == Op::Create) { opt.sources.push_back(a);
        } else { opt.patterns.push_back(a);
}
    }

    if (opt.op == Op::None) { print_help(); return 1; }
    if (opt.file.empty()) {
        if (opt.op == Op::Create) { opt.file = "tar.tar";
        } else { opt.file = "tar.tar";
}
    }
    return 0;
}

}  // namespace

int tar_command(int argc, char** argv) {
    TarOptions opt;
    int const rc = parse_args(argc, argv, opt);
    if (rc != 0) { return rc;
}
    switch (opt.op) {
    case Op::Create: return do_create(&opt);
    case Op::Extract: return do_extract(opt);
    case Op::List: return do_list(opt);
    default: return 1;
    }
}

REGISTER_COMMAND("tar", tar_command, "Create or extract an archive (tar)");
