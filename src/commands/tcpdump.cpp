#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <arpa/inet.h>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/tcpdump.hpp"
#include "commands/version_util.hpp"

namespace {

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
            if (err != nullptr)
                *err = "tcpdump: cannot open " + path + ": " + std::strerror(errno);
            return false;
        }
    }

    unsigned char hdr[kGlobalHeaderSize];
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    if (n < sizeof(hdr)) {
        if (f != stdin) fclose(f);
        if (err != nullptr) *err = "tcpdump: " + path + ": not a pcap file";
        return false;
    }

    bool big_endian;
    if (hdr[0] == 0xa1 && hdr[1] == 0xb2 && hdr[2] == 0xc3 && hdr[3] == 0xd4) {
        big_endian = false;
    } else if (hdr[0] == 0xd4 && hdr[1] == 0xc3 && hdr[2] == 0xb2 && hdr[3] == 0xa1) {
        big_endian = true;
    } else {
        if (f != stdin) fclose(f);
        if (err != nullptr) *err = "tcpdump: " + path + ": not a pcap file";
        return false;
    }

    uint32_t version_major = rd16(hdr, 4, big_endian);
    uint32_t linktype = rd32(hdr, 20, big_endian);

    if (version_major != 2) {
        if (f != stdin) fclose(f);
        if (err != nullptr)
            *err = "tcpdump: " + path + ": unsupported version " + std::to_string(version_major);
        return false;
    }
    if (linktype != 1) {
        if (f != stdin) fclose(f);
        if (err != nullptr)
            *err = "tcpdump: " + path + ": unsupported linktype " + std::to_string(linktype);
        return false;
    }

    out.f = f;
    out.big_endian = big_endian;
    out.linktype = linktype;
    return true;
}

bool read_record(PcapReader& r, uint32_t& ts_sec, uint32_t& ts_usec, std::vector<uint8_t>& bytes) {
    unsigned char hdr[kRecordHeaderSize];
    size_t n = fread(hdr, 1, sizeof(hdr), r.f);
    if (n == 0 && feof(r.f)) return false;
    if (n < sizeof(hdr)) {
        r.truncated = true;
        return false;
    }

    uint32_t incl_len = rd32(hdr, 8, r.big_endian);
    ts_sec = rd32(hdr, 0, r.big_endian);
    ts_usec = rd32(hdr, 4, r.big_endian);

    bytes.resize(incl_len);
    size_t got = fread(bytes.data(), 1, incl_len, r.f);
    if (got < incl_len) {
        r.truncated = true;
        return false;
    }
    return true;
}

