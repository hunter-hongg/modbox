#include <bits/types/struct_timeval.h>
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
#include <poll.h>
#include <csignal>
#include <sys/poll.h>
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

}  // namespace
int ping_command(int argc, char** argv) {
    const char* prog = argv[0];

    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_int* count_opt = arg_int0("c", "count", "<count>", "stop after sending <count> replies");
    struct arg_str* host_arg = arg_str1(nullptr, nullptr, "<destination>", "destination address or hostname");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, count_opt, host_arg, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... <destination>\n", prog);
        printf("Send ICMP ECHO_REQUEST to network hosts.\n");
        printf("\n");
        printf("  -c, --count=<count>   stop after sending <count> ECHO_REQUEST packets\n");
        printf("  -h, --help            display this help and exit\n");
        printf("  -V, --version         output version information and exit\n");
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

    const char* host = host_arg->sval[0];
    long const count = count_opt->count > 0 ? count_opt->ival[0] : -1;  // -1 = unlimited

    // Resolve the destination to an IPv4 address. Accept a literal address via
    // inet_pton; otherwise resolve a hostname (no socktype/protocol hints, as
    // getaddrinfo rejects SOCK_DGRAM+IPPROTO_ICMP together).
    struct sockaddr_in saddr {};
    saddr.sin_family = AF_INET;
    struct addrinfo* res = nullptr;
    if (inet_pton(AF_INET, host, &saddr.sin_addr) != 1) {
        struct addrinfo hints {};
        hints.ai_family = AF_INET;
        int const gai = getaddrinfo(host, nullptr, &hints, &res);
        if (gai != 0) {
            (void)fprintf(stderr, "%s: %s: %s\n", prog, host, gai_strerror(gai));
            return 1;
        }
        saddr = *reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    }

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &saddr.sin_addr, ipstr, sizeof(ipstr));

    // Use the "ping socket" interface (SOCK_DGRAM + IPPROTO_ICMP). Unlike a raw
    // ICMP socket this works for unprivileged users whose group is within
    // net.ipv4.ping_group_range. The kernel fills in/validates the ICMP header,
    // but the caller must still send the full ICMP header (type 8, id, seq)
    // plus the payload — a bare payload send returns EINVAL.
    int const sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
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

    struct sigaction sa {};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    const uint16_t ident = static_cast<uint16_t>(getpid() & 0xffff);
    const int payload_size = 56;  // classic default (8-byte ICMP hdr + 56 = 64)
    const int interval_ms = 1000;

    // Bind the ping socket to the ident as its pseudo-port so the kernel uses
    // it for the ECHO id and replies match (see iputils). Without a bind the
    // kernel picks an unrelated id and replies are filtered out.
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

    size_t transmitted = 0;
    size_t received = 0;
    RttStats rtt;

    printf("PING %s (%s): %d data bytes\n", host, ipstr, payload_size);

    for (long seq = 0; (g_stop == 0) && (count < 0 || seq < count); ++seq) {
        struct timeval t_send;
        gettimeofday(&t_send, nullptr);

        // Build ICMP echo request. The send timestamp rides in the payload so
        // RTT can be computed from the echoed copy regardless of reply order.
        struct icmphdr icmp {};
        icmp.type = ICMP_ECHO;
        icmp.code = 0;
        icmp.un.echo.id = htons(ident);
        icmp.un.echo.sequence = htons(static_cast<uint16_t>(seq));

        std::string packet(8 + payload_size, '\0');
        memcpy(packet.data(), &icmp, sizeof(icmp));
        memcpy(packet.data() + 8, &t_send, sizeof(t_send));

        icmp.checksum = in_cksum(packet.data(), packet.size());
        memcpy(packet.data(), &icmp, sizeof(icmp));

        ssize_t const nsent = sendto(sock, packet.data(), packet.size(), 0,
                               reinterpret_cast<struct sockaddr*>(&saddr), sizeof(saddr));
        if (nsent < 0) {
            (void)fprintf(stderr, "%s: sendto: %s\n", prog, strerror(errno));
            break;
        }
        transmitted++;

        // Wait the interval for a matching reply.
        struct pollfd pfd {.fd=sock, .events=POLLIN, .revents=0};
        int const pr = poll(&pfd, 1, interval_ms);
        if (pr < 0) {
            if (errno == EINTR) { break;  // user hit Ctrl+C
}
            break;
        }
        if (pr == 0 || ((pfd.revents & POLLIN) == 0)) {
            continue;  // timed out: reported as loss in the summary
        }

        char rbuf[65536];
        ssize_t const n = recvfrom(sock, rbuf, sizeof(rbuf), 0, nullptr, nullptr);
        if (n <= 0) { continue;
}

        // Ping sockets deliver the datagram payload: the ICMP header followed
        // by the echo data. Validate id + type, then extract the timestamp.
        if (static_cast<size_t>(n) < sizeof(struct icmphdr)) { continue;
}
        auto* rc = reinterpret_cast<struct icmphdr*>(rbuf);
        if (rc->type != ICMP_ECHOREPLY || ntohs(rc->un.echo.id) != ident) { continue;
}

        struct timeval t_recv;
        gettimeofday(&t_recv, nullptr);

        struct timeval t_sent {};
        if (static_cast<size_t>(n) >= sizeof(struct icmphdr) + sizeof(t_sent)) {
            memcpy(&t_sent, rbuf + sizeof(struct icmphdr), sizeof(t_sent));
        }
        double const ms = (t_recv.tv_sec - t_sent.tv_sec) * 1000.0 +
                    (t_recv.tv_usec - t_sent.tv_usec) / 1000.0;
        double const ms_show = ms < 0 ? 0.0 : ms;
        rtt.add(ms_show);
        received++;

        printf("%ld bytes from %s: icmp_seq=%u time=%.1f ms\n",
               static_cast<long>(n) - static_cast<long>(sizeof(struct icmphdr)),
               ipstr, ntohs(rc->un.echo.sequence), ms_show);
    }

    close(sock);
    freeaddrinfo(res);

    // Summary.
    printf("\n--- %s ping statistics ---\n", host);
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

