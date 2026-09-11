#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <string>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <poll.h>
#include <csignal>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

volatile sig_atomic_t g_stop = 0;

void on_sigint(int) { g_stop = 1; }

// Printable MAC address from a 6-byte hardware address.
std::string mac_to_string(const uint8_t* mac, size_t len) {
    char buf[18] = {0};
    (void)std::snprintf(buf, sizeof(buf),
                  "%02x:%02x:%02x:%02x:%02x:%02x",
                  len > 0 ? mac[0] : 0, len > 1 ? mac[1] : 0,
                  len > 2 ? mac[2] : 0, len > 3 ? mac[3] : 0,
                  len > 4 ? mac[4] : 0, len > 5 ? mac[5] : 0);
    return std::string(buf);
}

int arping_command(int argc, char** argv) {
    const char* prog = argv[0];

    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_str* iface_opt =
        arg_str1("I", "interface", "<interface>", "interface to use (required)");
    struct arg_int* count_opt = arg_int0("c", "count", "<count>", "stop after <count> replies");
    struct arg_str* ip_arg = arg_str1(nullptr, nullptr, "<ip>", "target IPv4 address");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, iface_opt, count_opt, ip_arg, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... <ip>\n", prog);
        printf("Ping a host by ARP request to resolve its MAC address.\n");
        printf("\n");
        printf("  -I, --interface=<interface>  interface to use (required)\n");
        printf("  -c, --count=<count>          stop after <count> replies\n");
        printf("  -h, --help                   display this help and exit\n");
        printf("  -V, --version                output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("arping");
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, prog);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return 2;
    }

    const char* ifname = iface_opt->sval[0];
    const char* ipstr = ip_arg->sval[0];
    long const count = count_opt->count > 0 ? count_opt->ival[0] : -1;  // -1 = unlimited

    // Resolve the target IPv4 address.
    struct in_addr target {};
    if (inet_pton(AF_INET, ipstr, &target) != 1) {
        (void)fprintf(stderr, "%s: invalid IPv4 address: %s\n", prog, ipstr);
        return 1;
    }

    // Resolve the interface index.
    unsigned int const ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        (void)fprintf(stderr, "%s: interface not found: %s\n", prog, ifname);
        return 1;
    }

    // Raw AF_PACKET socket — requires root or CAP_NET_RAW.
    int const sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sock < 0) {
        if (errno == EPERM || errno == EACCES) {
            (void)fprintf(stderr,
                    "%s: raw packet socket: %s (need root or CAP_NET_RAW)\n",
                    prog, strerror(errno));
        } else {
            (void)fprintf(stderr, "%s: socket: %s\n", prog, strerror(errno));
        }
        return 1;
    }

    // Source hardware address and source IPv4 address for this interface.
    uint8_t src_mac[6] = {0};
    struct in_addr src_ip {};
    src_ip.s_addr = htonl(INADDR_ANY);

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
        std::memcpy(src_mac, ifr.ifr_hwaddr.sa_data, 6);
    }
    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        src_ip = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr)->sin_addr;
    }

    struct sigaction sa {};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    // Build the Ethernet + ARP REQUEST frame.
    const uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    std::string frame(14 + 28, '\0');

    uint8_t* f = reinterpret_cast<uint8_t*>(frame.data());
    std::memcpy(f, broadcast, 6);                          // dst MAC
    std::memcpy(f + 6, src_mac, 6);                        // src MAC
    f[12] = 0x08; f[13] = 0x06;                            // ETH_P_ARP

    struct ether_arp* arp = reinterpret_cast<struct ether_arp*>(f + 14);
    arp->arp_hrd = htons(ARPHRD_ETHER);
    arp->arp_pro = htons(ETH_P_IP);
    arp->arp_hln = 6;
    arp->arp_pln = 4;
    arp->arp_op = htons(ARPOP_REQUEST);
    std::memcpy(arp->arp_sha, src_mac, 6);
    std::memcpy(&arp->arp_spa, &src_ip, 4);
    std::memset(arp->arp_tha, 0, 6);
    std::memcpy(&arp->arp_tpa, &target, 4);

    struct sockaddr_ll sll {};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = static_cast<int>(ifindex);
    sll.sll_halen = 6;
    std::memcpy(sll.sll_addr, broadcast, 6);

    const int interval_ms = 1000;
    size_t requests = 0;
    bool resolved = false;
    std::string resolved_mac;

    for (long seq = 0; (g_stop == 0) && (count < 0 || seq < count); ++seq) {
        ssize_t const nsent = sendto(sock, frame.data(), frame.size(), 0,
                               reinterpret_cast<struct sockaddr*>(&sll), sizeof(sll));
        if (nsent < 0) {
            (void)fprintf(stderr, "%s: sendto: %s\n", prog, strerror(errno));
            break;
        }
        requests++;

        // Wait the interval for a matching ARP reply.
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int const pr = poll(&pfd, 1, interval_ms);
        if (pr < 0) {
            if (errno == EINTR)
            {
                break;
            }
            break;
        }
        if (pr == 0 || ((pfd.revents & POLLIN) == 0))
        {
            continue;
        }

        char rbuf[2048];
        ssize_t const n = recvfrom(sock, rbuf, sizeof(rbuf), 0, nullptr, nullptr);
        if (n <= 0)
        {
            continue;
        }
        if (static_cast<size_t>(n) < 14 + sizeof(struct ether_arp))
        {
            continue;
        }

        const uint8_t* rf = reinterpret_cast<uint8_t*>(rbuf);
        uint16_t const ethertype = static_cast<uint16_t>(rf[12]) << 8 | rf[13];
        if (ethertype != ETH_P_ARP)
        {
            continue;
        }

        auto* ra = reinterpret_cast<struct ether_arp*>(rbuf + 14);
        if (ntohs(ra->arp_op) != ARPOP_REPLY)
        {
            continue;
        }

        // Reply must come from the target and be addressed to our source IP.
        struct in_addr rsp;
        std::memcpy(&rsp, &ra->arp_spa, 4);
        struct in_addr rtp;
        std::memcpy(&rtp, &ra->arp_tpa, 4);
        if (rsp.s_addr != target.s_addr)
        {
            continue;
        }
        if (src_ip.s_addr != htonl(INADDR_ANY) && rtp.s_addr != src_ip.s_addr)
        {
            continue;
        }

        resolved = true;
        resolved_mac = mac_to_string(ra->arp_sha, 6);
        printf("ARP REPLY %s: %s\n", ipstr, resolved_mac.c_str());
        break;
    }

    close(sock);

    if (requests == 0)
    {
        return 1;
    }
    if (!resolved) {
        printf("No ARP reply from %s (%zu request%s sent)\n", ipstr, requests,
               requests == 1 ? "" : "s");
        return 1;
    }
    return 0;
}

REGISTER_COMMAND("arping", arping_command, "Ping a host by ARP request to resolve its MAC address");

}