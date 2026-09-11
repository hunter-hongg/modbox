#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <iterator>
#include <string>
#include <vector>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <arpa/inet.h>
#include <net/if.h>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/tcpdump.hpp"
#include "commands/version_util.hpp"

namespace {

volatile sig_atomic_t g_stop = 0;

void on_stop(int) { g_stop = 1; }

void install_signal_handlers() {
    struct sigaction sa {};
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

// Summary goes to stderr so stdout stays packet-lines-only (matches real tcpdump).
void print_summary(int captured, int received, int dropped) {
    (void)fprintf(stderr, "%d packets captured, %d packets received, %d dropped\n",
            captured, received, dropped);
}

constexpr size_t kGlobalHeaderSize = 24;
constexpr size_t kRecordHeaderSize = 16;

struct PcapReader {
    FILE* f = nullptr;
    bool big_endian = false;
    uint32_t linktype = 0;
    bool truncated = false;
};

uint32_t b32(const unsigned char* p, size_t off) {
    return (static_cast<uint32_t>(p[off]) << 24U) | (static_cast<uint32_t>(p[off + 1]) << 16U) |
           (static_cast<uint32_t>(p[off + 2]) << 8U) | static_cast<uint32_t>(p[off + 3]);
}

uint32_t l32(const unsigned char* p, size_t off) {
    return static_cast<uint32_t>(p[off]) | (static_cast<uint32_t>(p[off + 1]) << 8U) |
           (static_cast<uint32_t>(p[off + 2]) << 16U) | (static_cast<uint32_t>(p[off + 3]) << 24U);
}

uint32_t b16(const unsigned char* p, size_t off) {
    return (static_cast<uint32_t>(p[off]) << 8U) | static_cast<uint32_t>(p[off + 1]);
}

uint32_t l16(const unsigned char* p, size_t off) {
    return static_cast<uint32_t>(p[off]) | (static_cast<uint32_t>(p[off + 1]) << 8U);
}

uint32_t rd32(const unsigned char* p, size_t off, bool big_endian) {
    return big_endian ? b32(p, off) : l32(p, off);
}

uint32_t rd16(const unsigned char* p, size_t off, bool big_endian) {
    return big_endian ? b16(p, off) : l16(p, off);
}

bool open_pcap(const std::string& path, PcapReader& out, std::string* err) {
    FILE* f = nullptr;
    if (path == "-") {
        f = stdin;
    } else {
        f = fopen(path.c_str(), "rb");
        if (f == nullptr) {
            if (err != nullptr) {
                *err = "tcpdump: cannot open " + path + ": " + std::strerror(errno);
}
            return false;
        }
    }

    unsigned char hdr[kGlobalHeaderSize];
    size_t const n = fread(hdr, 1, sizeof(hdr), f);
    if (n < sizeof(hdr)) {
        if (f != stdin) { (void)fclose(f);
}
        if (err != nullptr) { *err = "tcpdump: " + path + ": not a pcap file";
}
        return false;
    }

    bool big_endian;
    if (hdr[0] == 0xa1 && hdr[1] == 0xb2 && hdr[2] == 0xc3 && hdr[3] == 0xd4) {
        big_endian = false;
    } else if (hdr[0] == 0xd4 && hdr[1] == 0xc3 && hdr[2] == 0xb2 && hdr[3] == 0xa1) {
        big_endian = true;
    } else {
        if (f != stdin) { (void)fclose(f);
}
        if (err != nullptr) { *err = "tcpdump: " + path + ": not a pcap file";
}
        return false;
    }

    uint32_t const version_major = rd16(hdr, 4, big_endian);
    uint32_t const linktype = rd32(hdr, 20, big_endian);

    if (version_major != 2) {
        if (f != stdin) { (void)fclose(f);
}
        if (err != nullptr) {
            *err = "tcpdump: " + path + ": unsupported version " + std::to_string(version_major);
}
        return false;
    }
    if (linktype != 1) {
        if (f != stdin) { (void)fclose(f);
}
        if (err != nullptr) {
            *err = "tcpdump: " + path + ": unsupported linktype " + std::to_string(linktype);
}
        return false;
    }

    out.f = f;
    out.big_endian = big_endian;
    out.linktype = linktype;
    return true;
}

bool read_record(PcapReader& r, uint32_t& ts_sec, uint32_t& ts_usec, std::vector<uint8_t>& bytes) {
    unsigned char hdr[kRecordHeaderSize];
    size_t const n = fread(hdr, 1, sizeof(hdr), r.f);
    if (n == 0 && (feof(r.f) != 0)) { return false;
}
    if (n < sizeof(hdr)) {
        r.truncated = true;
        return false;
    }

    uint32_t const incl_len = rd32(hdr, 8, r.big_endian);
    ts_sec = rd32(hdr, 0, r.big_endian);
    ts_usec = rd32(hdr, 4, r.big_endian);

    bytes.resize(incl_len);
    size_t const got = fread(bytes.data(), 1, incl_len, r.f);
    if (got < incl_len) {
        r.truncated = true;
        return false;
    }
    return true;
}

void close_pcap(PcapReader& r) {
    if ((r.f != nullptr) && r.f != stdin) { (void)fclose(r.f);
}
    r.f = nullptr;
}

// ── pcap writer ─────────────────────────────────────────────────────────────

struct PcapWriter {
    FILE* f = nullptr;
    std::string path;
};

// Returns 0 on success, 2 for a usage error, 1 for a runtime error.
int open_pcap_writer(const std::string& path, PcapWriter& out, std::string* err) {
    if (path == "-") {
        if (err != nullptr) { *err = "tcpdump: -w does not support stdout";
}
        return 2;
    }
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        if (err != nullptr) { *err = "tcpdump: cannot create " + path + ": " + std::strerror(errno);
}
        return 1;
    }
    // global header: v2.4, host byte order (LE on x86), snaplen=65535, linktype=1 (EN10MB)
    unsigned char hdr[kGlobalHeaderSize] = {
        0xa1, 0xb2, 0xc3, 0xd4,
        0x02, 0x00, 0x04, 0x00,  // version 2.4
        0x00, 0x00, 0x00, 0x00,  // thiszone
        0x00, 0x00, 0x00, 0x00,  // sigfigs
        0xff, 0xff, 0x00, 0x00,  // snaplen 65535
        0x01, 0x00, 0x00, 0x00   // linktype EN10MB
    };
    size_t const n = fwrite(hdr, 1, sizeof(hdr), f);
    if (n < sizeof(hdr) || (ferror(f) != 0)) {
        if (err != nullptr) { *err = "tcpdump: write error on " + path;
}
        (void)fclose(f);
        return 1;
    }
    out.f = f;
    out.path = path;
    return 0;
}

bool write_record(PcapWriter& w, const uint8_t* d, size_t dlen,
                         uint32_t ts_sec, uint32_t ts_usec) {
    unsigned char hdr[kRecordHeaderSize];
    // host byte order (little endian on x86)
    uint32_t incl = static_cast<uint32_t>(dlen);
    memcpy(hdr + 0, &ts_sec, 4);
    memcpy(hdr + 4, &ts_usec, 4);
    memcpy(hdr + 8, &incl, 4);
    memcpy(hdr + 12, &incl, 4);  // orig = incl
    if (fwrite(hdr, 1, sizeof(hdr), w.f) < sizeof(hdr) || (ferror(w.f) != 0)) {
        return false;
    }
    if (fwrite(d, 1, dlen, w.f) < dlen || (ferror(w.f) != 0)) {
        return false;
    }
    return true;
}

void close_pcap_writer(PcapWriter& w) {
    if (w.f != nullptr) {
        (void)fflush(w.f);
        (void)fclose(w.f);
        w.f = nullptr;
    }
}




struct TcpdumpOptions {
    std::string input_file;
    std::string output_file;
    std::string filter_expr;
    std::string interface;
    int count = 0;
    int snaplen = 262144;
    bool show_link = false;
    bool brief = false;
    bool verbose = false;
    bool hex_dump = false;
    bool numeric = false;
    bool epoch_ts = false;
};

// ── AF_PACKET capture socket ────────────────────────────────────────────────

// Returns an open capture socket fd, or -1 with *err set.
// Interface resolution: empty or "any" → no bind (all interfaces);
// a named interface → if_nametoindex + bind.
int open_capture_socket(const TcpdumpOptions* opts, std::string* err) {
    int ifindex = 0;
    if (!opts->interface.empty() && opts->interface != "any") {
        ifindex = if_nametoindex(opts->interface.c_str());
        if (ifindex == 0) {
            *err = "tcpdump: unknown interface " + opts->interface;
            return -1;
        }
    }

    int const fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        if (errno == EPERM) {
            *err = "tcpdump: cannot open capture socket: Operation not permitted "
                   "(capture needs root or CAP_NET_RAW; use -r to read a capture file)";
        } else {
            *err = "tcpdump: cannot open capture socket: " + std::string(std::strerror(errno));
        }
        return -1;
    }

