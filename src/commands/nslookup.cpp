#include "commands/nslookup.hpp"
#include <resolv.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser_compat.h>
#include <netdb.h>
#include <string>
#include <vector>
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {
constexpr int kDnsPort = 53;
constexpr int kMaxResponse = 4096;

static const char* type_name(int qtype) {
    switch (qtype) {
        case T_A:    return "A";
        case T_NS:   return "NS";
        case T_CNAME: return "CNAME";
        case T_SOA:  return "SOA";
        case T_PTR:  return "PTR";
        case T_MX:   return "MX";
        case T_TXT:  return "TXT";
        case T_AAAA: return "AAAA";
        case T_SRV:  return "SRV";
        default:     return "?";
    }
}

static std::string parse_name(const uint8_t* ans, int anslen, int& pos) {
    std::string result;
    bool first = true;
    while (pos < anslen) {
        uint8_t len = ans[pos++];
        if (len == 0) break;
        if ((len & 0xc0) == 0xc0) {
            if (pos + 1 > anslen) break;
            int offset = (((len & 0x3f) << 8) | ans[pos]);
            pos += 1;
            if (offset < 0 || offset >= anslen) break;
            int saved = pos;
            pos = offset;
            std::string pointed = parse_name(ans, anslen, pos);
            pos = saved;
            if (!first && !pointed.empty()) result += '.';
            result += pointed;
            break;
        }
        if (pos + len > anslen) break;
        if (!first) result += '.';
        result.append(reinterpret_cast<const char*>(ans + pos), len);
        pos += len;
        first = false;
    }
    return result;
}

static std::string build_domain_labels(const std::string& domain) {
    std::string labels;
    size_t prev = 0;
    size_t dot;
    while ((dot = domain.find('.', prev)) != std::string::npos) {
        size_t len = dot - prev;
        labels += static_cast<char>(static_cast<uint8_t>(len));
        labels.append(domain, prev, len);
        prev = dot + 1;
    }
    size_t len = domain.size() - prev;
    labels += static_cast<char>(static_cast<uint8_t>(len));
    labels.append(domain, prev, len);
    labels += '\0';
    return labels;
}

static std::vector<uint8_t> build_query(uint16_t id, const std::string& domain,
                                        uint16_t qtype) {
    std::vector<uint8_t> pkt;
    pkt.reserve(512);

    // Header (12 bytes)
    pkt.push_back((id >> 8) & 0xff);
    pkt.push_back(id & 0xff);
    pkt.push_back(0x01);  // RD=1
    pkt.push_back(0x00);
    pkt.push_back(0x00); pkt.push_back(0x01);  // QDCOUNT=1
    pkt.push_back(0x00); pkt.push_back(0x00);  // ANCOUNT=0
    pkt.push_back(0x00); pkt.push_back(0x00);  // NSCOUNT=0
    pkt.push_back(0x00); pkt.push_back(0x00);  // ARCOUNT=0

    // Question section
    std::string labels = build_domain_labels(domain);
    pkt.insert(pkt.end(), labels.begin(), labels.end());
    pkt.push_back((qtype >> 8) & 0xff);
    pkt.push_back(qtype & 0xff);
    pkt.push_back(0x00);  // QCLASS=IN
    pkt.push_back(0x01);

    return pkt;
}

static int send_dns_query(const std::string& server, const std::string& domain,
                          uint16_t qtype, uint8_t* resp, int resp_size) {
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDnsPort);
    if (inet_pton(AF_INET, server.c_str(), &addr.sin_addr) != 1) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    static uint16_t id_counter = 0;
    uint16_t id = ++id_counter;

    std::vector<uint8_t> query = build_query(id, domain, qtype);

    ssize_t sent = sendto(sock, query.data(), query.size(), 0,
                          reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (sent < 0) {
        close(sock);
        return -1;
    }

    // Wait for response with timeout
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sock, &set);
    struct timeval tv{5, 0};
    int ready = select(sock + 1, &set, nullptr, nullptr, &tv);
    if (ready <= 0) {
        close(sock);
        return -1;
    }

    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(sock, resp, resp_size, 0,
                         reinterpret_cast<struct sockaddr*>(&from), &fromlen);
    close(sock);
    return n > 0 ? static_cast<int>(n) : -1;
}

static void print_record(const std::string& name, uint32_t ttl, int rtype,
                         const std::string& rdata) {
    printf("%-40s\t%d\t%s\t%s\n", name.c_str(), ttl, type_name(rtype), rdata.c_str());
}

