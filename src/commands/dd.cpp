#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include "commands/dd.hpp"
#include "commands/command_macros.hpp"

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_CIO
#define O_CIO 0
#endif
#ifndef O_NOLINKS
#define O_NOLINKS 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif

#define DEFAULT_BLOCKSIZE 512

enum {
    C_ASCII = 01,
    C_EBCDIC = 02,
    C_IBM = 04,
    C_BLOCK = 010,
    C_UNBLOCK = 020,
    C_LCASE = 040,
    C_UCASE = 0100,
    C_SWAB = 0200,
    C_NOERROR = 0400,
    C_NOTRUNC = 01000,
    C_SYNC = 02000,
    C_TWOBUFS = 04000,
    C_NOCREAT = 010000,
    C_EXCL = 020000,
    C_FDATASYNC = 040000,
    C_FSYNC = 0100000,
    C_SPARSE = 0200000
};

enum {
    STATUS_NONE = 1,
    STATUS_NOXFER = 2,
    STATUS_DEFAULT = 3,
    STATUS_PROGRESS = 4
};

/* Flag semantics that are not plain O_* bits. */
#define O_FULLBLOCK   (1 << 24)
#define O_COUNT_BYTES (1 << 25)
#define O_SKIP_BYTES  (1 << 26)
#define O_SEEK_BYTES  (1 << 27)

struct symbol_value {
    const char* symbol;
    int value;
};

static const symbol_value conversions[] = {
    {"ascii",   C_ASCII | C_UNBLOCK | C_TWOBUFS},
    {"ebcdic",  C_EBCDIC | C_BLOCK | C_TWOBUFS},
    {"ibm",     C_IBM | C_BLOCK | C_TWOBUFS},
    {"block",   C_BLOCK | C_TWOBUFS},
    {"unblock", C_UNBLOCK | C_TWOBUFS},
    {"lcase",   C_LCASE | C_TWOBUFS},
    {"ucase",   C_UCASE | C_TWOBUFS},
    {"sparse",  C_SPARSE},
    {"swab",    C_SWAB | C_TWOBUFS},
    {"noerror", C_NOERROR},
    {"nocreat", C_NOCREAT},
    {"excl",    C_EXCL},
    {"notrunc", C_NOTRUNC},
    {"sync",    C_SYNC},
    {"fdatasync", C_FDATASYNC},
    {"fsync",   C_FSYNC},
    {NULL, 0}
};

static const symbol_value flags[] = {
    {"append",     O_APPEND},
    {"binary",     O_BINARY},
    {"cio",        O_CIO},
    {"direct",     O_DIRECT},
    {"directory",  O_DIRECTORY},
    {"dsync",      O_DSYNC},
    {"noatime",    O_NOATIME},
    {"nocache",    0},
    {"noctty",     O_NOCTTY},
    {"nofollow",   O_NOFOLLOW},
    {"nolinks",    O_NOLINKS},
    {"nonblock",   O_NONBLOCK},
    {"sync",       O_SYNC},
    {"text",       O_TEXT},
    {"fullblock",  O_FULLBLOCK},
    {"count_bytes", O_COUNT_BYTES},
    {"skip_bytes", O_SKIP_BYTES},
    {"seek_bytes", O_SEEK_BYTES},
    {NULL, 0}
};

static const symbol_value statuses[] = {
    {"none",     STATUS_NONE},
    {"noxfer",   STATUS_NOXFER},
    {"progress", STATUS_PROGRESS},
    {NULL, 0}
};

/* Standard translation tables, taken from POSIX 1003.1-2013 / GNU dd. */
static const char ascii_to_ebcdic[] = {
  '\000', '\001', '\002', '\003', '\067', '\055', '\056', '\057',
  '\026', '\005', '\045', '\013', '\014', '\015', '\016', '\017',
  '\020', '\021', '\022', '\023', '\074', '\075', '\062', '\046',
  '\030', '\031', '\077', '\047', '\034', '\035', '\036', '\037',
  '\100', '\132', '\177', '\173', '\133', '\154', '\120', '\175',
  '\115', '\135', '\134', '\116', '\153', '\140', '\113', '\141',
  '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
  '\370', '\371', '\172', '\136', '\114', '\176', '\156', '\157',
  '\174', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
  '\310', '\311', '\321', '\322', '\323', '\324', '\325', '\326',
  '\327', '\330', '\331', '\342', '\343', '\344', '\345', '\346',
  '\347', '\350', '\351', '\255', '\340', '\275', '\232', '\155',
  '\171', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
  '\210', '\211', '\221', '\222', '\223', '\224', '\225', '\226',
  '\227', '\230', '\231', '\242', '\243', '\244', '\245', '\246',
  '\247', '\250', '\251', '\300', '\117', '\320', '\137', '\007',
  '\040', '\041', '\042', '\043', '\044', '\025', '\006', '\027',
  '\050', '\051', '\052', '\053', '\054', '\011', '\012', '\033',
  '\060', '\061', '\032', '\063', '\064', '\065', '\066', '\010',
  '\070', '\071', '\072', '\073', '\004', '\024', '\076', '\341',
  '\101', '\102', '\103', '\104', '\105', '\106', '\107', '\110',
  '\111', '\121', '\122', '\123', '\124', '\125', '\126', '\127',
  '\130', '\131', '\142', '\143', '\144', '\145', '\146', '\147',
  '\150', '\151', '\160', '\161', '\162', '\163', '\164', '\165',
  '\166', '\167', '\170', '\200', '\212', '\213', '\214', '\215',
  '\216', '\217', '\220', '\152', '\233', '\234', '\235', '\236',
  '\237', '\240', '\252', '\253', '\254', '\112', '\256', '\257',
  '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
  '\270', '\271', '\272', '\273', '\274', '\241', '\276', '\277',
  '\312', '\313', '\314', '\315', '\316', '\317', '\332', '\333',
  '\334', '\335', '\336', '\337', '\352', '\353', '\354', '\355',
  '\356', '\357', '\372', '\373', '\374', '\375', '\376', '\377'
};