    if (ifindex != 0) {
        struct sockaddr_ll sll {};
        sll.sll_family = AF_PACKET;
        sll.sll_protocol = htons(ETH_P_ALL);
        sll.sll_ifindex = ifindex;
        if (bind(fd, reinterpret_cast<struct sockaddr*>(&sll), sizeof(sll)) < 0) {
            *err = "tcpdump: cannot bind capture socket to " + opts->interface + ": " +
                   std::string(std::strerror(errno));
            close(fd);
            return -1;
        }
    }
    return fd;
}

// ── minimal filter expression parser (grammar + accept-all stub) ─────────────
// Grammar:
//   expr := term { "or" term }
//   term := factor { "and" factor }
//   factor := "not" factor | "(" expr ")" | atom
//   atom := keyword value
//   keyword := host | net | port | proto | src | dst | tcp | udp | icmp | arp | ip | ipv6 | icmp6
// Protocol shorthands (atoms with no value) map to proto/net checks internally.

namespace {

enum class Fk { Or, And, Not, Host, Net, Port, Proto, Src, Dst };

struct Node {
    Fk kind;
    int left = -1, right = -1;
    std::string value;
};

using Filter = std::vector<Node>;

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return out;
}

class FilterParseError : public std::runtime_error {
public:
    explicit FilterParseError(const std::string& msg) : std::runtime_error(msg) {}
};

static std::string peek_token(const std::string& expr, size_t& pos);
static Filter parse_expr(const std::string& expr, size_t& pos);
static Filter parse_term(const std::string& expr, size_t& pos);
static Filter parse_factor(const std::string& expr, size_t& pos);



std::string peek_token(const std::string& expr, size_t& pos) {
    while (pos < expr.size() && (std::isspace(static_cast<unsigned char>(expr[pos])) != 0)) {
        ++pos;
}
    if (pos >= expr.size()) { return "";
}
    size_t const start = pos;
    if (expr[pos] == '(' || expr[pos] == ')') {
        ++pos;
        return expr.substr(start, 1);
    }
    while (pos < expr.size() && (std::isspace(static_cast<unsigned char>(expr[pos])) == 0) && expr[pos] != '(' && expr[pos] != ')') {
        ++pos;
}
    return expr.substr(start, pos - start);
}

bool is_keyword(const std::string& t) {
    return t == "host" || t == "net" || t == "port" || t == "proto" ||
           t == "src"  || t == "dst" || t == "and" || t == "or" || t == "not";
}

bool is_proto_shorthand(const std::string& t) {
    return t == "tcp" || t == "udp" || t == "icmp" || t == "arp" ||
           t == "ip" || t == "ipv6" || t == "icmp6";
}

bool parse_port_strict(const std::string& s, uint16_t& out) {
    if (s.empty()) { return false;
}
    for (char c : s) {
        if (c < '0' || c > '9') { return false;
}
}
    long const v = std::stol(s);
    if (v < 1 || v > 65535) { return false;
}
    out = static_cast<uint16_t>(v);
    return true;
}

bool parse_addr_strict(const std::string& s, bool& v6) {
    if (s.find(':') != std::string::npos) {
        struct in6_addr a;
        if (inet_pton(AF_INET6, s.c_str(), &a) != 1) { return false;
}
        v6 = true;
        return true;
    }
    struct in_addr a;
    if (inet_pton(AF_INET, s.c_str(), &a) != 1) { return false;
}
    v6 = false;
    return true;
}

bool parse_family(const std::string& s) {
    return s == "tcp" || s == "udp" || s == "icmp" || s == "icmp6" ||
           s == "arp" || s == "ip" || s == "ipv6";
}

Filter parse_expr(const std::string& expr, size_t& pos) {
    Filter left = parse_term(expr, pos);
    while (pos < expr.size()) {
        size_t const saved = pos;
        std::string const tok = peek_token(expr, pos);
        if (tok == "or") {
            Filter right = parse_term(expr, pos);
            Node n;
            n.kind = Fk::Or;
            n.left = static_cast<int>(left.size());
            n.right = static_cast<int>(right.size());
            left.insert(left.end(), std::make_move_iterator(right.begin()), std::make_move_iterator(right.end()));
            left.push_back(n);
        } else {
            pos = saved;
            break;
        }
    }
    return left;
}

Filter parse_term(const std::string& expr, size_t& pos) {
    Filter left = parse_factor(expr, pos);
    while (pos < expr.size()) {
        size_t const saved = pos;
        std::string const tok = peek_token(expr, pos);
        if (tok == "and") {
            Filter right = parse_factor(expr, pos);
            Node n;
            n.kind = Fk::And;
            n.left = static_cast<int>(left.size());
            n.right = static_cast<int>(right.size());
            left.insert(left.end(), std::make_move_iterator(right.begin()), std::make_move_iterator(right.end()));
            left.push_back(n);
        } else {
            pos = saved;
            break;
        }
    }
    return left;
}