static int parse_response(uint8_t* ans, int anslen) {
    if (anslen < 12) {
        fprintf(stderr, "nslookup: response too short\n");
        return 1;
    }

    uint16_t flags = (ans[2] << 8) | ans[3];
    uint16_t rcode = flags & 0x0f;
    if (rcode != 0) {
        fprintf(stderr, "nslookup: DNS error code %d\n", rcode);
        return 1;
    }

    uint16_t qdcount = (ans[4] << 8) | ans[5];
    uint16_t ancount = (ans[6] << 8) | ans[7];

    // Skip question section
    int pos = 12;
    for (uint16_t i = 0; i < qdcount && pos < anslen; ++i) {
        parse_name(ans, anslen, pos);
        pos += 4;  // QTYPE + QCLASS
    }

    if (ancount > 0) {
        printf(";; ANSWER SECTION:\n");
        for (uint16_t i = 0; i < ancount && pos < anslen; ++i) {
            std::string name = parse_name(ans, anslen, pos);
            if (pos + 10 > anslen) break;

            int type = (ans[pos] << 8) | ans[pos + 1];
            pos += 2;
            pos += 2;  // CLASS
            uint32_t ttl = ((uint32_t)ans[pos] << 24) | ((uint32_t)ans[pos + 1] << 16) |
                           ((uint32_t)ans[pos + 2] << 8) | ans[pos + 3];
            pos += 4;
            uint16_t rdlen = (ans[pos] << 8) | ans[pos + 1];
            pos += 2;

            if (pos + rdlen > anslen) break;

            std::string rdata;
            int end = pos + rdlen;

            if (type == T_A && rdlen == 4) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, ans + pos, ip, sizeof(ip));
                rdata = ip;
            } else if (type == T_AAAA && rdlen == 16) {
                char ip[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, ans + pos, ip, sizeof(ip));
                rdata = ip;
            } else if (type == T_NS || type == T_CNAME || type == T_PTR) {
                int saved = pos;
                rdata = parse_name(ans, anslen, pos);
                pos = saved + rdlen;
            } else if (type == T_MX && rdlen >= 2) {
                uint16_t pref = (ans[pos] << 8) | ans[pos + 1];
                pos += 2;
                int saved = pos;
                std::string target = parse_name(ans, anslen, pos);
                pos = saved + rdlen;
                rdata = std::to_string(pref) + " " + target;
            } else if (type == T_TXT && rdlen > 0) {
                int saved = pos;
                while (pos < end) {
                    uint8_t txtlen = ans[pos++];
                    if (pos + txtlen > end) break;
                    if (!rdata.empty()) rdata += " ";
                    rdata.append(reinterpret_cast<const char*>(ans + pos), txtlen);
                    pos += txtlen;
                }
                pos = saved + rdlen;
            } else if (type == T_SOA) {
                int saved = pos;
                std::string mname = parse_name(ans, anslen, pos);
                std::string rname = parse_name(ans, anslen, pos);
                if (pos + 20 <= end) {
                    uint32_t serial = ((uint32_t)ans[pos] << 24) | ((uint32_t)ans[pos + 1] << 16) |
                                      ((uint32_t)ans[pos + 2] << 8) | ans[pos + 3];
                    uint32_t refresh = ((uint32_t)ans[pos + 4] << 24) | ((uint32_t)ans[pos + 5] << 16) |
                                       ((uint32_t)ans[pos + 6] << 8) | ans[pos + 7];
                    uint32_t retry = ((uint32_t)ans[pos + 8] << 24) | ((uint32_t)ans[pos + 9] << 16) |
                                     ((uint32_t)ans[pos + 10] << 8) | ans[pos + 11];
                    uint32_t expire = ((uint32_t)ans[pos + 12] << 24) | ((uint32_t)ans[pos + 13] << 16) |
                                      ((uint32_t)ans[pos + 14] << 8) | ans[pos + 15];
                    uint32_t minimum = ((uint32_t)ans[pos + 16] << 24) | ((uint32_t)ans[pos + 17] << 16) |
                                       ((uint32_t)ans[pos + 18] << 8) | ans[pos + 19];
                    rdata = mname + " " + rname + " " +
                            std::to_string(serial) + " " +
                            std::to_string(refresh) + " " +
                            std::to_string(retry) + " " +
                            std::to_string(expire) + " " +
                            std::to_string(minimum);
                }
                pos = saved + rdlen;
            } else if (type == T_SRV && rdlen >= 6) {
                uint16_t prio = (ans[pos] << 8) | ans[pos + 1];
                uint16_t weight = (ans[pos + 2] << 8) | ans[pos + 3];
                uint16_t port = (ans[pos + 4] << 8) | ans[pos + 5];
                pos += 6;
                int saved = pos;
                std::string target = parse_name(ans, anslen, pos);
                pos = saved + rdlen;
                rdata = std::to_string(prio) + " " +
                        std::to_string(weight) + " " +
                        std::to_string(port) + " " + target;
            } else {
                pos += rdlen;
                rdata = "[unsupported type " + std::to_string(type) + "]";
            }

            print_record(name, ttl, type, rdata);
        }
    }
    return 0;
}

