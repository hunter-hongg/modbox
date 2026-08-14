#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <map>

#include "commands/ip.hpp"
#include "commands/command_macros.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return "";
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Interface data from getifaddrs + sysfs
// ---------------------------------------------------------------------------

struct InterfaceAddr {
    std::string ip;
    int prefix;
    std::string family; // "inet" or "inet6"
    std::string scope;
};

struct InterfaceInfo {
    std::string name;
    int index;
    std::string mac;
    int mtu;
    std::string operstate;
    std::string flags_str;
    unsigned short flags;
    std::vector<InterfaceAddr> addrs;
    long long rx_bytes = 0, tx_bytes = 0;
    long long rx_packets = 0, tx_packets = 0;
    long long rx_errors = 0, tx_errors = 0;
};

static std::string operstate_to_string(const std::string& s) {
    if (s == "up") return "UP";
    if (s == "down") return "DOWN";
    if (s == "unknown") return "UNKNOWN";
    return s;
}

static std::string flags_to_string(unsigned short flags) {
    std::string out;
    auto add = [&](const char* s, int bit) {
        if (flags & (1 << bit)) {
            if (!out.empty()) out += ",";
            out += s;
        }
    };
    add("LOOPBACK", 0);
    add("BROADCAST", 1);
    add("DEBUG", 2);
    add("RUNNING", 3);
    add("MULTICAST", 4);
    add("LOWER_UP", 15);
    add("MASTER", 16);
    add("SLAVE", 17);
    return out;
}