void close_pcap(PcapReader& r) {
    if (r.f && r.f != stdin) fclose(r.f);
    r.f = nullptr;
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

static void decode_packet(const uint8_t* d, size_t len, uint32_t ts_sec, uint32_t ts_usec, const TcpdumpOptions* opts) {
    if (len < 14) {
        return;
    }
    if (opts->epoch_ts) {
        printf("%u.%06u ", ts_sec, ts_usec);
    } else {
        time_t t = static_cast<time_t>(ts_sec);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        printf("%02d:%02d:%02d.%06u ", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ts_usec);
    }

    char src_mac[18] = "";
    char dst_mac[18] = "";
    if (opts->show_link) {
        snprintf(dst_mac, sizeof(dst_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 d[0], d[1], d[2], d[3], d[4], d[5]);
        snprintf(src_mac, sizeof(src_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 d[6], d[7], d[8], d[9], d[10], d[11]);
        printf("%s > %s, ", src_mac, dst_mac);
    }

    uint16_t ethertype = (static_cast<uint16_t>(d[12]) << 8) | static_cast<uint16_t>(d[13]);

    if (ethertype == 0x0806 && len >= 14 + 28) {
        uint16_t op = (static_cast<uint16_t>(d[14 + 6]) << 8) | static_cast<uint16_t>(d[14 + 7]);
        struct in_addr spa_addr, tpa_addr;
        memcpy(&spa_addr.s_addr, d + 14 + 14, 4);
        memcpy(&tpa_addr.s_addr, d + 14 + 24, 4);
        char spa_str[INET_ADDRSTRLEN];
        char tpa_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &spa_addr, spa_str, sizeof(spa_str));
        inet_ntop(AF_INET, &tpa_addr, tpa_str, sizeof(tpa_str));
        if (op == 1) {
            printf("ARP, Request, who has %s tell %s, length %zu\n", tpa_str, spa_str, len);
        } else if (op == 2) {
            char sha_mac[18];
            snprintf(sha_mac, sizeof(sha_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     d[14 + 8], d[14 + 9], d[14 + 10], d[14 + 11], d[14 + 12], d[14 + 13]);
            printf("ARP, Reply, %s is-at %s, length %zu\n", spa_str, sha_mac, len);
        } else {
            printf("ARP, Unknown op %u, length %zu\n", op, len);
        }
    } else if (ethertype == 0x0800 && len >= 14 + 20) {
        uint8_t ver_ihl = d[14];
        uint8_t ihl = (ver_ihl & 0x0F) * 4;
        if (len < 14 + ihl) {
            printf("EtherType 0x%04x, length %zu\n", ethertype, len);
            return;
        }
        uint8_t proto = d[14 + 9];
        struct in_addr src_addr, dst_addr;
        memcpy(&src_addr.s_addr, d + 14 + 12, 4);
        memcpy(&dst_addr.s_addr, d + 14 + 16, 4);
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_addr, src_ip, sizeof(src_ip));
        inet_ntop(AF_INET, &dst_addr, dst_ip, sizeof(dst_ip));
        size_t payload_len = (len > 14 + ihl) ? len - 14 - ihl : 0;
        printf("%s > %s: IP, proto %u, length %zu\n", src_ip, dst_ip, proto, payload_len);
    } else if (ethertype == 0x86DD && len >= 14 + 40) {
        uint8_t ver = d[14] >> 4;
        if (ver != 6) {
            printf("EtherType 0x%04x, length %zu\n", ethertype, len);
            return;
        }
        uint16_t payload_len_field = (static_cast<uint16_t>(d[14 + 4]) << 8) | static_cast<uint16_t>(d[14 + 5]);
        uint8_t next = d[14 + 6];
        uint8_t hop = d[14 + 7];
        struct in6_addr src6, dst6;
        memcpy(&src6.s6_addr, d + 14 + 8, 16);
        memcpy(&dst6.s6_addr, d + 14 + 24, 16);
        char src_str[INET6_ADDRSTRLEN];
        char dst_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &src6, src_str, sizeof(src_str));
        inet_ntop(AF_INET6, &dst6, dst_str, sizeof(dst_str));
        size_t ipv6_hdr_len = 40;
        size_t rest_len = (len > 14 + ipv6_hdr_len) ? len - 14 - ipv6_hdr_len : 0;
        if (next == 6) { // TCP
            size_t tcp_off = 14 + 40;
            if (len < tcp_off + 20) {
                printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                if (opts->verbose) printf(", hop limit %u", hop);
                printf("\n");
                return;
            }
            uint16_t sport = (static_cast<uint16_t>(d[tcp_off]) << 8) | static_cast<uint16_t>(d[tcp_off + 1]);
            uint16_t dport = (static_cast<uint16_t>(d[tcp_off + 2]) << 8) | static_cast<uint16_t>(d[tcp_off + 3]);
            uint32_t seq = (static_cast<uint32_t>(d[tcp_off + 4]) << 24) | (static_cast<uint32_t>(d[tcp_off + 5]) << 16) |
                           (static_cast<uint32_t>(d[tcp_off + 6]) << 8) | static_cast<uint32_t>(d[tcp_off + 7]);
            uint16_t window = (static_cast<uint16_t>(d[tcp_off + 14]) << 8) | static_cast<uint16_t>(d[tcp_off + 15]);
            uint8_t data_offset = d[tcp_off + 12] >> 4;
            uint8_t flags = d[tcp_off + 13];
            size_t tcp_hdr_len = static_cast<size_t>(data_offset) * 4;
            if (tcp_hdr_len < 20) tcp_hdr_len = 20;
            size_t tcp_payload = 0;
            if (len > tcp_off + tcp_hdr_len) tcp_payload = len - tcp_off - tcp_hdr_len;
            char flag_buf[16] = "";
            int pos = 0;
            if (flags & 0x01) flag_buf[pos++] = 'F';
            if (flags & 0x02) flag_buf[pos++] = 'S';
            if (flags & 0x04) flag_buf[pos++] = 'R';
            if (flags & 0x08) flag_buf[pos++] = 'P';
            if (flags & 0x10) flag_buf[pos++] = 'A';
            if (flags & 0x20) flag_buf[pos++] = 'U';
            flag_buf[pos] = '\0';
            printf("%s.%u > %s.%u: Flags [%s], seq %u, win %u, length %zu", src_str, sport, dst_str, dport, flag_buf, seq, window, tcp_payload);
            if (opts->verbose) printf(", hop limit %u", hop);
            printf("\n");
        } else if (next == 17) { // UDP
            size_t udp_off = 14 + 40;
            if (len < udp_off + 8) {
                printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                if (opts->verbose) printf(", hop limit %u", hop);
                printf("\n");
                return;
            }
            uint16_t sport = (static_cast<uint16_t>(d[udp_off]) << 8) | static_cast<uint16_t>(d[udp_off + 1]);
            uint16_t dport = (static_cast<uint16_t>(d[udp_off + 2]) << 8) | static_cast<uint16_t>(d[udp_off + 3]);
            uint16_t udp_len_field = (static_cast<uint16_t>(d[udp_off + 4]) << 8) | static_cast<uint16_t>(d[udp_off + 5]);
            size_t udp_payload = 0;
            if (udp_len_field > 8) {
                udp_payload = static_cast<size_t>(udp_len_field) - 8;
                size_t available = (len > udp_off + 8) ? len - udp_off - 8 : 0;
                if (udp_payload > available) udp_payload = available;
            }
            printf("%s.%u > %s.%u: UDP, length %zu", src_str, sport, dst_str, dport, udp_payload);
            if (opts->verbose) printf(", hop limit %u", hop);
            printf("\n");
        } else if (next == 58) { // ICMPv6
            size_t icmp_off = 14 + 40;
            if (len < icmp_off + 4) {
                printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                if (opts->verbose) printf(", hop limit %u", hop);
                printf("\n");
                return;
            }
            uint8_t type = d[icmp_off];
            uint8_t code = d[icmp_off + 1];
            size_t icmp_len = rest_len;
            if (type == 128) {
                if (len < icmp_off + 8) {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                } else {
                    uint16_t id = (static_cast<uint16_t>(d[icmp_off + 4]) << 8) | static_cast<uint16_t>(d[icmp_off + 5]);
                    uint16_t seqv = (static_cast<uint16_t>(d[icmp_off + 6]) << 8) | static_cast<uint16_t>(d[icmp_off + 7]);
                    printf("%s > %s: ICMP6, echo request, id %u, seq %u", src_str, dst_str, id, seqv);
                }
            } else if (type == 129) {
                if (len < icmp_off + 8) {
                    printf("%s > %s: IP6, next %u, length %zu\n", src_str, dst_str, next, rest_len);
                } else {
                    uint16_t id = (static_cast<uint16_t>(d[icmp_off + 4]) << 8) | static_cast<uint16_t>(d[icmp_off + 5]);
                    uint16_t seqv = (static_cast<uint16_t>(d[icmp_off + 6]) << 8) | static_cast<uint16_t>(d[icmp_off + 7]);
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
            if (opts->verbose) printf(", hop limit %u", hop);
            printf("\n");
        } else {
            printf("%s > %s: IP6, next %u, length %zu", src_str, dst_str, next, rest_len);
            if (opts->verbose) printf(", hop limit %u", hop);
            printf("\n");
        }
    } else {
        printf("EtherType 0x%04x, length %zu\n", ethertype, len);
    }
}

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [EXPR]\n", prog);
    printf("Capture and display network packets.\n");
    printf("\n");
    printf("  -r <file>       read packets from <file> (pcap format)\n");
    printf("  -w <file>       write raw packets to <file>\n");
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
            static char buf[] = "--tt\0";
            argv[i] = buf;
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
    int nerrors = at.parse(argc, argv);

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
        fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return 2;
    }

    TcpdumpOptions opts;
    if (input_opt->count > 0) opts.input_file = input_opt->sval[0];
    if (output_opt->count > 0) opts.output_file = output_opt->sval[0];
    if (filter_opt->count > 0) opts.filter_expr = filter_opt->sval[0];
    if (iface_opt->count > 0) opts.interface = iface_opt->sval[0];
    if (count_opt->count > 0) opts.count = count_opt->ival[0];
    if (snaplen_opt->count > 0) opts.snaplen = snaplen_opt->ival[0];
    opts.show_link = (link_opt->count > 0);
    opts.brief = (brief_opt->count > 0);
    opts.verbose = (verbose_opt->count > 0);
    opts.hex_dump = (hex_opt->count > 0);
    opts.numeric = (numeric_opt->count > 0 || numeric2_opt->count > 0);
    opts.epoch_ts = (epoch_opt->count > 0);

    if (!opts.input_file.empty()) {
        PcapReader reader;
        std::string err;
        if (!open_pcap(opts.input_file, reader, &err)) {
            fprintf(stderr, "%s\n", err.c_str());
            return 1;
        }

        uint32_t ts_sec = 0, ts_usec = 0;
        std::vector<uint8_t> bytes;
        while (read_record(reader, ts_sec, ts_usec, bytes)) {
            if (bytes.size() < 14) continue;  // Skip short records silently
            if (bytes.size() >= 14) {
                uint16_t ethertype = (static_cast<uint16_t>(bytes[12]) << 8) | bytes[13];
                if (ethertype == 0x86dd && bytes.size() < 54) continue;  // Skip short IPv6 silently
            }
            decode_packet(bytes.data(), bytes.size(), ts_sec, ts_usec, &opts);
        }
        if (reader.truncated) {
            std::string rerr = "tcpdump: " + opts.input_file + ": truncated packet record";
            close_pcap(reader);
            fprintf(stderr, "%s\n", rerr.c_str());
            return 1;
        }
        close_pcap(reader);
        return 0;
    }

    fprintf(stderr, "%s: capture not yet implemented\n", prog);
    return 1;
}

REGISTER_COMMAND("tcpdump", tcpdump_command, "Capture and display network packets");