Filter parse_factor(const std::string& expr, size_t& pos) {
    std::string const tok = peek_token(expr, pos);
    if (tok == "not") {
        Filter inner = parse_factor(expr, pos);
        Node n;
        n.kind = Fk::Not;
        n.left = static_cast<int>(inner.size());
        inner.push_back(n);
        return inner;
    }
    if (tok == "(") {
        Filter inner = parse_expr(expr, pos);
        std::string const close = peek_token(expr, pos);
        if (close != ")") { throw FilterParseError("filter error: unexpected token: " + close);
}
        return inner;
    }
    if (tok == ")") { throw FilterParseError("filter error: unexpected ')'");
}
    // atom
    if (!is_keyword(tok)) {
        if (is_proto_shorthand(tok)) {
            Node n;
            n.kind = Fk::Proto;
            n.value = tok;
            return Filter{ n };
        }
        throw FilterParseError("filter error: unexpected token: " + tok);
    }
    std::string const kw = tok;
    std::string const val = peek_token(expr, pos);
    if (val.empty() || (val == "and" || val == "or" || val == ")")) {
        throw FilterParseError("filter error: expected value for '" + kw + "'");
}
    Node n;
    if (kw == "port") {
        n.kind = Fk::Port;
        uint16_t p = 0;
        if (!parse_port_strict(val, p)) {
            throw FilterParseError("filter error: port: invalid value '" + val + "'");
}
        n.value = val;
        return Filter{ n };
    }
    if (kw == "host") {
        n.kind = Fk::Host;
        bool v6 = false;
        if (!parse_addr_strict(val, v6)) {
            throw FilterParseError("filter error: host: invalid address '" + val + "'");
}
        n.value = val;
        return Filter{ n };
    }
    if (kw == "net") {
        n.kind = Fk::Net;
        size_t const slash = val.find('/');
        std::string const addr = (slash != std::string::npos) ? val.substr(0, slash) : val;
        bool v6 = false;
        if (!parse_addr_strict(addr, v6)) {
            throw FilterParseError("filter error: net: invalid address '" + addr + "'");
}
        if (slash != std::string::npos) {
            std::string const pfx = val.substr(slash + 1);
            if (pfx.empty()) {
                throw FilterParseError("filter error: net: missing prefix");
}
            for (char c : pfx) {
                if (c < '0' || c > '9') {
                    throw FilterParseError("filter error: net: invalid prefix '" + pfx + "'");
}
}
            int const p = std::stoi(pfx);
            int const maxp = v6 ? 128 : 32;
            if (p < 0 || p > maxp) {
                throw FilterParseError(std::string("filter error: net: prefix out of range for ") + (v6 ? "IPv6" : "IPv4"));
}
        }
        n.value = val;
        return Filter{ n };
    }
    if (kw == "proto") {
        n.kind = Fk::Proto;
        std::string const vl = to_lower(val);
        if (parse_family(vl)) {
            n.value = val;
            return Filter{ n };
        }
        uint16_t num = 0;
        if (parse_port_strict(val, num) && num <= 255) {
            n.value = val;
            return Filter{ n };
        }
        throw FilterParseError("filter error: proto: unknown protocol '" + val + "'");
    }
    if (kw == "src" || kw == "dst") {
        n.kind = (kw == "src") ? Fk::Src : Fk::Dst;
        bool v6 = false;
        if (parse_addr_strict(val, v6)) {
            n.value = val;
            return Filter{ n };
        }
        uint16_t p = 0;
        if (parse_port_strict(val, p)) {
            n.value = val;
            return Filter{ n };
        }
        throw FilterParseError("filter error: " + kw + ": invalid value '" + val + "'");
    }
    throw FilterParseError("filter error: unexpected keyword '" + kw + "'");
}

Filter compile_filter(const std::string& expr, std::string* err) {
    if (err != nullptr) { err->clear();
}
    if (expr.empty()) {
        throw FilterParseError("filter error: empty expression");
    }
    size_t pos = 0;
    Filter f = parse_expr(expr, pos);
    std::string const tail = expr.substr(pos);
    std::string trimmed = tail;
    std::string::iterator const end = std::remove_if(trimmed.begin(), trimmed.end(), [](char c){ return std::isspace(static_cast<unsigned char>(c)); });
    trimmed.erase(end, trimmed.end());
    if (!trimmed.empty()) {
        std::string const msg = "filter error: trailing characters";
        if (err != nullptr) { *err = msg;
}
        throw FilterParseError(msg);
    }
    return f;
}

bool is_dotted_decimal_ipv4(const std::string& s) {
    int dots = 0;
    std::string const tmp = s;
    if (tmp.size() > 15) { return false;
}
    for (char c : tmp) {
        if (c == '.') { ++dots; if (dots > 3) { return false; 
}continue; }
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) { return false;
}
    }
    return dots == 3;
}

bool is_ipv6(const std::string& s) {
    return s.find(':') != std::string::npos;
}

bool is_net_with_prefix(const std::string& s) {
    size_t const slash = s.find('/');
    if (slash == std::string::npos) { return false;
}
    std::string const addr = s.substr(0, slash);
    std::string const prefix = s.substr(slash + 1);
    if (!is_dotted_decimal_ipv4(addr) && !is_ipv6(addr)) { return false;
}
    for (char c : prefix) { if (std::isdigit(static_cast<unsigned char>(c)) == 0) { return false;
}
}
    return true;
}

// Packet context extracted once per record, shared by filter evaluation and decode.
struct PacketCtx {
    bool valid = false;
    uint16_t ethertype = 0;
    // IPv4
    bool ipv4 = false;
    std::string src_ip, dst_ip;
    uint8_t ipv4_proto = 0;
    // IPv6
    bool ipv6 = false;
    std::string src6, dst6;
    uint8_t ipv6_next = 0;
    // ARP
    bool arp = false;
    std::string arp_spa, arp_tpa;
    // Ports
    uint16_t src_port = 0, dst_port = 0;
    bool has_ports = false;
};

PacketCtx build_context(const uint8_t* d, size_t len) {
    PacketCtx ctx;
    if (len < 14) { return ctx;
}
    ctx.ethertype = (static_cast<uint16_t>(d[12]) << 8) | static_cast<uint16_t>(d[13]);
    if (ctx.ethertype == 0x0800 && len >= 34) {
        ctx.ipv4 = true;
        uint8_t ihl = (d[14] & 0x0F) * 4;
        ihl = std::max<uint8_t>(ihl, 20);  // guard against a malformed 0 IHL
        if (len >= 14 + ihl) {
            struct in_addr src;
            struct in_addr dst;
            memcpy(&src.s_addr, d + 14 + 12, 4);
            memcpy(&dst.s_addr, d + 14 + 16, 4);
            char buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &src, buf, sizeof(buf)) != nullptr) { ctx.src_ip = buf;
}
            if (inet_ntop(AF_INET, &dst, buf, sizeof(buf)) != nullptr) { ctx.dst_ip = buf;
}
            ctx.ipv4_proto = d[14 + 9];
            size_t const t = 14 + ihl;
            if (ctx.ipv4_proto == 6 && len >= t + 20) {
                ctx.src_port = (static_cast<uint16_t>(d[t]) << 8) | d[t + 1];
                ctx.dst_port = (static_cast<uint16_t>(d[t + 2]) << 8) | d[t + 3];
                ctx.has_ports = true;
            } else if (ctx.ipv4_proto == 17 && len >= t + 8) {
                ctx.src_port = (static_cast<uint16_t>(d[t]) << 8) | d[t + 1];
                ctx.dst_port = (static_cast<uint16_t>(d[t + 2]) << 8) | d[t + 3];
                ctx.has_ports = true;
            }
        }
    } else if (ctx.ethertype == 0x0806 && len >= 14 + 28) {
        ctx.arp = true;
        struct in_addr spa;
        struct in_addr tpa;
        memcpy(&spa.s_addr, d + 14 + 14, 4);
        memcpy(&tpa.s_addr, d + 14 + 24, 4);
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &spa, buf, sizeof(buf)) != nullptr) { ctx.arp_spa = buf;
}
        if (inet_ntop(AF_INET, &tpa, buf, sizeof(buf)) != nullptr) { ctx.arp_tpa = buf;
}
    } else if (ctx.ethertype == 0x86DD && len >= 54) {
        ctx.ipv6 = true;
        ctx.ipv6_next = d[14 + 6];
        struct in6_addr src;
        struct in6_addr dst;
        memcpy(&src.s6_addr, d + 14 + 8, 16);
        memcpy(&dst.s6_addr, d + 14 + 24, 16);
        char buf[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &src, buf, sizeof(buf)) != nullptr) { ctx.src6 = buf;
}
        if (inet_ntop(AF_INET6, &dst, buf, sizeof(buf)) != nullptr) { ctx.dst6 = buf;
}
        size_t const t = 14 + 40;
        if (ctx.ipv6_next == 6 && len >= t + 20) {
            ctx.src_port = (static_cast<uint16_t>(d[t]) << 8) | d[t + 1];
            ctx.dst_port = (static_cast<uint16_t>(d[t + 2]) << 8) | d[t + 3];
            ctx.has_ports = true;
        } else if (ctx.ipv6_next == 17 && len >= t + 8) {
            ctx.src_port = (static_cast<uint16_t>(d[t]) << 8) | d[t + 1];
            ctx.dst_port = (static_cast<uint16_t>(d[t + 2]) << 8) | d[t + 3];
            ctx.has_ports = true;
        }
    }
    ctx.valid = true;
    return ctx;
}

