#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <sstream>
#include <argtable3.h>
#include <arpa/inet.h>

#include "commands/ss.hpp"
#include "commands/command_macros.hpp"

static std::string read_all(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return "";
    std::string s;
    char b[4096];
    size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    fclose(f);
    return s;
}

struct Entry {
    const char* type;
    bool ipv6;
    const char* state;
    unsigned recv_q, send_q;
    char local[64];
    char remote[64];
};

static void hex_to_ip4(const char* h, char* out) {
    uint32_t val;
    sscanf(h, "%08x", &val);
    struct in_addr a;
    a.s_addr = val;
    strncpy(out, inet_ntoa(a), 63);
    out[63] = '\0';
}

static void hex_to_ip6(const char* h, char* out) {
    uint8_t bytes[16];
    for (int i = 0; i < 16; i++) {
        unsigned x;
        sscanf(h + i*2, "%02x", &x);
        bytes[i] = (uint8_t)x;
    }
    inet_ntop(AF_INET6, bytes, out, 64);
}

static void parse_addr(const char* raw, char* ip_out, unsigned* port_out) {
    const char* cp = strchr(raw, ':');
    if (!cp) { *ip_out = '\0'; *port_out = 0; return; }
    size_t len = (size_t)(cp - raw);
    if (len == 8) {
        char buf[9];
        strncpy(buf, raw, 8); buf[8] = '\0';
        hex_to_ip4(buf, ip_out);
    } else if (len == 32) {
        char buf[33];
        strncpy(buf, raw, 32); buf[32] = '\0';
        hex_to_ip6(buf, ip_out);
    } else {
        strncpy(ip_out, raw, 63); ip_out[63] = '\0';
    }
    sscanf(cp + 1, "%04x", port_out);
}

static const char* state_str(const char* s) {
    unsigned val;
    sscanf(s, "%x", &val);
    switch(val) {
        case 1: return "ESTABLISHED"; case 2: return "SYN-SENT";
        case 3: return "SYN-RECV"; case 4: return "FIN-WAIT-1";
        case 5: return "FIN-WAIT-2"; case 6: return "TIME-WAIT";
        case 7: return "CLOSE"; case 8: return "CLOSE-WAIT";
        case 9: return "LAST-ACK"; case 10: return "LISTEN";
        case 11: return "CLOSING"; default: return "?";
    }
}

static void read_tcp_file(const char* path, bool ipv6, std::vector<Entry>& out) {
    std::string content = read_all(path);
    if (content.empty()) return;
    
    std::istringstream iss(content);
    std::string line;
    bool skip = true;
    while (std::getline(iss, line)) {
        if (skip) { skip = false; continue; }
        std::istringstream ls(line);
        std::string sl, local, remote, st, txq, rxq;
        if (!(ls >> sl >> local >> remote >> st >> txq >> rxq)) continue;
        
        Entry e;
        e.type = "tcp"; e.ipv6 = ipv6;
        e.state = state_str(st.c_str());
        e.recv_q = 0; e.send_q = 0;
        unsigned lp = 0, rp = 0;
        char lip[64], rip[64];
        parse_addr(local.c_str(), lip, &lp);
        parse_addr(remote.c_str(), rip, &rp);
        snprintf(e.local, 64, "%s:%u", lip, lp);
        snprintf(e.remote, 64, "%s:%u", rip, rp);
        out.push_back(e);
    }
}

static void read_udp_file(const char* path, bool ipv6, std::vector<Entry>& out) {
    std::string content = read_all(path);
    if (content.empty()) return;
    
    std::istringstream iss(content);
    std::string line;
    bool skip = true;
    while (std::getline(iss, line)) {
        if (skip) { skip = false; continue; }
        std::istringstream ls(line);
        std::string sl, local, remote, st;
        if (!(ls >> sl >> local >> remote >> st)) continue;
        
        Entry e;
        e.type = "udp"; e.ipv6 = ipv6;
        e.state = state_str(st.c_str());
        unsigned v;
        sscanf(st.c_str(), "%x", &v);
        if (v == 7) e.state = "UNCONN";
        e.recv_q = 0; e.send_q = 0;
        unsigned lp = 0, rp = 0;
        char lip[64], rip[64];
        parse_addr(local.c_str(), lip, &lp);
        parse_addr(remote.c_str(), rip, &rp);
        snprintf(e.local, 64, "%s:%u", lip, lp);
        snprintf(e.remote, 64, "%s:%u", rip, rp);
        out.push_back(e);
    }
}

int ss_command(int argc, char** argv) {
    bool show_tcp = false, show_udp = false;
    bool listening = false, all = false;
    bool filter4 = false, filter6 = false;
    bool help = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) help = true;
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tcp") == 0) show_tcp = true;
        else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--udp") == 0) show_udp = true;
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--listening") == 0) listening = true;
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) all = true;
        else if (strcmp(argv[i], "-4") == 0 || strcmp(argv[i], "--4") == 0) filter4 = true;
        else if (strcmp(argv[i], "-6") == 0 || strcmp(argv[i], "--6") == 0) filter6 = true;
        else if (argv[i][0] == '-') {
            for (char* p = (char*)argv[i] + 1; *p; p++) {
                if (*p == 't') show_tcp = true;
                else if (*p == 'u') show_udp = true;
                else if (*p == 'l') listening = true;
                else if (*p == 'a') all = true;
                else if (*p == '4') filter4 = true;
                else if (*p == '6') filter6 = true;
                else if (*p == 'n') { }
                else { fprintf(stderr, "ss: unknown option '-%c'\n", *p); return 1; }
            }
        }
    }
    
    if (help) {
        printf("Usage: ss [OPTION]...\n\n");
        printf("  -t, --tcp          show TCP sockets\n");
        printf("  -u, --udp          show UDP sockets\n");
        printf("  -l, --listening    show only listening sockets\n");
        printf("  -a, --all          show all sockets\n");
        printf("  -n, --numeric      don't resolve service names\n");
        printf("  -4, --4            show only IPv4 sockets\n");
        printf("  -6, --6            show only IPv6 sockets\n");
        printf("  -h, --help         display this help and exit\n");
        return 0;
    }
    
    if (!show_tcp && !show_udp) show_tcp = true;
    if (!listening && !all) all = true;
    
    std::vector<Entry> entries;
    if (show_tcp) {
        read_tcp_file("/proc/net/tcp", false, entries);
        read_tcp_file("/proc/net/tcp6", true, entries);
    }
    if (show_udp) {
        read_udp_file("/proc/net/udp", false, entries);
        read_udp_file("/proc/net/udp6", true, entries);
    }
    
    printf("%-8s %-22s %-22s %s\n", "State", "Local Address:Port", "Peer Address:Port", "");
    
    for (const auto& e : entries) {
        if (filter4 && e.ipv6) continue;
        if (filter6 && !e.ipv6) continue;
        
        bool type_ok = (show_tcp && show_udp) || 
                       (show_tcp && strcmp(e.type, "tcp") == 0) ||
                       (show_udp && strcmp(e.type, "udp") == 0);
        if (!type_ok) continue;
        
        bool state_ok = true;
        if (listening && !all) {
            if (strcmp(e.type, "tcp") == 0) state_ok = (strcmp(e.state, "LISTEN") == 0);
            else state_ok = (strcmp(e.state, "UNCONN") == 0);
        }
        if (!state_ok) continue;
        
        printf("%-8s %-22s %-22s\n", e.state, e.local, e.remote);
    }
    
    return 0;
}

REGISTER_COMMAND("ss", ss_command, "Statistics of socket messages");