static const char ascii_to_ibm[] = {
  '\000', '\001', '\002', '\003', '\067', '\055', '\056', '\057',
  '\026', '\005', '\045', '\013', '\014', '\015', '\016', '\017',
  '\020', '\021', '\022', '\023', '\074', '\075', '\062', '\046',
  '\030', '\031', '\077', '\047', '\034', '\035', '\036', '\037',
  '\100', '\132', '\177', '\173', '\133', '\154', '\120', '\175',
  '\115', '\135', '\134', '\116', '\153', '\140', '\113', '\141',
  '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
  '\370', '\371', '\172', '\136', '\114', '\176', '\156', '\157',
  '\174', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
  '\310', '\311', '\321', '\322', '\323', '\324', '\325', '\326',
  '\327', '\330', '\331', '\342', '\343', '\344', '\345', '\346',
  '\347', '\350', '\351', '\255', '\340', '\275', '\137', '\155',
  '\171', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
  '\210', '\211', '\221', '\222', '\223', '\224', '\225', '\226',
  '\227', '\230', '\231', '\242', '\243', '\244', '\245', '\246',
  '\247', '\250', '\251', '\300', '\117', '\320', '\241', '\007',
  '\040', '\041', '\042', '\043', '\044', '\025', '\006', '\027',
  '\050', '\051', '\052', '\053', '\054', '\011', '\012', '\033',
  '\060', '\061', '\032', '\063', '\064', '\065', '\066', '\010',
  '\070', '\071', '\072', '\073', '\004', '\024', '\076', '\341',
  '\101', '\102', '\103', '\104', '\105', '\106', '\107', '\110',
  '\111', '\121', '\122', '\123', '\124', '\125', '\126', '\127',
  '\130', '\131', '\142', '\143', '\144', '\145', '\146', '\147',
  '\150', '\151', '\160', '\161', '\162', '\163', '\164', '\165',
  '\166', '\167', '\170', '\200', '\212', '\213', '\214', '\215',
  '\216', '\217', '\220', '\232', '\233', '\234', '\235', '\236',
  '\237', '\240', '\252', '\253', '\254', '\255', '\256', '\257',
  '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
  '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
  '\312', '\313', '\314', '\315', '\316', '\317', '\332', '\333',
  '\334', '\335', '\336', '\337', '\352', '\353', '\354', '\355',
  '\356', '\357', '\372', '\373', '\374', '\375', '\376', '\377'
};

static const char ebcdic_to_ascii[] = {
  '\000', '\001', '\002', '\003', '\234', '\011', '\206', '\177',
  '\227', '\215', '\216', '\013', '\014', '\015', '\016', '\017',
  '\020', '\021', '\022', '\023', '\235', '\205', '\010', '\207',
  '\030', '\031', '\222', '\217', '\034', '\035', '\036', '\037',
  '\200', '\201', '\202', '\203', '\204', '\012', '\027', '\033',
  '\210', '\211', '\212', '\213', '\214', '\005', '\006', '\007',
  '\220', '\221', '\026', '\223', '\224', '\225', '\226', '\004',
  '\230', '\231', '\232', '\233', '\024', '\025', '\236', '\032',
  '\040', '\240', '\241', '\242', '\243', '\244', '\245', '\246',
  '\247', '\250', '\325', '\056', '\074', '\050', '\053', '\174',
  '\046', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
  '\260', '\261', '\041', '\044', '\052', '\051', '\073', '\176',
  '\055', '\057', '\262', '\263', '\264', '\265', '\266', '\267',
  '\270', '\271', '\313', '\054', '\045', '\137', '\076', '\077',
  '\272', '\273', '\274', '\275', '\276', '\277', '\300', '\301',
  '\302', '\140', '\072', '\043', '\100', '\047', '\075', '\042',
  '\303', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
  '\150', '\151', '\304', '\305', '\306', '\307', '\310', '\311',
  '\312', '\152', '\153', '\154', '\155', '\156', '\157', '\160',
  '\161', '\162', '\136', '\314', '\315', '\316', '\317', '\320',
  '\321', '\345', '\163', '\164', '\165', '\166', '\167', '\170',
  '\171', '\172', '\322', '\323', '\324', '\133', '\326', '\327',
  '\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',
  '\340', '\341', '\342', '\343', '\344', '\135', '\346', '\347',
  '\173', '\101', '\102', '\103', '\104', '\105', '\106', '\107',
  '\110', '\111', '\350', '\351', '\352', '\353', '\354', '\355',
  '\175', '\112', '\113', '\114', '\115', '\116', '\117', '\120',
  '\121', '\122', '\356', '\357', '\360', '\361', '\362', '\363',
  '\134', '\237', '\123', '\124', '\125', '\126', '\127', '\130',
  '\131', '\132', '\364', '\365', '\366', '\367', '\370', '\371',
  '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
  '\070', '\071', '\372', '\373', '\374', '\375', '\376', '\377'
};

