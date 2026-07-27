#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cerrno>

#include "commands/hostname.hpp"
#include "commands/command_macros.hpp"

static constexpr size_t HOSTNAME_BUF = 64;

static std::vector<std::string> get_host_addresses() {
    std::vector<std::string> addrs;

    char hostname[HOSTNAME_BUF];
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        return addrs;
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = 0;
    hints.ai_flags = AI_ADDRCONFIG;

    if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
        for (struct addrinfo* p = res; p != NULL; p = p->ai_next) {
            char ipstr[INET6_ADDRSTRLEN];
            if (p->ai_family == AF_INET) {
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                const char* ip = inet_ntoa(ipv4->sin_addr);
                if (ip) addrs.push_back(std::string(ip));
            } else if (p->ai_family == AF_INET6) {
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
                if (inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipstr, sizeof(ipstr)) != NULL) {
                    addrs.push_back(std::string(ipstr));
                }
            }
        }
        freeaddrinfo(res);
    }

    addrs.erase(std::unique(addrs.begin(), addrs.end()), addrs.end());
    return addrs;
}

static std::string get_domain_name() {
    char domain[HOSTNAME_BUF];
    if (getdomainname(domain, sizeof(domain)) >= 0 && strlen(domain) > 0 && strcmp(domain, "(none)") != 0) {
        return std::string(domain);
    }
    return "";
}

static std::string get_fqdn() {
    char hostname[HOSTNAME_BUF];
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        return "";
    }
    std::string hn(hostname);
    if (hn.find('.') != std::string::npos) {
        return hn;
    }
    std::string domain = get_domain_name();
    if (!domain.empty()) {
        return hn + "." + domain;
    }
    return hn;
}

void hostname_command(int argc, char** argv) {
    bool show_aliases = false;
    bool show_domain = false;
    bool show_fqdn = false;
    bool show_ips = false;
    bool show_all_ips = false;
    bool show_short = false;
    bool set_hostname = false;
    std::string new_hostname;
    bool help = false, version = false;

    int i = 1;
    while (i < argc) {
        const char* opt = argv[i];
        if (strcmp(opt, "-a") == 0) { show_aliases = true; i++; continue; }
        if (strcmp(opt, "-d") == 0) { show_domain = true; i++; continue; }
        if (strcmp(opt, "-f") == 0) { show_fqdn = true; i++; continue; }
        if (strcmp(opt, "-i") == 0) { show_ips = true; i++; continue; }
        if (strcmp(opt, "-I") == 0) { show_all_ips = true; i++; continue; }
        if (strcmp(opt, "-s") == 0) { show_short = true; i++; continue; }
        if (strcmp(opt, "-p") == 0) { show_domain = true; i++; continue; }
        if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) { help = true; i++; continue; }
        if (strcmp(opt, "-V") == 0 || strcmp(opt, "--version") == 0) { version = true; i++; continue; }
        if (opt[0] != '-') {
            set_hostname = true;
            new_hostname = opt;
            i++; continue;
        }
        i++; // unknown option, skip
    }

    if (version) {
        printf("hostname (modbox) 1.0\n");
        printf("Copyright (C) 2026 modbox\n");
        printf("License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>\n");
        return;
    }

    if (help) {
        printf("Usage: hostname [OPTION]... [HOSTNAME]\n");
        printf("Print or set the host name.\n");
        printf("\n");
        printf("  -a, --aliases         print alias names\n");
        printf("  -d, --domain          print DNS domain name\n");
        printf("  -f, --fqdn            print fully qualified domain name\n");
        printf("  -i, --addresses       print IP addresses (space separated)\n");
        printf("  -I, --all-addresses   print all configured addresses (one per line)\n");
        printf("  -s, --short           print node name (short form)\n");
        printf("  -p, --precise         print domain name (same as -d)\n");
        printf("  -h, --help            display this help and exit\n");
        printf("\n");
        printf("With no options, prints the nodename. If a single argument is given,\n");
        printf("sets the host name to that value (requires appropriate privileges).\n");
        return;
    }

    if (set_hostname && !show_aliases && !show_domain && !show_fqdn && !show_ips && !show_all_ips && !show_short) {
        if (sethostname(new_hostname.c_str(), new_hostname.length()) < 0) {
            fprintf(stderr, "hostname: %s\n", strerror(errno));
            return;
        }
        printf("%s\n", new_hostname.c_str());
        return;
    }

    char hostname_buf[HOSTNAME_BUF];
    if (gethostname(hostname_buf, sizeof(hostname_buf)) < 0) {
        fprintf(stderr, "hostname: %s\n", strerror(errno));
        return;
    }
    std::string nodename(hostname_buf);

    bool any_display = show_aliases || show_domain || show_fqdn || show_ips || show_all_ips || show_short;
    if (!any_display) show_short = true;

    bool first = true;

    if (show_aliases) {
        printf("%s", nodename.c_str());
        first = false;
    }

    if (show_domain) {
        std::string domain = get_domain_name();
        if (!domain.empty()) {
            if (!first) printf(" ");
            printf("%s", domain.c_str());
            first = false;
        }
    }

    if (show_fqdn) {
        std::string fqdn = get_fqdn();
        if (!fqdn.empty()) {
            if (!first) printf(" ");
            printf("%s", fqdn.c_str());
            first = false;
        }
    }

    if (show_ips) {
        std::vector<std::string> addrs = get_host_addresses();
        for (size_t j = 0; j < addrs.size(); ++j) {
            if (j > 0) printf(" ");
            printf("%s", addrs[j].c_str());
        }
        first = false;
    }

    if (show_all_ips) {
        std::vector<std::string> addrs = get_host_addresses();
        for (size_t j = 0; j < addrs.size(); ++j) {
            if (j > 0) putchar('\n');
            printf("%s", addrs[j].c_str());
            first = false;
        }
    }

    if (show_short) {
        std::string short_name = nodename;
        size_t dot_pos = short_name.find('.');
        if (dot_pos != std::string::npos) {
            short_name = short_name.substr(0, dot_pos);
        }
        if (!first) printf(" ");
        printf("%s", short_name.c_str());
        first = false;
    }

    if (first) {
        printf("%s", nodename.c_str());
    }
    putchar('\n');
}

REGISTER_COMMAND("hostname", hostname_command, "Show or set the host name");