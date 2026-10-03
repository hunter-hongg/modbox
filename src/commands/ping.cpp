#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <poll.h>
#include <csignal>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/ping.hpp"
#include "commands/version_util.hpp"

namespace {

volatile sig_atomic_t g_stop = 0;

void on_sigint(int) { g_stop = 1; }

// Internet checksum (RFC 1071), used for the ICMP header + payload.
uint16_t in_cksum(const void* data, size_t len) {
    const uint16_t* buf = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) {
        sum += *static_cast<const uint8_t*>(data);
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

// Statistics accumulator for RTT figures.
struct RttStats {
    double min_ = 0;
    double max_ = 0;
    double sum_ = 0;
    double sumsq_ = 0;
    size_t n_ = 0;

    void add(double ms) {
        if (n_ == 0 || ms < min_) { min_ = ms;
}
        if (n_ == 0 || ms > max_) { max_ = ms;
}
        sum_ += ms;
        sumsq_ += ms * ms;
        n_++;
    }

    [[nodiscard]] double mean() const { return (n_ != 0u) ? sum_ / n_ : 0; }
    // Standard deviation (population) of the samples.
    [[nodiscard]] double mdev() const {
        if (n_ == 0) { return 0;
}
        double const m = mean();
        double const v = sumsq_ / n_ - m * m;
        return v > 0 ? std::sqrt(v) : 0;
    }
};

// Parsed options for the ping command.
struct PingOptions {
    int family = 0;          // 0 = auto, AF_INET, or AF_INET6
    long count = -1;         // -1 = unlimited
    double interval = 1.0;   // seconds between probes (-i)
    int size = 56;           // payload bytes (-s)
    double timeout = 10.0;   // per-probe wait, seconds (-W)
    bool quiet = false;      // -q
    bool verbose = false;    // -v
};

double now_ms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Parse a non-negative numeric option argument in [-i/-W] range. Rejects
// trailing garbage and negatives (both are usage errors, exit 2 per the
// spec). Returns false on any invalid input.
bool parse_seconds(const char* text, const char* flag, const char* prog,
                   double* out) {
    char* endp = nullptr;
    errno = 0;
    double const v = std::strtod(text, &endp);
    if (endp == text || (endp != nullptr && *endp != '\0') || errno == EINVAL ||
        std::isnan(v) || std::isinf(v)) {
        (void)fprintf(stderr, "%s: invalid argument for -%s: '%s'\n", prog, flag, text);
        return false;
    }
    if (v < 0) {
        (void)fprintf(stderr, "%s: invalid argument for -%s: '%s'\n", prog, flag, text);
        return false;
    }
    *out = v;
    return true;
}

}  // namespace

int ping_command(int argc, char** argv) {
    const char* prog = argv[0];

    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* ipv4_opt = arg_lit0("4", "ipv4", "force IPv4 (ICMP echo)");
    struct arg_lit* ipv6_opt = arg_lit0("6", "ipv6", "force IPv6 (ICMPv6 echo)");
    struct arg_lit* quiet_opt = arg_lit0("q", "quiet", "summary only, no per-packet lines");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "verbose output");
    struct arg_int* count_opt = arg_int0("c", "count", "<count>", "stop after sending <count> packets");
    struct arg_str* interval_opt = arg_str0("i", "interval", "<seconds>", "wait <seconds> between probes (default 1)");
    struct arg_str* size_opt = arg_str0("s", "size", "<bytes>", "payload size in bytes (default 56)");
    struct arg_str* timeout_opt = arg_str0("W", "timeout", "<seconds>", "per-probe wait timeout (default 10)");
    struct arg_str* host_arg = arg_str1(nullptr, nullptr, "<destination>", "destination address or hostname");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, ipv4_opt, ipv6_opt, quiet_opt, verbose_opt,
                 count_opt, interval_opt, size_opt, timeout_opt, host_arg, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... <destination>\n", prog);
        printf("Send ICMP ECHO_REQUEST to network hosts.\n");
        printf("\n");
        printf("  -c, --count=<count>     stop after sending <count> packets\n");
        printf("  -i, --interval=<secs>   wait <secs> between probes (default 1)\n");
        printf("  -s, --size=<bytes>      payload size in bytes (default 56)\n");
        printf("  -W, --timeout=<secs>    per-probe wait timeout (default 10)\n");
        printf("  -4, --ipv4              force IPv4 (ICMP echo)\n");
        printf("  -6, --ipv6              force IPv6 (ICMPv6 echo)\n");
        printf("  -q, --quiet             print only the summary\n");
        printf("  -v, --verbose           verbose output\n");
        printf("  -h, --help              display this help and exit\n");
        printf("  -V, --version           output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("ping");
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, prog);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return 2;
    }

    PingOptions opts;
    if (ipv4_opt->count > 0 && ipv6_opt->count > 0) {
        (void)fprintf(stderr, "%s: only one of -4 and -6 may be specified\n", prog);
        return 2;
    }
    if (ipv4_opt->count > 0) { opts.family = AF_INET;
}
    if (ipv6_opt->count > 0) { opts.family = AF_INET6; }
    opts.quiet = quiet_opt->count > 0;
    opts.verbose = verbose_opt->count > 0;
    if (count_opt->count > 0) { opts.count = count_opt->ival[0]; }

    if (interval_opt->count > 0 &&
        !parse_seconds(interval_opt->sval[0], "i", prog, &opts.interval)) {
        return 2;
    }
    if (timeout_opt->count > 0 &&
        !parse_seconds(timeout_opt->sval[0], "W", prog, &opts.timeout)) {
        return 2;
    }
    if (size_opt->count > 0) {
        const char* text = size_opt->sval[0];
        char* endp = nullptr;
        errno = 0;
        long long const v = std::strtoll(text, &endp, 0);
        if (endp == text || (endp != nullptr && *endp != '\0') || errno == ERANGE ||
            v < 1 || v > 65507) {
            (void)fprintf(stderr, "%s: invalid -s value: '%s': out of range: 1 <= value <= 65507\n",
                          prog, text);
            return 2;
        }
        opts.size = static_cast<int>(v);
    }

    const char* host = host_arg->sval[0];

    // Resolve the destination. With an explicit family the resolver is told
    // to return only that family, so a v4 literal under -6 (or vice versa)
    // reports an address-family mismatch instead of silently working.
    int family = opts.family;
    if (family == 0) {
        struct in_addr a4;
        struct in6_addr a6;
        if (inet_pton(AF_INET, host, &a4) == 1) {
            family = AF_INET;
        } else if (inet_pton(AF_INET6, host, &a6) == 1) {
            family = AF_INET6;
        }
    }

    struct addrinfo hints {};
    hints.ai_family = family;  // AF_UNSPEC when auto
    struct addrinfo* res = nullptr;
    int gai = getaddrinfo(host, nullptr, &hints, &res);
    if (gai != 0 && family == 0 && opts.family == 0) {
        // Retry without restricting by family; inet_pton above failed but a
        // hostname may still resolve.
        struct addrinfo hints2 {};
        hints2.ai_family = AF_UNSPEC;
        gai = getaddrinfo(host, nullptr, &hints2, &res);
    }
    if (gai != 0) {
        // A family mismatch gets the reference's message; anything else the
        // resolver's.
        if (opts.family != 0 && (gai == EAI_ADDRFAMILY || gai == EAI_NONAME ||
                                 gai == EAI_BADFLAGS || gai == EAI_FAMILY)) {
            // Re-resolve with the other family: if it succeeds, this is a
            // genuine mismatch, not an unknown host.
            struct addrinfo hintsOther {};
            hintsOther.ai_family = (opts.family == AF_INET) ? AF_INET6 : AF_INET;
            struct addrinfo* resOther = nullptr;
            if (getaddrinfo(host, nullptr, &hintsOther, &resOther) == 0) {
                freeaddrinfo(resOther);
                (void)fprintf(stderr, "%s: %s: Address family for hostname not supported\n",
                              prog, host);
                return 2;
            }
        }
        (void)fprintf(stderr, "%s: %s: %s\n", prog, host, gai_strerror(gai));
        return 1;
    }
    family = res->ai_family;
    char ipstr[INET6_ADDRSTRLEN] = {0};
    if (family == AF_INET) {
        auto* s4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        inet_ntop(AF_INET, &s4->sin_addr, ipstr, sizeof(ipstr));
    } else {
        auto* s6 = reinterpret_cast<struct sockaddr_in6*>(res->ai_addr);
        inet_ntop(AF_INET6, &s6->sin6_addr, ipstr, sizeof(ipstr));
    }

    // Open the echo socket. Prefer a raw ICMP/ICMPv6 socket (full control,
    // needed for IPv6 on pre-4.17 kernels); fall back to the unprivileged
    // ping-socket interface (SOCK_DGRAM) when raw is not permitted. The ping
    // socket delivers the ICMP header + payload and validates/computes the
    // checksum, and works for IPv6 too on kernels >= 4.17.
    int const proto = (family == AF_INET) ? static_cast<int>(IPPROTO_ICMP)
                                          : static_cast<int>(IPPROTO_ICMPV6);
    bool raw = false;
    int sock = socket(family, SOCK_RAW, proto);
    if (sock >= 0) {
        raw = true;
    } else {
        sock = socket(family, SOCK_DGRAM, proto);
        if (sock < 0) {
            if (errno == EPERM || errno == EACCES) {
                (void)fprintf(stderr,
                        "%s: ping socket: %s (need root, CAP_NET_RAW, or a GID in "
                        "net.ipv4.ping_group_range)\n",
                        prog, strerror(errno));
            } else {
                (void)fprintf(stderr, "%s: socket: %s\n", prog, strerror(errno));
            }
            freeaddrinfo(res);
            return 1;
        }
    }

    // Request the incoming hop limit so replies can print ttl= (works on
    // both raw and ping sockets; on ping sockets the kernel reports the TTL
    // of the reply it delivered).
    int const on = 1;
    if (family == AF_INET) {
        setsockopt(sock, IPPROTO_IP, IP_RECVTTL, &on, sizeof(on));
    } else {
        setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof(on));
    }

    struct sigaction sa {};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    const uint16_t ident = static_cast<uint16_t>(getpid() & 0xffff);

    // On the ping socket the kernel uses the bound port as the echo id and
    // filters replies by it (see iputils). Raw sockets need no bind.
    if (!raw) {
        if (family == AF_INET) {
            struct sockaddr_in src {};
            src.sin_family = AF_INET;
            src.sin_port = htons(ident);
            src.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(sock, reinterpret_cast<struct sockaddr*>(&src), sizeof(src)) < 0) {
                (void)fprintf(stderr, "%s: bind: %s\n", prog, strerror(errno));
                close(sock);
                freeaddrinfo(res);
                return 1;
            }
        } else {
            struct sockaddr_in6 src {};
            src.sin6_family = AF_INET6;
            src.sin6_port = htons(ident);
            if (bind(sock, reinterpret_cast<struct sockaddr*>(&src), sizeof(src)) < 0) {
                (void)fprintf(stderr, "%s: bind: %s\n", prog, strerror(errno));
                close(sock);
                freeaddrinfo(res);
                return 1;
            }
        }
    }

    const int icmp_hdr = 8;  // both ICMP v4 and ICMPv6 echo headers are 8 bytes
    const int packet_size = icmp_hdr + opts.size;

    // Header line (printed even under -q, matching the reference). v4 uses the
    // classic "SIZE(SIZE+28)" form (20-byte IP + 8-byte ICMP); v6 uses the
    // iputils form.
    if (family == AF_INET) {
        printf("PING %s (%s) %d(%d) bytes of data.\n", host, ipstr, opts.size,
               packet_size + 20);
    } else {
        printf("PING %s (%s) %d data bytes\n", host, ipstr, opts.size);
    }
    if (opts.verbose) {
        printf("%s: %s: %s socket, family %s\n", prog, host,
               raw ? "raw" : "ping", family == AF_INET ? "AF_INET" : "AF_INET6");
    }

    size_t transmitted = 0;
    size_t received = 0;
    RttStats rtt;
    double const start_ms = now_ms();

    // Outstanding probe deadlines (send time + -W) for replies that may still
    // arrive after their interval slot has passed.
    struct Outstanding { uint16_t seq; double deadline_ms; };
    std::vector<struct Outstanding> outstanding;

    double next_send_ms = start_ms;
    long seq = 0;
    while (g_stop == 0 && (opts.count < 0 || seq < opts.count)) {
        struct timeval t_send;
        gettimeofday(&t_send, nullptr);

        // Build the echo request: 8-byte header + payload carrying the send
        // timestamp so RTT is order-independent.
        std::string packet(static_cast<size_t>(packet_size), '\0');
        uint8_t* p = reinterpret_cast<uint8_t*>(packet.data());
        if (family == AF_INET) {
            auto* icmp = reinterpret_cast<struct icmphdr*>(p);
            icmp->type = ICMP_ECHO;
            icmp->code = 0;
            icmp->un.echo.id = htons(ident);
            icmp->un.echo.sequence = htons(static_cast<uint16_t>(seq + 1));
        } else {
            auto* icmp6 = reinterpret_cast<struct icmp6_hdr*>(p);
            icmp6->icmp6_type = ICMP6_ECHO_REQUEST;  // 128
            icmp6->icmp6_code = 0;
            icmp6->icmp6_id = htons(ident);
            icmp6->icmp6_seq = htons(static_cast<uint16_t>(seq + 1));
            // The ICMPv6 checksum is left zero on both socket types: the
            // kernel computes it over the IPv6 pseudo-header for raw ICMPv6
            // sockets (rawv6/icmp6 push path) and for ping sockets alike,
            // using the actually-selected source address.
        }
        memcpy(p + icmp_hdr, &t_send, sizeof(t_send) <= static_cast<size_t>(opts.size) ? sizeof(t_send) : static_cast<size_t>(opts.size));
        if (raw && family == AF_INET) {
            auto* icmp = reinterpret_cast<struct icmphdr*>(p);
            icmp->checksum = 0;
            icmp->checksum = in_cksum(packet.data(), packet.size());
        }

        ssize_t nsent = -1;
        if (family == AF_INET) {
            auto* dst4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
            nsent = sendto(sock, packet.data(), packet.size(), 0,
                           reinterpret_cast<struct sockaddr*>(dst4), sizeof(*dst4));
        } else {
            auto* dst6 = reinterpret_cast<struct sockaddr_in6*>(res->ai_addr);
            nsent = sendto(sock, packet.data(), packet.size(), 0,
                           reinterpret_cast<struct sockaddr*>(dst6), sizeof(*dst6));
        }
        if (nsent < 0) {
            (void)fprintf(stderr, "%s: sendto: %s\n", prog, strerror(errno));
            break;
        }
        transmitted++;
        outstanding.push_back({static_cast<uint16_t>(seq + 1), now_ms() + opts.timeout * 1000.0});
        seq++;

        // Sleep until the next send slot, draining replies as they arrive.
        // The per-probe timeout (-W) bounds how long the very last outstanding
        // probe is waited for once all sends are done.
        next_send_ms += opts.interval * 1000.0;
        bool more_sends = (g_stop == 0) && (opts.count < 0 || seq < opts.count);

        // Drain until the earlier of the next send slot or (when no sends
        // remain) the newest outstanding deadline.
        double drain_until;
        if (more_sends) {
            drain_until = next_send_ms;
        } else if (!outstanding.empty()) {
            drain_until = outstanding.back().deadline_ms;
        } else {
            break;
        }

        while (true) {
            double const now = now_ms();
            int wait_ms = static_cast<int>(drain_until - now);
            if (wait_ms < 0) { wait_ms = 0; }
            struct pollfd pfd {.fd=sock, .events=POLLIN, .revents=0};
            int const pr = poll(&pfd, 1, wait_ms);
            if (pr < 0) {
                if (errno == EINTR) { g_stop = 1;
}
                break;
            }
            if (pr == 0) {
                break;  // drain window elapsed
            }

            unsigned char rbuf[65536];
            unsigned char cbuf[CMSG_SPACE(sizeof(int))];
            struct iovec iov {
                rbuf, sizeof(rbuf)
            };
            struct msghdr msg {};
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_control = cbuf;
            msg.msg_controllen = sizeof(cbuf);
            ssize_t const n = recvmsg(sock, &msg, 0);
            if (n <= 0) { continue;
}

            int hoplimit = -1;
            for (struct cmsghdr* cm = CMSG_FIRSTHDR(&msg); cm != nullptr;
                 cm = CMSG_NXTHDR(&msg, cm)) {
                if (family == AF_INET && cm->cmsg_level == IPPROTO_IP &&
                    cm->cmsg_type == IP_TTL) {
                    memcpy(&hoplimit, CMSG_DATA(cm), sizeof(int));
                } else if (family == AF_INET6 && cm->cmsg_level == IPPROTO_IPV6 &&
                           cm->cmsg_type == IPV6_HOPLIMIT) {
                    memcpy(&hoplimit, CMSG_DATA(cm), sizeof(int));
                }
            }

            // Raw IPv4 sockets deliver the IP header ahead of the ICMP header;
            // strip it so both socket types share one parse path.
            size_t off = 0;
            if (raw && family == AF_INET) {
                if (static_cast<size_t>(n) < 20) { continue;
}
                auto* ip = reinterpret_cast<struct iphdr*>(rbuf);
                size_t const ihl = static_cast<size_t>(ip->ihl) * 4;
                if (static_cast<size_t>(n) < ihl + 8) { continue;
}
                if (hoplimit < 0) { hoplimit = ip->ttl;
}
                off = ihl;
            }
            if (static_cast<size_t>(n) < off + 8) { continue;
}

            uint8_t const type = rbuf[off];
            uint16_t const rid = static_cast<uint16_t>((rbuf[off + 4] << 8) | rbuf[off + 5]);
            uint16_t const rseq = static_cast<uint16_t>((rbuf[off + 6] << 8) | rbuf[off + 7]);
            uint8_t const reply_type = (family == AF_INET) ? ICMP_ECHOREPLY : 129;
            if (type != reply_type || rid != ident) { continue;
}

            struct timeval t_recv;
            gettimeofday(&t_recv, nullptr);
            struct timeval t_sent {};
            size_t const ts_bytes = std::min<size_t>(sizeof(t_sent),
                                                     static_cast<size_t>(opts.size));
            if (static_cast<size_t>(n) >= off + 8 + ts_bytes) {
                memcpy(&t_sent, rbuf + off + 8, ts_bytes);
            }
            double const ms = (t_recv.tv_sec - t_sent.tv_sec) * 1000.0 +
                        (t_recv.tv_usec - t_sent.tv_usec) / 1000.0;
            double const ms_show = ms < 0 ? 0.0 : ms;
            rtt.add(ms_show);
            received++;

            if (!opts.quiet) {
                if (hoplimit >= 0) {
                    printf("%zd bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
                           n - static_cast<ssize_t>(off), std::string(ipstr).c_str(),
                           rseq, hoplimit, ms_show);
                } else {
                    printf("%zd bytes from %s: icmp_seq=%u time=%.3f ms\n",
                           n - static_cast<ssize_t>(off), std::string(ipstr).c_str(),
                           rseq, ms_show);
                }
            }
            // Drop the matching outstanding entry.
            for (size_t i = 0; i < outstanding.size(); i++) {
                if (outstanding[i].seq == rseq) {
                    outstanding.erase(outstanding.begin() + static_cast<long>(i));
                    break;
                }
            }
            if (outstanding.empty() && !more_sends) {
                break;  // everything answered and no sends remain
            }
        }
    }

    close(sock);
    freeaddrinfo(res);

    // Summary (suppress nothing: even -q prints it).
    if (!opts.quiet) { printf("\n"); }
    printf("--- %s ping statistics ---\n", host);
    size_t const loss = (transmitted != 0u) ? (transmitted - received) * 100 / transmitted : 0;
    printf("%zu packets transmitted, %zu packets received, %zu%% packet loss\n",
           transmitted, received, loss);
    if (rtt.n_ > 0) {
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", rtt.min_,
               rtt.mean(), rtt.max_, rtt.mdev());
    }

    return received > 0 ? 0 : 1;
}

REGISTER_COMMAND("ping", ping_command, "Send ICMP ECHO_REQUEST to network hosts");