static unsigned char trans_table[256];

struct DdOptions {
    const char* input_file = nullptr;
    const char* output_file = nullptr;
    int64_t input_blocksize = 0;
    int64_t output_blocksize = 0;
    int64_t conversion_blocksize = 0;
    int64_t skip_records = 0;
    int64_t skip_bytes = 0;
    int64_t seek_records = 0;
    int64_t seek_bytes = 0;
    int64_t max_records = -1;
    int64_t max_bytes = 0;
    int conv_mask = 0;
    int in_flags = 0;
    int out_flags = 0;
    int status_level = STATUS_DEFAULT;
};

/* ── Error reporting ──────────────────────────────────────────────────── */

static void dd_error(const char* msg) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "dd: %s\n", msg);
}

/* ── Number / size parsing (GNU dd operands) ───────────────────────────── */

static int64_t suffix_multiplier(const char* p, bool* is_binary) {
    *is_binary = false;
    size_t len = strlen(p);
    if (len == 0) return 1;
    switch (len) {
        case 1:
            switch (p[0]) {
                case 'c': return 1;
                case 'w': return 2;
                case 'b': return 512;
                case 'k': case 'K': return 1024LL;
                case 'M': return 1024LL * 1024;
                case 'G': return 1024LL * 1024 * 1024;
                case 'T': return 1024LL * 1024 * 1024 * 1024;
                case 'P': return 1024LL * 1024 * 1024 * 1024 * 1024;
                case 'E': return 1024LL * 1024 * 1024 * 1024 * 1024 * 1024;
                case 'Z': return INT64_MAX;
                case 'Y': return INT64_MAX;
                case 'B': return 1;
                default: return -1;
            }
        case 2:
            if (strcmp(p, "kB") == 0) return 1000LL;
            if (strcmp(p, "KB") == 0) return 1000LL;
            if (strcmp(p, "MB") == 0) return 1000LL * 1000;
            if (strcmp(p, "GB") == 0) return 1000LL * 1000 * 1000;
            if (strcmp(p, "TB") == 0) return 1000LL * 1000 * 1000 * 1000;
            if (strcmp(p, "PB") == 0) return 1000LL * 1000 * 1000 * 1000 * 1000;
            if (strcmp(p, "EB") == 0) return 1000LL * 1000 * 1000 * 1000 * 1000 * 1000;
            if (strcmp(p, "ZB") == 0) return INT64_MAX;
            if (strcmp(p, "YB") == 0) return INT64_MAX;
            if (strcmp(p, "Ki") == 0) { *is_binary = true; return 1024LL; }
            if (strcmp(p, "Mi") == 0) { *is_binary = true; return 1024LL * 1024; }
            if (strcmp(p, "Gi") == 0) { *is_binary = true; return 1024LL * 1024 * 1024; }
            if (strcmp(p, "Ti") == 0) { *is_binary = true; return 1024LL * 1024 * 1024 * 1024; }
            if (strcmp(p, "Pi") == 0) { *is_binary = true; return 1024LL * 1024 * 1024 * 1024 * 1024; }
            if (strcmp(p, "Ei") == 0) { *is_binary = true; return 1024LL * 1024 * 1024 * 1024 * 1024 * 1024; }
            return -1;
        case 3:
            if (strcmp(p, "KiB") == 0) return 1024LL;
            if (strcmp(p, "MiB") == 0) return 1024LL * 1024;
            if (strcmp(p, "GiB") == 0) return 1024LL * 1024 * 1024;
            if (strcmp(p, "TiB") == 0) return 1024LL * 1024 * 1024 * 1024;
            if (strcmp(p, "PiB") == 0) return 1024LL * 1024 * 1024 * 1024 * 1024;
            if (strcmp(p, "EiB") == 0) return 1024LL * 1024 * 1024 * 1024 * 1024 * 1024;
            if (strcmp(p, "ZiB") == 0) return INT64_MAX;
            if (strcmp(p, "YiB") == 0) return INT64_MAX;
            return -1;
        default:
            return -1;
    }
}