bool ip_match(const std::string& a, const std::string& b) {
    return a == b;
}

bool port_match(uint16_t port, uint16_t want) { return port == want; }

bool net_match(const std::string& ip_str, const std::string& spec) {
    size_t const slash = spec.find('/');
    std::string const addr = (slash != std::string::npos) ? spec.substr(0, slash) : spec;
    std::string const pfx_str = (slash != std::string::npos) ? spec.substr(slash + 1) : "32";
    int const pfx = std::stoi(pfx_str);

    bool const is_v6 = ip_str.find(':') != std::string::npos;
    bool const addr_v6 = addr.find(':') != std::string::npos;
    if (is_v6 != addr_v6) { return false;
}

    struct in_addr a4;
    struct in_addr b4;
    struct in6_addr a6;
    struct in6_addr b6;
    if (!is_v6) {
        if (inet_pton(AF_INET, ip_str.c_str(), &a4) != 1) { return false;
}
        if (inet_pton(AF_INET, addr.c_str(), &b4) != 1) { return false;
}
        if (pfx == 0) { return true;
}
        uint32_t const mask = (pfx == 32) ? 0xFFFFFFFFU : (~0U << (32 - pfx));
        return (ntohl(a4.s_addr) & mask) == (ntohl(b4.s_addr) & mask);
    }         if (inet_pton(AF_INET6, ip_str.c_str(), &a6) != 1) { return false;
}
        if (inet_pton(AF_INET6, addr.c_str(), &b6) != 1) { return false;
}
        if (pfx == 0) { return true;
}
        const uint8_t* ap = reinterpret_cast<const uint8_t*>(a6.s6_addr);
        const uint8_t* bp = reinterpret_cast<const uint8_t*>(b6.s6_addr);
        int const full = pfx / 8;
        int const rem = pfx % 8;
        for (int i = 0; i < full; ++i) {
            if (ap[i] != bp[i]) { return false;
}
        }
        if (rem > 0 && (ap[full] & (0xFFu << (8 - rem))) != (bp[full] & (0xFFu << (8 - rem)))) { return false;
}
        return true;
   
}

bool eval_atom(const Node& n, const PacketCtx& ctx) {
    if (n.kind == Fk::Proto) {
        const std::string& v = to_lower(n.value);
        if (v == "arp") { return ctx.ethertype == 0x0806;
}
        if (v == "ip") { return ctx.ipv4;
}
        if (v == "ipv6") { return ctx.ipv6;
}
        if (v == "tcp") { return ctx.ipv4 && ctx.ipv4_proto == 6;
}
        if (v == "udp") { return (ctx.ipv4 && ctx.ipv4_proto == 17) || (ctx.ipv6 && ctx.ipv6_next == 17);
}
        if (v == "icmp") { return ctx.ipv4 && ctx.ipv4_proto == 1;
}
        if (v == "icmp6") { return ctx.ipv6 && ctx.ipv6_next == 58;
}
        if (!v.empty()) {
            bool all_digits = true;
            for (char c : v) {
                if (c < '0' || c > '9') { all_digits = false; break; }
}
            if (all_digits) {
                int const num = std::stoi(v);
                return ctx.ipv4 && ctx.ipv4_proto == static_cast<uint8_t>(num);
            }
        }
        return false;
    }
    if (!ctx.ipv4 && !ctx.ipv6 && !ctx.arp) { return false;
}
    if (n.kind == Fk::Host) {
        const std::string& v = n.value;
        return ip_match(ctx.src_ip, v) || ip_match(ctx.dst_ip, v) ||
               ip_match(ctx.src6, v) || ip_match(ctx.dst6, v) ||
               ip_match(ctx.arp_spa, v) || ip_match(ctx.arp_tpa, v);
    }
    if (n.kind == Fk::Net) {
        return net_match(ctx.src_ip, n.value) || net_match(ctx.dst_ip, n.value) ||
               net_match(ctx.src6, n.value) || net_match(ctx.dst6, n.value) ||
               net_match(ctx.arp_spa, n.value) || net_match(ctx.arp_tpa, n.value);
    }
    if (n.kind == Fk::Port) {
        if (!ctx.has_ports) { return false;
}
        uint16_t const p = static_cast<uint16_t>(std::stoul(n.value));
        return port_match(ctx.src_port, p) || port_match(ctx.dst_port, p);
    }
    if (n.kind == Fk::Src) {
        const std::string& v = n.value;
        bool const as_ip = v.find('.') != std::string::npos || v.find(':') != std::string::npos;
        if (as_ip) {
            return ip_match(ctx.src_ip, v) || ip_match(ctx.src6, v) || ip_match(ctx.arp_spa, v);
}
        uint16_t const p = static_cast<uint16_t>(std::stoul(v));
        return ctx.has_ports && port_match(ctx.src_port, p);
    }
    if (n.kind == Fk::Dst) {
        const std::string& v = n.value;
        bool const as_ip = v.find('.') != std::string::npos || v.find(':') != std::string::npos;
        if (as_ip) {
            return ip_match(ctx.dst_ip, v) || ip_match(ctx.dst6, v) || ip_match(ctx.arp_tpa, v);
}
        uint16_t const p = static_cast<uint16_t>(std::stoul(v));
        return ctx.has_ports && port_match(ctx.dst_port, p);
    }
    return false;
}

bool evaluate_filter(const Filter& f, const PacketCtx& ctx) {
    if (f.empty()) { return true;
}
    const Node& root = f.back();
    if (root.kind == Fk::And || root.kind == Fk::Or) {
        return (root.kind == Fk::And ? evaluate_filter(Filter(f.begin(), f.begin() + root.left), ctx) &&
                                       evaluate_filter(Filter(f.begin() + root.left, f.end() - 1), ctx)
                                    : evaluate_filter(Filter(f.begin(), f.begin() + root.left), ctx) ||
                                       evaluate_filter(Filter(f.begin() + root.left, f.end() - 1), ctx));
    }
    if (root.kind == Fk::Not) {
        return !evaluate_filter(Filter(f.begin(), f.begin() + root.left), ctx);
    }
    return eval_atom(root, ctx);
}

}  // namespace

// Returns true when the record produces no output line (silently skipped).
bool record_is_undecodable(const uint8_t* d, size_t len) {
    if (len < 14) { return true;
}
    uint16_t const ethertype = (static_cast<uint16_t>(d[12]) << 8) | static_cast<uint16_t>(d[13]);
    if (ethertype == 0x0806) { return len < 14 + 28;
}
    if (ethertype == 0x0800) {
        if (len < 14 + 20) { return true;
}
        uint8_t const ihl = (d[14] & 0x0F) * 4;
        if (len < 14 + ihl) { return true;
}
        uint8_t const proto = d[14 + 9];
        size_t const payload = len - 14 - ihl;
        if (proto == 6) { return payload > 0 && payload < 20;
}
        if (proto == 17) { return payload > 0 && payload < 8;
}
        if (proto == 1) { return payload > 0 && payload < 4;
}
        return false;
    }
    if (ethertype == 0x86DD) { return len < 14 + 40;
}
    return false;
}