static std::vector<InterfaceInfo> enumerate_interfaces(bool show_stats, const std::string& filter_iface) {
    // Collect all interface data keyed by name
    std::map<std::string, InterfaceInfo> by_name;
    
    struct ifaddrs *addrs = nullptr;
    if (getifaddrs(&addrs) == 0) {
        for (struct ifaddrs *ptr = addrs; ptr; ptr = ptr->ifa_next) {
            if (!ptr->ifa_addr) continue;
            
            auto& info = by_name[ptr->ifa_name];
            if (info.index == 0) {
                info.name = ptr->ifa_name;
                info.flags = ptr->ifa_flags;
                info.index = if_nametoindex(ptr->ifa_name);
            }
            
            InterfaceAddr ia;
            if (ptr->ifa_addr->sa_family == AF_INET) {
                char buf[64];
                inet_ntop(AF_INET, &((struct sockaddr_in*)ptr->ifa_addr)->sin_addr, buf, sizeof(buf));
                ia.ip = buf;
                ia.family = "inet";
                ia.prefix = 32;
            } else if (ptr->ifa_addr->sa_family == AF_INET6) {
                char buf[64];
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)ptr->ifa_addr)->sin6_addr, buf, sizeof(buf));
                ia.ip = buf;
                ia.family = "inet6";
                ia.prefix = 128;
            } else {
                continue;
            }
            info.addrs.push_back(ia);
        }
        freeifaddrs(addrs);
    }
    
    // Enrich each interface
    for (auto& p : by_name) {
        InterfaceInfo& info = p.second;
        
        // Get netmask via ioctl for first inet addr
        struct ifreq ir;
        memset(&ir, 0, sizeof(ir));
        strncpy(ir.ifr_name, info.name.c_str(), sizeof(ir.ifr_name) - 1);
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s >= 0) {
            if (ioctl(s, SIOCGIFADDR, &ir) == 0) {
                for (auto& addr : info.addrs) {
                    if (addr.family == "inet") {
                        struct sockaddr_in* sa = (struct sockaddr_in*)&ir.ifr_addr;
                        char buf[64];
                        inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
                        if (addr.ip == buf) {
                            if (ioctl(s, SIOCGIFNETMASK, &ir) == 0) {
                                struct sockaddr_in* mask_sa = (struct sockaddr_in*)&ir.ifr_addr;
                                uint32_t mask = __builtin_bswap32(mask_sa->sin_addr.s_addr);
                                addr.prefix = 0;
                                for (uint32_t bit = 0x80000000; bit && mask; bit >>= 1) {
                                    if (mask & bit) addr.prefix++;
                                    else break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            close(s);
        }
        
        // Set ipv6 prefixes based on address type
        for (auto& addr : info.addrs) {
            if (addr.family == "inet6") {
                if (addr.ip == "::1" || addr.ip.find("0000:0000:0000:0000:0000:0000:0000:0001") == 0 ||
                    addr.ip.find("0:0:0:0:0:0:0:1") == 0) {
                    addr.prefix = 128;
                } else if (addr.ip.find("fe80:") == 0) {
                    addr.prefix = 64;
                } else {
                    addr.prefix = 64;
                }
            }
        }
        
        // Enrich with sysfs data
        char path[512];
        snprintf(path, sizeof(path), "/sys/class/net/%s/address", info.name.c_str());
        std::string mac = read_file(path);
        if (!mac.empty() && mac.back() == '\n') mac.pop_back();
        info.mac = mac;
        
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", info.name.c_str());
        std::string state = read_file(path);
        if (!state.empty() && state.back() == '\n') state.pop_back();
        info.operstate = operstate_to_string(state);
        
        snprintf(path, sizeof(path), "/sys/class/net/%s/mtu", info.name.c_str());
        std::string mtu_s = read_file(path);
        if (!mtu_s.empty()) {
            info.mtu = atoi(mtu_s.c_str());
        }
        
        info.flags_str = flags_to_string(info.flags);
        
        if (show_stats) {
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", info.name.c_str());
            info.rx_bytes = atoll(read_file(path).c_str());
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", info.name.c_str());
            info.tx_bytes = atoll(read_file(path).c_str());
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_packets", info.name.c_str());
            info.rx_packets = atoll(read_file(path).c_str());
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_packets", info.name.c_str());
            info.tx_packets = atoll(read_file(path).c_str());
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_errors", info.name.c_str());
            info.rx_errors = atoll(read_file(path).c_str());
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_errors", info.name.c_str());
            info.tx_errors = atoll(read_file(path).c_str());
        }
    }
    
    // Sort by index
    std::vector<InterfaceInfo> ifaces;
    for (auto& p : by_name) ifaces.push_back(p.second);
    std::sort(ifaces.begin(), ifaces.end(),
              [](const InterfaceInfo& a, const InterfaceInfo& b) {
                  return a.index < b.index;
              });
    
    if (!filter_iface.empty()) {
        std::vector<InterfaceInfo> filtered;
        for (auto& info : ifaces) {
            if (info.name == filter_iface) filtered.push_back(info);
        }
        ifaces = filtered;
    }
    
    return ifaces;
}

// ---------------------------------------------------------------------------
// Routes from /proc/net/route
// ---------------------------------------------------------------------------

struct RouteEntry {
    std::string iface;
    std::string dst;
    int prefix;
    std::string gateway;
    std::string flags;
    int metric;
};

static std::vector<RouteEntry> read_routes() {
    std::vector<RouteEntry> routes;
    std::string content = read_file("/proc/net/route");
    if (content.empty()) return routes;
    
    std::istringstream iss(content);
    std::string line;
    bool header = true;
    while (std::getline(iss, line)) {
        if (header) { header = false; continue; }
        std::istringstream lss(line);
        std::string iface, dst_hex, gw_hex, flags_hex, refcnt, use, metric_s, mask_hex;
        if (!(lss >> iface >> dst_hex >> gw_hex >> flags_hex >> refcnt >> use >> metric_s >> mask_hex)) continue;
        
        RouteEntry r;
        r.iface = iface;
        r.dst = hex_to_ipv4(dst_hex.c_str());
        r.gateway = (gw_hex == "00000000") ? "*" : hex_to_ipv4(gw_hex.c_str());
        r.metric = atoi(metric_s.c_str());
        
        uint32_t mask_val;
        sscanf(mask_hex.c_str(), "%08x", &mask_val);
        r.prefix = 0;
        for (uint32_t bit = 0x80000000; bit && mask_val; bit >>= 1) {
            if (mask_val & bit) r.prefix++;
            else break;
        }
        if (dst_hex == "00000000") r.prefix = 0;
        
        unsigned short fl;
        sscanf(flags_hex.c_str(), "%hx", &fl);
        std::string fstr;
        if (fl & 0x0001) fstr += "U";
        if (fl & 0x0002) fstr += "H";
        if (fl & 0x0004) fstr += "G";
        if (fl & 0x0008) fstr += "R";
        if (fl & 0x0010) fstr += "M";
        if (fl & 0x0020) fstr += "C";
        if (fl & 0x0040) fstr += "A";
        if (fstr.empty()) fstr = "-";
        r.flags = fstr;
        
        if (r.dst == "0.0.0.0" && r.prefix == 0) {
            r.dst = "default";
        }
        
        routes.push_back(r);
    }
    
    return routes;
}

// ---------------------------------------------------------------------------
// Output functions
// ---------------------------------------------------------------------------

static void print_addr(const std::vector<InterfaceInfo>& ifaces, int fam_filter) {
    for (const auto& iface : ifaces) {
        printf("%d: %s <%s> mtu %d\n",
               iface.index, iface.name.c_str(),
               iface.flags_str.c_str(), iface.mtu);
        printf("    link/%s %s",
               (iface.name == "lo") ? "loopback" : "ether",
               iface.mac.c_str());
        if (iface.name != "lo") printf(" brd ff:ff:ff:ff:ff:ff");
        printf("\n");

        for (const auto& addr : iface.addrs) {
            if (fam_filter == 4 && addr.family != "inet") continue;
            if (fam_filter == 6 && addr.family != "inet6") continue;

            const char* fam = (addr.family == "inet6") ? "inet6" : "inet";
            const char* scope = "global";
            if (addr.family == "inet" && addr.ip == "127.0.0.1") {
                scope = "host";
            }
            if (addr.family == "inet6" && (addr.ip == "::1" || addr.ip.find("fe80:") == 0)) {
                scope = (addr.ip == "::1") ? "host" : "link";
            }

            printf("    %s %s/%d scope %s\n",
                   fam, addr.ip.c_str(), addr.prefix, scope);
            printf("       valid_lft forever preferred_lft forever\n");
        }
    }
}

static void print_link(const std::vector<InterfaceInfo>& ifaces) {
    for (const auto& iface : ifaces) {
        const char* link_type = (iface.name == "lo") ? "loopback" : "ether";
        const char* brd = (iface.name == "lo") ? "00:00:00:00:00:00" : "ff:ff:ff:ff:ff:ff";
        printf("%d: %s <%s> mtu %d qdisc noqueue state %s ",
               iface.index, iface.name.c_str(),
               iface.flags_str.c_str(), iface.mtu,
               iface.operstate.c_str());
        printf("link/%s %s brd %s\n", link_type, iface.mac.c_str(), brd);
    }
}

static void print_link_stats(const std::vector<InterfaceInfo>& ifaces) {
    for (const auto& iface : ifaces) {
        printf("%d: %s <%s> mtu %d\n",
               iface.index, iface.name.c_str(),
               iface.flags_str.c_str(), iface.mtu);
        printf("    link/loopback %s brd 00:00:00:00:00:00\n",
               iface.mac.c_str());
        printf("    RX: bytes packets errors dropped mcast\n");
        printf("    %-8lld%-8lld%-7lld%-7lld%-7lld\n",
               iface.rx_bytes, iface.rx_packets, iface.rx_errors,
               0LL, 0LL);
        printf("    TX: bytes packets errors dropped carrier collsns\n");
        printf("    %-8lld%-8lld%-7lld%-7lld%-7lld%-7lld\n",
               iface.tx_bytes, iface.tx_packets, iface.tx_errors,
               0LL, 0LL, 0LL);
    }
}

static void print_route(const std::vector<RouteEntry>& routes) {
    for (const auto& r : routes) {
        if (r.dst == "default") {
            printf("default ");
        } else if (r.prefix > 0) {
            printf("%s/%d ", r.dst.c_str(), r.prefix);
        } else {
            printf("%s ", r.dst.c_str());
        }
        printf("dev %s ", r.iface.c_str());
        if (r.gateway != "*") {
            printf("via %s ", r.gateway.c_str());
        }
        if (!r.flags.empty()) {
            printf("proto kernel ");
            if (r.metric) printf("metric %d ", r.metric);
        }
        printf("\n");
    }
}

// ---------------------------------------------------------------------------
// Argument parsing (manual, to support subcommands like `ip addr`)
// ---------------------------------------------------------------------------

static void print_help(const char* prog) {
    printf("Usage: %s [OPTIONS] { addr | link | route } [SUBOPTIONS]\n", prog);
    printf("\n");
    printf("  -4, --4         show only IPv4 addresses/routes\n");
    printf("  -6, --6         show only IPv6 addresses/routes\n");
    printf("  -s, --statistics  show interface statistics\n");
    printf("  -h, --help      display this help and exit\n");
    printf("\n");
    printf("Commands:\n");
    printf("  addr show       show IP addresses on interfaces\n");
    printf("  link show       show link information of interfaces\n");
    printf("  route show      show routing table\n");
}

int ip_command(int argc, char** argv) {
    bool show_help = false;
    int fam_filter = 0; // 0=both, 4=ipv4, 6=ipv6
    bool show_stats = false;
    
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);
        if (a == "--help" || a == "-h") { show_help = true; continue; }
        if (a == "-4" || a == "--4") { fam_filter = 4; continue; }
        if (a == "-6" || a == "--6") { fam_filter = 6; continue; }
        if (a == "-s" || a == "--statistics") { show_stats = true; continue; }
        args.push_back(a);
    }
    
    if (show_help) {
        print_help(argv[0]);
        return 0;
    }
    
    if (args.empty()) {
        fprintf(stderr, "ip: need a subcommand: addr, link, or route\n");
        return 1;
    }
    
    std::string subcmd = args[0];
    std::string filter_iface;
    
    if (subcmd == "addr" || subcmd == "address") {
        // optional: show <iface>
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            filter_iface = args[i];
        }
    } else if (subcmd == "link") {
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            filter_iface = args[i];
        }
    } else if (subcmd == "route" || subcmd == "ru") {
        // no extra args
    } else {
        fprintf(stderr, "ip: unknown command \"%s\". Use: addr, link, route\n", subcmd.c_str());
        return 1;
    }
    
    if (subcmd == "addr" || subcmd == "address") {
        auto ifaces = enumerate_interfaces(false, filter_iface);
        print_addr(ifaces, fam_filter);
    } else if (subcmd == "link") {
        auto ifaces = enumerate_interfaces(show_stats, filter_iface);
        if (show_stats) print_link_stats(ifaces);
        else print_link(ifaces);
    } else if (subcmd == "route" || subcmd == "ru") {
        auto routes = read_routes();
        print_route(routes);
    }
    
    return 0;
}

REGISTER_COMMAND("ip", ip_command, "Show / manipulate routing, devices and tunneling");