static std::string resolve_server(const std::string& server) {
    if (!server.empty()) return server;
    // Try to get default server from /etc/resolv.conf or use 127.0.0.53
    FILE* fp = fopen("/etc/resolv.conf", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "nameserver", 10) == 0) {
                char* sep = strchr(line, ' ');
                if (sep) {
                    std::string ns(sep + 1);
                    ns.erase(ns.find_last_of(" \t\n\r"));
                    fclose(fp);
                    return ns;
                }
            }
        }
        fclose(fp);
    }
    return "127.0.0.53";
}

static std::string ip_to_ptr_domain(const std::string& ip) {
    struct in_addr addr{};
    if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
        return std::to_string(addr.s_addr & 0xff) + "." +
               std::to_string((addr.s_addr >> 8) & 0xff) + "." +
               std::to_string((addr.s_addr >> 16) & 0xff) + "." +
               std::to_string((addr.s_addr >> 24) & 0xff) + ".in-addr.arpa";
    }
    return "";
}

int nslookup_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "Show help");
    struct arg_lit* version_opt = arg_lit0("V", "version", "Show version");
    struct arg_str* type_opt = arg_str1("t", "type", "<type>", "Record type");
    struct arg_str* server_opt = arg_str0("s", "server", "<server>", "DNS server");
    struct arg_str* domain_opt = arg_str1(nullptr, nullptr, "<domain>", "Domain name");
    struct arg_end* end_opt = arg_end(1);

    void* argtable[] = { help_opt, version_opt, type_opt, server_opt, domain_opt, end_opt };
    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors != 0) {
        arg_print_errors(stderr, end_opt, "nslookup");
        arg_free(argtable);
        return 1;
    }

    if (help_opt->count > 0) {
        printf("Usage: modbox nslookup [options] <domain>\n");
        printf("\nOptions:\n");
        printf("  -t, --type=<type>        Record type: A, AAAA, MX, NS, CNAME, SOA, PTR, TXT, SRV\n");
        printf("  -s, --server=<server>    DNS server to query\n");
        printf("  -h, --help               Show help\n");
        printf("  -V, --version            Show version\n");
        arg_free(argtable);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("nslookup");
        arg_free(argtable);
        return 0;
    }

    std::string record_type = type_opt->sval[0];
    std::string dns_server = server_opt->count > 0 ? server_opt->sval[0] : "";
    std::string domain = domain_opt->sval[0];
    arg_free(argtable);

    int qtype = 0;
    std::string rtype_upper = record_type;
    for (auto& c : rtype_upper) c = toupper(c);

    if (rtype_upper == "A") qtype = T_A;
    else if (rtype_upper == "AAAA") qtype = T_AAAA;
    else if (rtype_upper == "MX") qtype = T_MX;
    else if (rtype_upper == "NS") qtype = T_NS;
    else if (rtype_upper == "CNAME") qtype = T_CNAME;
    else if (rtype_upper == "SOA") qtype = T_SOA;
    else if (rtype_upper == "PTR") qtype = T_PTR;
    else if (rtype_upper == "TXT") qtype = T_TXT;
    else if (rtype_upper == "SRV") qtype = T_SRV;
    else {
        fprintf(stderr, "nslookup: unknown record type '%s'\n", record_type.c_str());
        return 1;
    }

    std::string server = resolve_server(dns_server);

    // For PTR queries with an IP address, convert to reverse DNS format
    if (qtype == T_PTR) {
        std::string ptr_domain = ip_to_ptr_domain(domain);
        if (!ptr_domain.empty()) {
            domain = ptr_domain;
        }
    }

    uint8_t ans[kMaxResponse] = {};
    int n = send_dns_query(server, domain, qtype, ans, sizeof(ans));
    if (n < 0) {
        fprintf(stderr, "nslookup: query failed (%s)\n", strerror(errno));
        return 1;
    }

    printf("Server:\t\t%s\n", server.c_str());
    printf("Address:\t%s#%d\n\n", server.c_str(), kDnsPort);

    parse_response(ans, n);
    return 0;
}

REGISTER_COMMAND("nslookup", nslookup_command, "Query DNS name servers");
} // namespace