void print_hex_dump(const uint8_t* d, size_t len) {
    for (size_t off = 0; off < len; off += 16) {
        size_t const rowlen = (len - off < 16) ? (len - off) : 16;
        printf("%04zx  ", off);
        for (size_t i = 0; i < rowlen; ++i) {
            if (i > 0) { printf(" ");
}
            printf("%02x", d[off + i]);
        }
        printf("\n");
    }
    for (size_t off = 0; off < len; off += 16) {
        size_t const rowlen = (len - off < 16) ? (len - off) : 16;
        printf("     |");
        for (size_t i = 0; i < rowlen; ++i) {
            uint8_t const c = d[off + i];
            printf("%c", (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.');
        }
        printf("|\n");
    }
}

void decode_packet_line(const uint8_t* d, size_t len, uint32_t ts_sec, uint32_t ts_usec, const TcpdumpOptions* opts) {
    if (record_is_undecodable(d, len)) {
        return;
    }
    if (opts->epoch_ts) {
        printf("%u.%06u ", ts_sec, ts_usec);
    } else {
        time_t const t = static_cast<time_t>(ts_sec);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        printf("%02d:%02d:%02d.%06u ", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ts_usec);
    }

    char src_mac[18] = "";
    char dst_mac[18] = "";
    if (opts->show_link) {
        (void)snprintf(dst_mac, sizeof(dst_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 d[0], d[1], d[2], d[3], d[4], d[5]);
        (void)snprintf(src_mac, sizeof(src_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 d[6], d[7], d[8], d[9], d[10], d[11]);
        printf("%s > %s, ", src_mac, dst_mac);
    }

    uint16_t const ethertype = (static_cast<uint16_t>(d[12]) << 8) | static_cast<uint16_t>(d[13]);

    if (ethertype == 0x0806) {
        if (len < 14 + 28) {
            return;
        }
        uint16_t const op = (static_cast<uint16_t>(d[14 + 6]) << 8) | static_cast<uint16_t>(d[14 + 7]);
        struct in_addr spa_addr;
        struct in_addr tpa_addr;
        memcpy(&spa_addr.s_addr, d + 14 + 14, 4);
        memcpy(&tpa_addr.s_addr, d + 14 + 24, 4);
        char spa_str[INET_ADDRSTRLEN];
        char tpa_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &spa_addr, spa_str, sizeof(spa_str));
        inet_ntop(AF_INET, &tpa_addr, tpa_str, sizeof(tpa_str));
        if (opts->brief) {
            printf("ARP, length %zu\n", len);
        } else if (op == 1) {
            printf("ARP, Request, who has %s tell %s, length %zu\n", tpa_str, spa_str, len);
        } else if (op == 2) {
            char sha_mac[18];
            (void)snprintf(sha_mac, sizeof(sha_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     d[14 + 8], d[14 + 9], d[14 + 10], d[14 + 11], d[14 + 12], d[14 + 13]);
            printf("ARP, Reply, %s is-at %s, length %zu\n", spa_str, sha_mac, len);
        } else {
            printf("ARP, Unknown op %u, length %zu\n", op, len);
        }
    } else if (ethertype == 0x0800) {
        if (len < 14 + 20) {
            return;
        }
        uint8_t const ver_ihl = d[14];
        uint8_t ihl = (ver_ihl & 0x0F) * 4;
        ihl = std::max<uint8_t>(ihl, 20);  // guard against a malformed 0 IHL
        if (len < 14 + ihl) {
            return;
        }
        if ((ver_ihl >> 4) != 4) {
            printf("EtherType 0x%04x, length %zu\n", ethertype, len);
            return;
        }
        uint8_t const proto = d[14 + 9];
        struct in_addr src_addr;
        struct in_addr dst_addr;
        memcpy(&src_addr.s_addr, d + 14 + 12, 4);
        memcpy(&dst_addr.s_addr, d + 14 + 16, 4);
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_addr, src_ip, sizeof(src_ip));
        inet_ntop(AF_INET, &dst_addr, dst_ip, sizeof(dst_ip));
        size_t const payload_len = len - 14 - ihl;
        // TCP (proto 6)
        if (proto == 6) {
            if (payload_len >= 20) {
                size_t const t = 14 + ihl;
                uint16_t const sport = (static_cast<uint16_t>(d[t]) << 8) | static_cast<uint16_t>(d[t + 1]);
                uint16_t const dport = (static_cast<uint16_t>(d[t + 2]) << 8) | static_cast<uint16_t>(d[t + 3]);
                uint32_t const seq = (static_cast<uint32_t>(d[t + 4]) << 24) | (static_cast<uint16_t>(d[t + 5]) << 16) |
                               (static_cast<uint16_t>(d[t + 6]) << 8) | static_cast<uint16_t>(d[t + 7]);
                uint32_t const ack_no = (static_cast<uint32_t>(d[t + 8]) << 24) | (static_cast<uint16_t>(d[t + 9]) << 16) |
                                  (static_cast<uint16_t>(d[t + 10]) << 8) | static_cast<uint16_t>(d[t + 11]);
                uint8_t const data_offset = d[t + 12] >> 4;
                uint8_t const flags = d[t + 13];
                uint16_t const window = (static_cast<uint16_t>(d[t + 14]) << 8) | static_cast<uint16_t>(d[t + 15]);
                size_t tcp_hdr_len = static_cast<size_t>(data_offset) * 4;
                tcp_hdr_len = std::max<size_t>(tcp_hdr_len, 20);
                size_t const tcp_payload = (payload_len > tcp_hdr_len) ? payload_len - tcp_hdr_len : 0;

                if (opts->brief) {
                    printf("%s > %s: TCP, length %zu\n", src_ip, dst_ip, tcp_payload);
                    return;
                }
                char flag_buf[16] = "";
                int pos = 0;
                if ((flags & 0x01) != 0) { flag_buf[pos++] = 'F';
}
                if ((flags & 0x02) != 0) { flag_buf[pos++] = 'S';
}
                if ((flags & 0x04) != 0) { flag_buf[pos++] = 'R';
}
                if ((flags & 0x08) != 0) { flag_buf[pos++] = 'P';
}
                if ((flags & 0x10) != 0) { flag_buf[pos++] = '.';
}
                if ((flags & 0x20) != 0) { flag_buf[pos++] = 'U';
}
                flag_buf[pos] = '\0';

                printf("%s.%u > %s.%u: Flags [%s], seq %u", src_ip, sport, dst_ip, dport, flag_buf, seq);
                if ((flags & 0x10) != 0 || opts->verbose) { printf(", ack %u", ack_no);
}
                printf(", win %u, length %zu", window, tcp_payload);

                if (opts->verbose) {
                    if (tcp_hdr_len > 20) {
                        std::string opt_desc;
                        size_t o = t + 20;
                        size_t const opt_end = t + tcp_hdr_len;
                        while (o < opt_end) {
                            uint8_t const kind = d[o];
                            if (kind == 0) { break;
}
                            if (kind == 1) {
                                if (!opt_desc.empty()) { opt_desc += ", ";
}
                                opt_desc += "nop";
                                o += 1;
                                continue;
                            }
                            if (o + 1 >= opt_end) { break;
}
                            uint8_t const olen = d[o + 1];
                            if (olen < 2 || o + static_cast<size_t>(olen) > opt_end) { break;
}
                            if (kind == 2 && olen == 4) {
                                if (!opt_desc.empty()) { opt_desc += ", ";
}
                                opt_desc += "mss " + std::to_string((static_cast<uint16_t>(d[o + 2]) << 8) | d[o + 3]);
                            } else if (kind == 3 && olen == 3) {
                                if (!opt_desc.empty()) { opt_desc += ", ";
}
                                opt_desc += "wscale " + std::to_string(static_cast<int>(d[o + 2]));
                            } else if (kind == 4 && olen == 2) {
                                if (!opt_desc.empty()) { opt_desc += ", ";
}
                                opt_desc += "sackOK";
                            } else if (kind == 8 && olen == 10) {
                                if (!opt_desc.empty()) { opt_desc += ", ";
}
                                uint32_t const ts_val = (static_cast<uint32_t>(d[o + 2]) << 24) | (static_cast<uint16_t>(d[o + 3]) << 16) |
                                                  (static_cast<uint16_t>(d[o + 4]) << 8) | static_cast<uint16_t>(d[o + 5]);
                                uint32_t const ts_ecr = (static_cast<uint32_t>(d[o + 6]) << 24) | (static_cast<uint16_t>(d[o + 7]) << 16) |
                                                  (static_cast<uint16_t>(d[o + 8]) << 8) | static_cast<uint16_t>(d[o + 9]);
                                opt_desc += "TS val " + std::to_string(ts_val) + " ecr " + std::to_string(ts_ecr);
                            }
                            o += olen;
                        }
                        if (!opt_desc.empty()) { printf(", options [%s]", opt_desc.c_str());
}
                    }
                    uint8_t const ttl = d[14 + 8];
                    uint16_t const id_val = (static_cast<uint16_t>(d[14 + 4]) << 8) | static_cast<uint16_t>(d[14 + 5]);
                    uint16_t const flags_ip = (static_cast<uint16_t>(d[14 + 6]) << 8) | static_cast<uint16_t>(d[14 + 7]);
                    printf(", ttl %u, id %u", ttl, id_val);
                    if ((flags_ip & 0x4000) != 0) { printf(", DF");
}
                    if ((flags_ip & 0x2000) != 0) { printf(", MF");
}
                }
                printf("\n");
            } else if (payload_len > 0) {
                return;
            } else {
                if (opts->brief) {
                    printf("%s > %s: IP, length %zu\n", src_ip, dst_ip, payload_len);
                } else {
                    printf("%s > %s: IP, proto %u, length %zu\n", src_ip, dst_ip, proto, payload_len);
                }
            }
        }
        // UDP (proto 17)
        else if (proto == 17) {
            if (payload_len >= 8) {
                size_t const t = 14 + ihl;
                uint16_t const sport = (static_cast<uint16_t>(d[t]) << 8) | static_cast<uint16_t>(d[t + 1]);
                uint16_t const dport = (static_cast<uint16_t>(d[t + 2]) << 8) | static_cast<uint16_t>(d[t + 3]);
                uint16_t const udp_len = (static_cast<uint16_t>(d[t + 4]) << 8) | static_cast<uint16_t>(d[t + 5]);
                size_t udp_payload = 0;
                if (udp_len > 8) {
                    udp_payload = static_cast<size_t>(udp_len) - 8;
                    udp_payload = std::min(udp_payload, payload_len - 8);
                }
                if (opts->brief) {
                    printf("%s > %s: UDP, length %zu\n", src_ip, dst_ip, udp_payload);
                } else {
                    printf("%s.%u > %s.%u: UDP, length %zu\n", src_ip, sport, dst_ip, dport, udp_payload);
                }
            }
            // payload_len in (0, 8) is skipped (see record_is_undecodable)
        }
        // ICMP (proto 1)
        else if (proto == 1) {
            if (payload_len >= 4) {
                uint8_t const icmp_type = d[14 + ihl];
                uint8_t const icmp_code = d[14 + ihl + 1];
                size_t const icmp_len = payload_len;
                if (opts->brief) {
                    printf("%s > %s: ICMP, length %zu\n", src_ip, dst_ip, icmp_len);
                } else if (icmp_type == 0) {
                    if (payload_len >= 8) {
                        uint16_t const id = (static_cast<uint16_t>(d[14 + ihl + 4]) << 8) | static_cast<uint16_t>(d[14 + ihl + 5]);
                        uint16_t const seq = (static_cast<uint16_t>(d[14 + ihl + 6]) << 8) | static_cast<uint16_t>(d[14 + ihl + 7]);
                        printf("%s > %s: ICMP echo reply, id %u, seq %u, length %zu\n", src_ip, dst_ip, id, seq, icmp_len);
                    } else {
                        printf("%s > %s: ICMP echo reply, length %zu\n", src_ip, dst_ip, icmp_len);
                    }
                } else if (icmp_type == 3) {
                    printf("%s > %s: ICMP destination unreachable, length %zu\n", src_ip, dst_ip, icmp_len);
                } else if (icmp_type == 8) {
                    if (payload_len >= 8) {
                        uint16_t const id = (static_cast<uint16_t>(d[14 + ihl + 4]) << 8) | static_cast<uint16_t>(d[14 + ihl + 5]);
                        uint16_t const seq = (static_cast<uint16_t>(d[14 + ihl + 6]) << 8) | static_cast<uint16_t>(d[14 + ihl + 7]);
                        printf("%s > %s: ICMP echo request, id %u, seq %u, length %zu\n", src_ip, dst_ip, id, seq, icmp_len);
                    } else {
                        printf("%s > %s: ICMP echo request, length %zu\n", src_ip, dst_ip, icmp_len);
                    }
                } else if (icmp_type == 11) {
                    printf("%s > %s: ICMP time exceeded, length %zu\n", src_ip, dst_ip, icmp_len);
                } else {
                    printf("%s > %s: ICMP type %u code %u, length %zu\n", src_ip, dst_ip, icmp_type, icmp_code, icmp_len);
                }
            }
            // payload_len in (0, 4) is skipped (see record_is_undecodable)
        }
        // Generic fallback
        else {
            printf("%s > %s: IP, proto %u, length %zu\n", src_ip, dst_ip, proto, payload_len);
        }
    } else if (ethertype == 0x86DD) {
        if (len < 14 + 40) {
            return;
        }
        uint8_t const ver = d[14] >> 4;
        if (ver != 6) {
            printf("EtherType 0x%04x, length %zu\n", ethertype, len);
            return;
        }
        uint16_t const payload_len_field = (static_cast<uint16_t>(d[14 + 4]) << 8) | static_cast<uint16_t>(d[14 + 5]);
        uint8_t const next = d[14 + 6];
        uint8_t const hop = d[14 + 7];
        struct in6_addr src6;
        struct in6_addr dst6;
        memcpy(&src6.s6_addr, d + 14 + 8, 16);
        memcpy(&dst6.s6_addr, d + 14 + 24, 16);
        char src_str[INET6_ADDRSTRLEN];
        char dst_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &src6, src_str, sizeof(src_str));
        inet_ntop(AF_INET6, &dst6, dst_str, sizeof(dst_str));
        size_t const ipv6_hdr_len = 40;
        size_t const rest_len = (len > 14 + ipv6_hdr_len) ? len - 14 - ipv6_hdr_len : 0;
        if (next == 6) { // TCP
            size_t const tcp_off = 14 + 40;
            if (len < tcp_off + 20) {
                if (opts->brief) {
                    printf("%s > %s: IP6, length %zu\n", src_str, dst_str, rest_len);
                } else {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                    if (opts->verbose) { printf(", hop limit %u", hop);
}
                }
                printf("\n");
                return;
            }
            uint16_t const sport = (static_cast<uint16_t>(d[tcp_off]) << 8) | static_cast<uint16_t>(d[tcp_off + 1]);
            uint16_t const dport = (static_cast<uint16_t>(d[tcp_off + 2]) << 8) | static_cast<uint16_t>(d[tcp_off + 3]);
            uint32_t const seq = (static_cast<uint32_t>(d[tcp_off + 4]) << 24) | (static_cast<uint32_t>(d[tcp_off + 5]) << 16) |
                           (static_cast<uint32_t>(d[tcp_off + 6]) << 8) | static_cast<uint32_t>(d[tcp_off + 7]);
            uint16_t const window = (static_cast<uint16_t>(d[tcp_off + 14]) << 8) | static_cast<uint16_t>(d[tcp_off + 15]);
            uint8_t const data_offset = d[tcp_off + 12] >> 4;
            uint8_t const flags = d[tcp_off + 13];
            size_t tcp_hdr_len = static_cast<size_t>(data_offset) * 4;
            tcp_hdr_len = std::max<size_t>(tcp_hdr_len, 20);
            size_t tcp_payload = 0;
            if (len > tcp_off + tcp_hdr_len) { tcp_payload = len - tcp_off - tcp_hdr_len;
}
            if (opts->brief) {
                printf("%s > %s: TCP, length %zu\n", src_str, dst_str, tcp_payload);
                return;
            }
            char flag_buf[16] = "";
            int pos = 0;
            if ((flags & 0x01) != 0) { flag_buf[pos++] = 'F';
}
            if ((flags & 0x02) != 0) { flag_buf[pos++] = 'S';
}
            if ((flags & 0x04) != 0) { flag_buf[pos++] = 'R';
}
            if ((flags & 0x08) != 0) { flag_buf[pos++] = 'P';
}
            if ((flags & 0x10) != 0) { flag_buf[pos++] = '.';
}
            if ((flags & 0x20) != 0) { flag_buf[pos++] = 'U';
}
            flag_buf[pos] = '\0';
            printf("%s.%u > %s.%u: Flags [%s], seq %u, win %u, length %zu", src_str, sport, dst_str, dport, flag_buf, seq, window, tcp_payload);
            if (opts->verbose) { printf(", hop limit %u", hop);
}
            printf("\n");
        } else if (next == 17) { // UDP
            size_t const udp_off = 14 + 40;
            if (len < udp_off + 8) {
                if (opts->brief) {
                    printf("%s > %s: IP6, length %zu\n", src_str, dst_str, rest_len);
                } else {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                    if (opts->verbose) { printf(", hop limit %u", hop);
}
                }
                printf("\n");
                return;
            }
            uint16_t const sport = (static_cast<uint16_t>(d[udp_off]) << 8) | static_cast<uint16_t>(d[udp_off + 1]);
            uint16_t const dport = (static_cast<uint16_t>(d[udp_off + 2]) << 8) | static_cast<uint16_t>(d[udp_off + 3]);
            uint16_t const udp_len_field = (static_cast<uint16_t>(d[udp_off + 4]) << 8) | static_cast<uint16_t>(d[udp_off + 5]);
            size_t udp_payload = 0;
            if (udp_len_field > 8) {
                udp_payload = static_cast<size_t>(udp_len_field) - 8;
                size_t const available = (len > udp_off + 8) ? len - udp_off - 8 : 0;
                udp_payload = std::min(udp_payload, available);
            }
            if (opts->brief) {
                printf("%s > %s: UDP, length %zu", src_str, dst_str, udp_payload);
            } else {
                printf("%s.%u > %s.%u: UDP, length %zu", src_str, sport, dst_str, dport, udp_payload);
            }
            if (opts->verbose) { printf(", hop limit %u", hop);
}
            printf("\n");
        } else if (next == 58) { // ICMPv6
            size_t const icmp_off = 14 + 40;
            if (len < icmp_off + 4) {
                if (opts->brief) {
                    printf("%s > %s: IP6, length %zu\n", src_str, dst_str, rest_len);
                } else {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                    if (opts->verbose) { printf(", hop limit %u", hop);
}
                }
                printf("\n");
                return;
            }
            uint8_t const type = d[icmp_off];
            uint8_t const code = d[icmp_off + 1];
            size_t const icmp_len = rest_len;
            if (opts->brief) {
                printf("%s > %s: ICMP6, length %zu\n", src_str, dst_str, icmp_len);
                return;
            }
            if (type == 128) {
                if (len < icmp_off + 8) {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                } else {
                    uint16_t const id = (static_cast<uint16_t>(d[icmp_off + 4]) << 8) | static_cast<uint16_t>(d[icmp_off + 5]);
                    uint16_t const seqv = (static_cast<uint16_t>(d[icmp_off + 6]) << 8) | static_cast<uint16_t>(d[icmp_off + 7]);
                    printf("%s > %s: ICMP6, echo request, id %u, seq %u", src_str, dst_str, id, seqv);
                }
            } else if (type == 129) {
                if (len < icmp_off + 8) {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                } else {
                    uint16_t const id = (static_cast<uint16_t>(d[icmp_off + 4]) << 8) | static_cast<uint16_t>(d[icmp_off + 5]);
                    uint16_t const seqv = (static_cast<uint16_t>(d[icmp_off + 6]) << 8) | static_cast<uint16_t>(d[icmp_off + 7]);
                    printf("%s > %s: ICMP6, echo reply, id %u, seq %u", src_str, dst_str, id, seqv);
                }
            } else if (type == 133) {
                printf("%s > %s: ICMP6, router solicitation, length %zu", src_str, dst_str, icmp_len);
            } else if (type == 134) {
                printf("%s > %s: ICMP6, router advertisement, length %zu", src_str, dst_str, icmp_len);
            } else if (type == 135) {
                printf("%s > %s: ICMP6, neighbor solicitation, length %zu", src_str, dst_str, icmp_len);
            } else if (type == 136) {
                printf("%s > %s: ICMP6, neighbor advertisement, length %zu", src_str, dst_str, icmp_len);
            } else {
                printf("%s > %s: ICMP6 type %u code %u, length %zu", src_str, dst_str, type, code, icmp_len);
            }
            if (opts->verbose) { printf(", hop limit %u", hop);
}
            printf("\n");
        } else {
            if (opts->brief) {
                printf("%s > %s: IP6, length %zu", src_str, dst_str, rest_len);
            } else {
                printf("%s > %s: IP6, next %u, length %zu", src_str, dst_str, next, rest_len);
                if (opts->verbose) { printf(", hop limit %u", hop);
}
            }
            printf("\n");
        }
    } else {
        printf("EtherType 0x%04x, length %zu\n", ethertype, len);
    }
}

void decode_packet(const uint8_t* d, size_t len, uint32_t ts_sec, uint32_t ts_usec, const TcpdumpOptions* opts) {
    if (record_is_undecodable(d, len)) {
        return;
    }
    decode_packet_line(d, len, ts_sec, ts_usec, opts);
    if (opts->hex_dump) {
        print_hex_dump(d, len);
    }
}

// Shared -r / live pipeline: decode → filter → print → write.
// Returns true when the packet is kept (undecodable and filter-rejected packets are dropped).
bool process_packet(const uint8_t* d, size_t len, uint32_t ts_sec, uint32_t ts_usec,
                           const TcpdumpOptions* opts, const Filter& filter, PcapWriter& writer) {
    (void)&writer;  // suppress unused-param warning when writer is ignored
    if (record_is_undecodable(d, len)) { return false;
}
    if (!filter.empty()) {
        PacketCtx const ctx = build_context(d, len);
        if (!evaluate_filter(filter, ctx)) { return false;
}
    }
    decode_packet(d, len, ts_sec, ts_usec, opts);
    if (writer.f != nullptr) { write_record(writer, d, len, ts_sec, ts_usec);
}
    return true;
}

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [EXPR]\n", prog);
    printf("Capture and display network packets.\n");
    printf("\n");
    printf("  -r <file>       read packets from <file> (pcap format)\n");
    printf("  -w <file>       write packets to <file> (pcap format)\n");
    printf("  -c <count>      exit after receiving <count> packets\n");
    printf("  -i <interface>  listen on <interface>\n");
    printf("  -s <snaplen>    capture <snaplen> bytes of each packet (default 262144)\n");
    printf("  -e              print the link-level header on each line\n");
    printf("  -q              quiet (print less protocol information)\n");
    printf("  -v              verbose output (more protocol information)\n");
    printf("  -x              print packet hex dump\n");
    printf("  -n              don't convert addresses to names\n");
    printf("  -nn             don't convert protocol numbers to names either\n");
    printf("  -tt             print unformatted timestamps\n");
    printf("  -f <expr>       set the capture filter expression\n");
    printf("  -h, --help      display this help and exit\n");
    printf("  -V, --version   output version information and exit\n");
    printf("\n");
    printf("FILTER SYNTAX\n");
    printf("  expr := term {\"or\" term}\n");
    printf("  term := factor {\"and\" factor}\n");
    printf("  factor := \"not\" factor | \"(\" expr \")\" | atom\n");
    printf("  atom := host | net | port | proto | src | dst\n");
    printf("  protocol shorthands: tcp udp icmp arp ip ipv6 icmp6\n");
    printf("\n");
    printf("EXAMPLES\n");
    printf("  %s -i eth0                     capture all traffic on eth0\n", prog);
    printf("  %s -i eth0 port 80            capture HTTP traffic on port 80\n", prog);
    printf("  %s -r capture.pcap tcp        read a file, filter for TCP packets\n", prog);
}

}  // namespace