static int64_t parse_number(const char* s, bool* ends_with_B) {
    *ends_with_B = false;
    if (s == NULL || *s == '\0') return -1;

    char* end = NULL;
    errno = 0;
    long long a = strtoll(s, &end, 10);
    if (end == s) return -1;
    int64_t result = a;

    if (*end == 'x') {
        char* end2 = NULL;
        long long b = strtoll(end + 1, &end2, 10);
        if (end2 == end + 1) return -1;
        result *= b;
        end = end2;
    }

    if (*end != '\0') {
        if (strcmp(end, "B") == 0) {
            *ends_with_B = true;
        } else {
            bool is_binary = false;
            int64_t mult = suffix_multiplier(end, &is_binary);
            if (mult < 0) return -1;
            result *= mult;
        }
    }
    return result;
}

/* ── Symbol parsing for conv= / iflag= / oflag= / status= ─────────────── */

static bool operand_is(const char* operand, const char* name) {
    size_t nlen = strlen(name);
    if (strncmp(operand, name, nlen) != 0) return false;
    return operand[nlen] == '\0' || operand[nlen] == '=';
}

static int parse_symbols(const char* str, const symbol_value* table,
                         bool exclusive, const char* errmsg) {
    int value = 0;
    const char* s = str;
    while (true) {
        const char* comma = strchr(s, ',');
        size_t len = comma ? (size_t)(comma - s) : strlen(s);
        const symbol_value* entry;
        for (entry = table; entry->symbol != NULL; entry++) {
            size_t slen = strlen(entry->symbol);
            if (slen == len && strncmp(s, entry->symbol, slen) == 0) {
                if (exclusive) value = entry->value;
                else value |= entry->value;
                break;
            }
        }
        if (entry->symbol == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "dd: %s: %s\n", errmsg, s);
            return -1;
        }
        if (comma == NULL) break;
        s = comma + 1;
    }
    return value;
}

/* ── I/O helpers ──────────────────────────────────────────────────────── */