int tcpdump_command(int argc, char** argv) {
    const char* prog = argv[0];

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-tt") == 0) {
            static char tt_buf[] = "--tt\0";
            argv[i] = tt_buf;
        } else if (std::strcmp(argv[i], "-nn") == 0) {
            static char nn_buf[] = "--nn\0";
            argv[i] = nn_buf;
        }
    }

    struct arg_str* input_opt = arg_str0("r", "read", "<file>", "read packets from <file>");
    struct arg_str* output_opt = arg_str0("w", "write", "<file>", "write raw packets to <file>");
    struct arg_str* filter_opt = arg_str0("f", "filter", "<expr>", "capture filter expression");
    struct arg_str* iface_opt = arg_str0("i", "interface", "<interface>", "listen on <interface>");
    struct arg_int* count_opt = arg_int0("c", "count", "<count>", "exit after receiving <count> packets");
    struct arg_int* snaplen_opt = arg_int0("s", "snaplen", "<snaplen>", "capture <snaplen> bytes of each packet");
    struct arg_lit* link_opt = arg_lit0("e", "link", "print the link-level header on each line");
    struct arg_lit* brief_opt = arg_lit0("q", "quiet", "quiet output");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* hex_opt = arg_lit0("x", "hex", "print packet hex dump");
    struct arg_lit* numeric_opt = arg_lit0("n", "numeric", "don't convert addresses to names");
    struct arg_lit* numeric2_opt = arg_lit0(NULL, "nn", "don't convert protocol numbers either");
    struct arg_lit* epoch_opt = arg_lit0(NULL, "tt", "print unformatted timestamps");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_end* end = arg_end(15);

    ArgTable at({input_opt, output_opt, filter_opt, iface_opt, count_opt, snaplen_opt,
                 link_opt, brief_opt, verbose_opt, hex_opt, numeric_opt, numeric2_opt,
                 epoch_opt, help_opt, version_opt, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_help(prog);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("tcpdump");
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, prog);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return 2;
    }

    TcpdumpOptions opts;
    if (input_opt->count > 0) { opts.input_file = input_opt->sval[0];
}
    if (output_opt->count > 0) { opts.output_file = output_opt->sval[0];
}
    if (iface_opt->count > 0) { opts.interface = iface_opt->sval[0];
}
    if (count_opt->count > 0) { opts.count = count_opt->ival[0];
}
    if (snaplen_opt->count > 0) { opts.snaplen = snaplen_opt->ival[0];
}
    opts.show_link = (link_opt->count > 0);
    opts.brief = (brief_opt->count > 0);
    opts.verbose = (verbose_opt->count > 0);
    opts.hex_dump = (hex_opt->count > 0);
    opts.numeric = (numeric_opt->count > 0 || numeric2_opt->count > 0);
    opts.epoch_ts = (epoch_opt->count > 0);

    Filter filter;
    if (filter_opt->count > 0) {
        opts.filter_expr = filter_opt->sval[0];
        std::string err;
        try {
            filter = compile_filter(opts.filter_expr, &err);
        } catch (const FilterParseError& e) {
            (void)fprintf(stderr, "tcpdump: %s\n", e.what());
            return 2;
        }
    }

    if (!opts.input_file.empty() && !opts.output_file.empty()) {
        if (opts.input_file == opts.output_file) {
            (void)fprintf(stderr, "tcpdump: -r and -w must not name the same file\n");
            return 2;
        }
    }

    PcapWriter writer;
    if (!opts.output_file.empty()) {
        std::string err;
        int const wrc = open_pcap_writer(opts.output_file, writer, &err);
        if (wrc != 0) {
            (void)fprintf(stderr, "%s\n", err.c_str());
            return wrc;
        }
    }

    if (!opts.input_file.empty()) {
        PcapReader reader;
        std::string err;
        if (!open_pcap(opts.input_file, reader, &err)) {
            if (!opts.output_file.empty()) { close_pcap_writer(writer);
}
            (void)fprintf(stderr, "%s\n", err.c_str());
            return 1;
        }

        uint32_t ts_sec = 0;
        uint32_t ts_usec = 0;
        std::vector<uint8_t> bytes;
        int kept = 0;
        int received = 0;
        while (read_record(reader, ts_sec, ts_usec, bytes)) {
            ++received;
            size_t dlen = bytes.size();
            if (opts.snaplen > 0 && dlen > static_cast<size_t>(opts.snaplen)) {
                dlen = static_cast<size_t>(opts.snaplen);
}
            if (!process_packet(bytes.data(), dlen, ts_sec, ts_usec, &opts, filter, writer)) {
                continue;
}
            ++kept;
            if (opts.count > 0 && kept >= opts.count) { break;
}
        }
        if (reader.truncated) {
            std::string const rerr = "tcpdump: " + opts.input_file + ": truncated packet record";
            close_pcap(reader);
            if (!opts.output_file.empty()) { close_pcap_writer(writer);
}
            (void)fprintf(stderr, "%s\n", rerr.c_str());
            return 1;
        }
        close_pcap(reader);
        if (!opts.output_file.empty()) {
            if ((writer.f != nullptr) && (ferror(writer.f) != 0)) {
                std::string const werr = "tcpdump: write error on " + opts.output_file;
                close_pcap_writer(writer);
                (void)fprintf(stderr, "%s\n", werr.c_str());
                return 1;
            }
            close_pcap_writer(writer);
        }
        print_summary(kept, received, 0);
        return 0;
    }

    // Live capture path.
    std::string lerr;
    int const fd = open_capture_socket(&opts, &lerr);
    if (fd < 0) {
        if (writer.f != nullptr) { close_pcap_writer(writer);
}
        (void)fprintf(stderr, "%s\n", lerr.c_str());
        return 1;
    }
    install_signal_handlers();
    size_t const bufsz = static_cast<size_t>(opts.snaplen) > 64 ? static_cast<size_t>(opts.snaplen) : 64;
    std::vector<uint8_t> buf(bufsz);
    int kept = 0;
    int received = 0;
    while (g_stop == 0) {
        ssize_t const n = recvfrom(fd, buf.data(), buf.size(), 0, nullptr, nullptr);
        if (n < 0) {
            if (errno == EINTR) { continue;
}
            if (writer.f != nullptr) { close_pcap_writer(writer);
}
            std::string const err = "tcpdump: recvfrom: " + std::string(std::strerror(errno));
            close(fd);
            (void)fprintf(stderr, "%s\n", err.c_str());
            return 1;
        }
        ++received;
        uint32_t const ts_sec = 0;
        uint32_t const ts_usec = 0;  // live: not available from recvfrom
        if (!process_packet(buf.data(), static_cast<size_t>(n), ts_sec, ts_usec,
                            &opts, filter, writer)) {
            continue;
}
        ++kept;
        if (opts.count > 0 && kept >= opts.count) { break;
}
    }
    close(fd);
    if (writer.f != nullptr) { close_pcap_writer(writer);
}
    print_summary(kept, received, 0);
    return 0;
}

REGISTER_COMMAND("tcpdump", tcpdump_command, "Capture and display network packets");