static ssize_t dd_read_full(int fd, void* buf, size_t size) {
    size_t total = 0;
    char* p = (char*)buf;
    while (total < size) {
        ssize_t n = read(fd, p + total, size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

static ssize_t dd_read(int fd, void* buf, size_t size, bool fullblock) {
    if (fullblock) return dd_read_full(fd, buf, size);
    return read(fd, buf, size);
}

static ssize_t dd_write_all(int fd, const void* buf, size_t size) {
    size_t total = 0;
    const char* p = (const char*)buf;
    while (total < size) {
        ssize_t n = write(fd, p + total, size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            errno = ENOSPC;
            break;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

static bool is_nul(const char* buf, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (buf[i] != '\0') return false;
    return true;
}

/* Seek/advance FDESC by records*blocksize + *bytes, via lseek or reads.
 * For input (STDIN_FILENO) returns remaining records; for output writes
 * zeros for the residual. */
static int64_t dd_skip(int fd, int64_t records, int64_t blocksize,
                       int64_t* bytes) {
    off_t offset = (off_t)(records * blocksize + *bytes);
    if (lseek(fd, offset, SEEK_CUR) >= 0) {
        *bytes = 0;
        return 0;
    }

    std::vector<char> buf(blocksize > 0 ? (size_t)blocksize : 4096);
    while (records > 0 || *bytes > 0) {
        size_t want = records > 0 ? (size_t)blocksize : (size_t)*bytes;
        ssize_t n = dd_read(fd, buf.data(), want, false);
        if (n < 0) {
            dd_error(strerror(errno));
            return -1;
        }
        if (n == 0) break;
        if (records > 0) records--;
        else *bytes = 0;
    }
    return records;
}

/* ── Conversion helpers ───────────────────────────────────────────────── */

static char newline_character = '\n';
static char space_character = ' ';
static int64_t r_truncate = 0;
static int64_t col = 0;
static int64_t pending_spaces = 0;

static void translate_charset(const char* new_trans) {
    for (int i = 0; i < 256; i++)
        trans_table[i] = new_trans[trans_table[i]];
}

static void translate_buffer(char* buf, int64_t nread) {
    for (int64_t i = 0; i < nread; i++)
        buf[i] = (char)trans_table[(unsigned char)buf[i]];
}

static char* swab_buffer(char* buf, int64_t* nread, int* saved_byte) {
    if (*nread == 0) return buf;

    int prev_saved = *saved_byte;
    if ((prev_saved < 0) == (*nread & 1)) {
        unsigned char c = (unsigned char)buf[--*nread];
        *saved_byte = c;
    } else {
        *saved_byte = -1;
    }

    for (int64_t i = *nread; 1 < i; i -= 2)
        buf[i] = buf[i - 2];

    if (prev_saved < 0)
        return buf + 1;

    buf[1] = (char)prev_saved;
    ++*nread;
    return buf;
}

/* Output buffer (for block/unblock + aggregation). */
static std::vector<char> obuf;
static int64_t oc = 0;
static int64_t output_blocksize_g = 0;

static void write_output(int fd) {
    ssize_t n = dd_write_all(fd, obuf.data(), (size_t)output_blocksize_g);
    if (n != output_blocksize_g) {
        dd_error("write error");
        exit(1);
    }
    oc = 0;
}

static void output_char(int fd, char c) {
    obuf[oc++] = c;
    if (oc >= output_blocksize_g)
        write_output(fd);
}

static void copy_simple(int fd, const char* buf, int64_t nread) {
    const char* start = buf;
    while (nread != 0) {
        int64_t nfree = nread < output_blocksize_g - oc
                            ? nread : output_blocksize_g - oc;
        memcpy(obuf.data() + oc, start, (size_t)nfree);
        nread -= nfree;
        start += nfree;
        oc += nfree;
        if (oc >= output_blocksize_g)
            write_output(fd);
    }
}

static void copy_with_block(int fd, const char* buf, int64_t nread,
                            int64_t cbs) {
    for (int64_t i = 0; i < nread; i++, buf++) {
        if (*buf == newline_character) {
            if (col < cbs) {
                for (int64_t j = col; j < cbs; j++)
                    output_char(fd, space_character);
            }
            col = 0;
        } else {
            if (col == cbs)
                r_truncate++;
            else if (col < cbs)
                output_char(fd, *buf);
            col++;
        }
    }
}

static void copy_with_unblock(int fd, const char* buf, int64_t nread,
                              int64_t cbs) {
    for (int64_t i = 0; i < nread; i++) {
        char c = buf[i];
        if (col++ >= cbs) {
            col = pending_spaces = 0;
            i--;
            output_char(fd, newline_character);
        } else if (c == space_character) {
            pending_spaces++;
        } else {
            while (pending_spaces) {
                output_char(fd, space_character);
                --pending_spaces;
            }
            output_char(fd, c);
        }
    }
}

/* ── Argument scanning ────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static bool scanargs(int argc, char** argv, DdOptions* opts) {
    int64_t blocksize = 0;
    int64_t count = -1;
    int64_t skip = 0;
    int64_t seek = 0;
    bool count_B = false, skip_B = false, seek_B = false;

    for (int i = 1; i < argc; i++) {
        const char* name = argv[i];
        const char* val = strchr(name, '=');
        if (val == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "dd: unrecognized operand %s\n", name);
            return false;
        }
        val++;

        if (operand_is(name, "if")) {
            opts->input_file = val;
        } else if (operand_is(name, "of")) {
            opts->output_file = val;
        } else if (operand_is(name, "conv")) {
            int v = parse_symbols(val, conversions, false, "invalid conversion");
            if (v < 0) return false;
            opts->conv_mask |= v;
        } else if (operand_is(name, "iflag")) {
            int v = parse_symbols(val, flags, false, "invalid input flag");
            if (v < 0) return false;
            opts->in_flags |= v;
        } else if (operand_is(name, "oflag")) {
            int v = parse_symbols(val, flags, false, "invalid output flag");
            if (v < 0) return false;
            opts->out_flags |= v;
        } else if (operand_is(name, "status")) {
            int v = parse_symbols(val, statuses, true, "invalid status level");
            if (v < 0) return false;
            opts->status_level = v;
        } else {
            bool has_B = false;
            int64_t n = parse_number(val, &has_B);
            if (n < 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "dd: invalid number: '%s'\n", val);
                return false;
            }

            if (operand_is(name, "ibs")) {
                if (n < 1) { dd_error("invalid number"); return false; }
                opts->input_blocksize = n;
            } else if (operand_is(name, "obs")) {
                if (n < 1) { dd_error("invalid number"); return false; }
                opts->output_blocksize = n;
            } else if (operand_is(name, "bs")) {
                if (n < 1) { dd_error("invalid number"); return false; }
                blocksize = n;
            } else if (operand_is(name, "cbs")) {
                if (n < 1) { dd_error("invalid number"); return false; }
                opts->conversion_blocksize = n;
            } else if (operand_is(name, "skip") || operand_is(name, "iseek")) {
                skip = n;
                skip_B = has_B;
            } else if (operand_is(name + (*name == 'o'), "seek")) {
                seek = n;
                seek_B = has_B;
            } else if (operand_is(name, "count")) {
                count = n;
                count_B = has_B;
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "dd: unrecognized operand %s\n", name);
                return false;
            }
        }
    }

    if (blocksize) {
        opts->input_blocksize = opts->output_blocksize = blocksize;
    } else {
        opts->conv_mask |= C_TWOBUFS;
    }

    if (opts->input_blocksize == 0)
        opts->input_blocksize = DEFAULT_BLOCKSIZE;
    if (opts->output_blocksize == 0)
        opts->output_blocksize = DEFAULT_BLOCKSIZE;
    if (opts->conversion_blocksize == 0)
        opts->conv_mask &= ~(C_BLOCK | C_UNBLOCK);

    opts->max_records = (count < 0) ? -1 : count;
    opts->max_bytes = 0;

    if (skip_B) {
        opts->in_flags |= O_SKIP_BYTES;
    }
    if (count_B) {
        opts->in_flags |= O_COUNT_BYTES;
    }
    if (opts->in_flags & O_SKIP_BYTES && skip != 0) {
        opts->skip_records = skip / opts->input_blocksize;
        opts->skip_bytes = skip % opts->input_blocksize;
    } else if (skip != 0) {
        opts->skip_records = skip;
    }

    if (opts->in_flags & O_COUNT_BYTES && count >= 0) {
        opts->max_records = count / opts->input_blocksize;
        opts->max_bytes = count % opts->input_blocksize;
    }

    if (seek_B) {
        opts->out_flags |= O_SEEK_BYTES;
    }
    if (opts->out_flags & O_SEEK_BYTES && seek != 0) {
        opts->seek_records = seek / opts->output_blocksize;
        opts->seek_bytes = seek % opts->output_blocksize;
    } else if (seek != 0) {
        opts->seek_records = seek;
    }

    if (opts->out_flags & O_FULLBLOCK) {
        dd_error("invalid output flag: 'fullblock'");
        return false;
    }

    int combined = opts->conv_mask & (C_ASCII | C_EBCDIC | C_IBM);
    if (combined & (combined - 1)) {
        dd_error("cannot combine any two of {ascii,ebcdic,ibm}");
        return false;
    }
    combined = opts->conv_mask & (C_BLOCK | C_UNBLOCK);
    if (combined & (combined - 1)) {
        dd_error("cannot combine block and unblock");
        return false;
    }
    combined = opts->conv_mask & (C_LCASE | C_UCASE);
    if (combined & (combined - 1)) {
        dd_error("cannot combine lcase and ucase");
        return false;
    }
    combined = opts->conv_mask & (C_EXCL | C_NOCREAT);
    if (combined & (combined - 1)) {
        dd_error("cannot combine excl and nocreat");
        return false;
    }

    return true;
}

/* ── Statistics ───────────────────────────────────────────────────────── */

static int64_t r_full = 0, r_partial = 0;
static int64_t w_full = 0, w_partial = 0;
static int64_t w_bytes = 0;

static void print_stats(int status_level) {
    if (status_level == STATUS_NONE) return;

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "%lld+%lld records in\n",
                  (long long)r_full, (long long)r_partial);
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "%lld+%lld records out\n",
                  (long long)w_full, (long long)w_partial);

    if (r_truncate != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "%lld truncated record%s\n",
                      (long long)r_truncate,
                      r_truncate == 1 ? "" : "s");
    }

    if (status_level == STATUS_NOXFER) return;

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "%lld bytes copied\n", (long long)w_bytes);
}

/* ── Main copy loop ───────────────────────────────────────────────────── */

static int dd_copy(int infd, int outfd, const DdOptions* opts) {
    int exit_status = 0;
    std::vector<char> ibuf((size_t)opts->input_blocksize + 1);

    if (opts->skip_records != 0 || opts->skip_bytes != 0) {
        int64_t bytes = opts->skip_bytes;
        dd_skip(infd, opts->skip_records, opts->input_blocksize, &bytes);
    }

    if (opts->seek_records != 0 || opts->seek_bytes != 0) {
        int64_t bytes = opts->seek_bytes;
        int64_t recs = dd_skip(outfd, opts->seek_records,
                               opts->output_blocksize, &bytes);
        (void)recs;
        if (bytes != 0) {
            std::vector<char> z((size_t)opts->output_blocksize, 0);
            dd_write_all(outfd, z.data(), (size_t)bytes);
        }
    }

    if (opts->max_records == 0 && opts->max_bytes == 0)
        return exit_status;

    output_blocksize_g = opts->output_blocksize;
    obuf.assign((size_t)opts->output_blocksize, 0);
    oc = 0;
    col = 0;
    pending_spaces = 0;

    bool twobufs = (opts->conv_mask & C_TWOBUFS) != 0;
    bool fullblock = (opts->in_flags & O_FULLBLOCK) != 0;
    int saved_byte = -1;
    int64_t partread = 0;

    while (true) {
        if (opts->max_records >= 0 &&
            r_full + r_partial >= opts->max_records + (opts->max_bytes ? 1 : 0))
            break;

        int64_t want = (opts->max_records >= 0 &&
                        r_full + r_partial >= opts->max_records)
                           ? opts->max_bytes
                           : opts->input_blocksize;

        if ((opts->conv_mask & C_SYNC) && (opts->conv_mask & C_NOERROR))
            memset(ibuf.data(),
                   (opts->conv_mask & (C_BLOCK | C_UNBLOCK)) ? ' ' : '\0',
                   (size_t)opts->input_blocksize);

        ssize_t nread = dd_read(infd, ibuf.data(), (size_t)want, fullblock);
        if (nread > 0) {
            /* ok */
        } else if (nread == 0) {
            break;
        } else {
            if (!(opts->conv_mask & C_NOERROR) ||
                opts->status_level != STATUS_NONE)
                dd_error(strerror(errno));

            if (opts->conv_mask & C_NOERROR) {
                print_stats(opts->status_level);
                int64_t bad = opts->input_blocksize - partread;
                if ((opts->conv_mask & C_SYNC) && !partread) {
                    nread = 0;
                } else {
                    memmove(ibuf.data(), ibuf.data() + partread,
                            (size_t)(opts->input_blocksize - partread));
                    memset(ibuf.data() + (opts->input_blocksize - partread),
                           '\0', (size_t)partread);
                    nread = (ssize_t)opts->input_blocksize;
                    if (lseek(infd, bad, SEEK_CUR) < 0) {
                        exit_status = 1;
                        break;
                    }
                }
            } else {
                exit_status = 1;
                break;
            }
        }

        int64_t n = nread;

        if (n < opts->input_blocksize) {
            r_partial++;
            partread = n;
            if (opts->conv_mask & C_SYNC) {
                if (!(opts->conv_mask & C_NOERROR))
                    memset(ibuf.data() + n,
                           (opts->conv_mask & (C_BLOCK | C_UNBLOCK)) ? ' ' : '\0',
                           (size_t)(opts->input_blocksize - n));
                n = opts->input_blocksize;
            }
        } else {
            r_full++;
            partread = 0;
        }

        if (!twobufs) {
            ssize_t nwritten = dd_write_all(outfd, ibuf.data(), (size_t)n);
            w_bytes += nwritten;
            if (nwritten != n) {
                dd_error(strerror(errno));
                return 1;
            }
            if (n == opts->input_blocksize) w_full++;
            else w_partial++;
            continue;
        }

        if (opts->conv_mask & (C_ASCII | C_EBCDIC | C_IBM | C_LCASE | C_UCASE))
            translate_buffer(ibuf.data(), n);

        char* bufstart;
        if (opts->conv_mask & C_SWAB)
            bufstart = swab_buffer(ibuf.data(), &n, &saved_byte);
        else
            bufstart = ibuf.data();

        if (opts->conv_mask & C_BLOCK)
            copy_with_block(outfd, bufstart, n, opts->conversion_blocksize);
        else if (opts->conv_mask & C_UNBLOCK)
            copy_with_unblock(outfd, bufstart, n, opts->conversion_blocksize);
        else
            copy_simple(outfd, bufstart, n);
    }

    if (0 <= saved_byte) {
        char sc = (char)saved_byte;
        if (opts->conv_mask & C_BLOCK)
            copy_with_block(outfd, &sc, 1, opts->conversion_blocksize);
        else if (opts->conv_mask & C_UNBLOCK)
            copy_with_unblock(outfd, &sc, 1, opts->conversion_blocksize);
        else
            output_char(outfd, sc);
    }

    if ((opts->conv_mask & C_BLOCK) && col > 0) {
        for (int64_t i = col; i < opts->conversion_blocksize; i++)
            output_char(outfd, space_character);
    }
    if (col && (opts->conv_mask & C_UNBLOCK)) {
        output_char(outfd, newline_character);
    }

    if (oc != 0) {
        ssize_t nwritten = dd_write_all(outfd, obuf.data(), (size_t)oc);
        w_bytes += nwritten;
        if (nwritten != 0) w_partial++;
        if (nwritten != oc) {
            dd_error(strerror(errno));
            return 1;
        }
    }

    if (opts->conv_mask & C_FDATASYNC) {
        if (fdatasync(outfd) != 0) { dd_error(strerror(errno)); exit_status = 1; }
    }
    if (opts->conv_mask & C_FSYNC) {
        if (fsync(outfd) != 0) { dd_error(strerror(errno)); exit_status = 1; }
    }

    return exit_status;
}

/* ── Help ─────────────────────────────────────────────────────────────── */

static void usage(const char* argv0) {
    printf("Usage: %s [OPERAND]...\n", argv0);
    printf("  or:  %s OPTION\n", argv0);
    printf("Copy a file, converting and formatting according to the operands.\n");
    printf("\n");
    printf("  bs=BYTES        read and write up to BYTES bytes at a time (default: 512);\n");
    printf("                  overrides ibs and obs\n");
    printf("  cbs=BYTES       convert BYTES bytes at a time\n");
    printf("  conv=CONVS      convert the file as per the comma separated symbol list\n");
    printf("  count=N         copy only N input blocks\n");
    printf("  ibs=BYTES       read up to BYTES bytes at a time (default: 512)\n");
    printf("  if=FILE         read from FILE instead of stdin\n");
    printf("  iflag=FLAGS     read as per the comma separated symbol list\n");
    printf("  obs=BYTES       write BYTES bytes at a time (default: 512)\n");
    printf("  of=FILE         write to FILE instead of stdout\n");
    printf("  oflag=FLAGS     write as per the comma separated symbol list\n");
    printf("  seek=N          skip N obs-sized output blocks\n");
    printf("  skip=N          skip N ibs-sized input blocks\n");
    printf("  status=LEVEL    ");
    printf("'none' suppresses everything but error messages,\n");
    printf("                  'noxfer' suppresses the final transfer statistics,\n");
    printf("                  'progress' shows periodic transfer statistics\n");
    printf("\n");
    printf("N and BYTES may be followed by multiplicative suffixes:\n");
    printf("c=1, w=2, b=512, kB=1000, K=1024, MB=1000*1000, M=1024*1024, xM=M,\n");
    printf("GB=1000*1000*1000, G=1024*1024*1024, and so on for T, P, E, Z, Y.\n");
    printf("Binary prefixes: KiB=K, MiB=M, etc.\n");
    printf("If N ends in 'B', it counts bytes not blocks.\n");
    printf("\n");
    printf("Each CONV symbol may be:\n");
    printf("  ascii     from EBCDIC to ASCII\n");
    printf("  ebcdic    from ASCII to EBCDIC\n");
    printf("  ibm       from ASCII to alternate EBCDIC\n");
    printf("  block     pad newline-terminated records with spaces to cbs-size\n");
    printf("  unblock   replace trailing spaces in cbs-size records with newline\n");
    printf("  lcase     change upper case to lower case\n");
    printf("  ucase     change lower case to upper case\n");
    printf("  swab      swap every pair of input bytes\n");
    printf("  sync      pad every input block with NULs to ibs-size\n");
    printf("  noerror   continue after read errors\n");
    printf("  notrunc   do not truncate the output file\n");
    printf("  nocreat   do not create the output file\n");
    printf("  excl      fail if the output file already exists\n");
    printf("  fdatasync physically write output file data before finishing\n");
    printf("  fsync     likewise, but also write metadata\n");
    printf("\n");
    printf("Each FLAG symbol may be:\n");
    printf("  append    append mode (makes sense only for output; conv=notrunc suggested)\n");
    printf("  direct    use direct I/O for data\n");
    printf("  sync      use synchronized I/O for data\n");
    printf("  fullblock accumulate full blocks of input (iflag only)\n");
    printf("  nonblock  use non-blocking I/O\n");
    printf("  noatime   do not update access time\n");
    printf("  noctty    do not assign controlling terminal from file\n");
    printf("  nofollow  do not follow symlinks\n");
    printf("  binary    use binary I/O for data\n");
    printf("  text      use text I/O for data\n");
    printf("  count_bytes  count=N is in bytes\n");
    printf("  skip_bytes   skip=N is in bytes\n");
    printf("  seek_bytes   seek=N is in bytes\n");
}

void dd_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return;
        }
    }

    DdOptions opts = {};
    if (!scanargs(argc, argv, &opts)) {
        usage(argv[0]);
        return;
    }

    for (int i = 0; i < 256; i++)
        trans_table[i] = (unsigned char)i;
    if (opts.conv_mask & C_ASCII)
        translate_charset(ebcdic_to_ascii);
    if (opts.conv_mask & C_EBCDIC)
        translate_charset(ascii_to_ebcdic);
    if (opts.conv_mask & C_IBM)
        translate_charset(ascii_to_ibm);
    if (opts.conv_mask & C_LCASE) {
        for (int i = 0; i < 256; i++)
            if (trans_table[i] >= 'A' && trans_table[i] <= 'Z')
                trans_table[i] = (unsigned char)(trans_table[i] - 'A' + 'a');
    }
    if (opts.conv_mask & C_UCASE) {
        for (int i = 0; i < 256; i++)
            if (trans_table[i] >= 'a' && trans_table[i] <= 'z')
                trans_table[i] = (unsigned char)(trans_table[i] - 'a' + 'A');
    }

    if (opts.conv_mask & (C_EBCDIC | C_IBM)) {
        newline_character = (char)ascii_to_ebcdic[(unsigned char)'\n'];
        space_character = (char)ascii_to_ebcdic[(unsigned char)' '];
    }

    int infd = STDIN_FILENO;
    if (opts.input_file != nullptr && strcmp(opts.input_file, "-") != 0) {
        int flags = O_RDONLY;
        int extra = opts.in_flags & (O_DIRECT | O_NONBLOCK | O_NOATIME |
                                     O_NOFOLLOW | O_BINARY | O_DIRECTORY |
                                     O_NOCTTY | O_NOLINKS | O_TEXT | O_DSYNC | O_SYNC);
        flags |= extra;
        infd = open(opts.input_file, flags, 0666);
        if (infd < 0) {
            dd_error(strerror(errno));
            return;
        }
    }

    int outfd = STDOUT_FILENO;
    if (opts.output_file != nullptr && strcmp(opts.output_file, "-") != 0) {
        int flags = O_WRONLY | O_CREAT;
        if (!(opts.conv_mask & C_NOTRUNC)) flags |= O_TRUNC;
        if (opts.out_flags & O_APPEND) flags |= O_APPEND;
        if (opts.conv_mask & C_EXCL) flags |= O_EXCL;
        if (opts.conv_mask & C_NOCREAT) flags &= ~O_CREAT;
        int extra = opts.out_flags & (O_DIRECT | O_NONBLOCK | O_NOATIME |
                                      O_NOFOLLOW | O_BINARY | O_DIRECTORY |
                                      O_NOCTTY | O_NOLINKS | O_TEXT | O_DSYNC | O_SYNC);
        flags |= extra;
        outfd = open(opts.output_file, flags, 0666);
        if (outfd < 0) {
            dd_error(strerror(errno));
            return;
        }
    }

    int status = dd_copy(infd, outfd, &opts);

    if (infd != STDIN_FILENO) close(infd);
    if (outfd != STDOUT_FILENO) close(outfd);

    print_stats(opts.status_level);

    if (status != 0) exit(status);
}

REGISTER_COMMAND("dd", dd_command, "Convert and copy a file");
