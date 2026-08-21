#include "commands/dns_util.hpp"
#include <resolv.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <vector>

const char* dns_type_name(int qtype) {
    switch (qtype) {
        case T_A:     return "A";
        case T_NS:    return "NS";
        case T_CNAME: return "CNAME";
        case T_SOA:   return "SOA";
        case T_PTR:   return "PTR";
        case T_MX:    return "MX";
        case T_TXT:   return "TXT";
        case T_AAAA:  return "AAAA";
        case T_SRV:   return "SRV";
        default:      return "?";
    }
}

std::string dns_parse_name(const uint8_t* ans, int anslen, int& pos) {
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
            std::string pointed = dns_parse_name(ans, anslen, pos);
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

std::string dns_build_domain_labels(const std::string& domain) {
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

std::vector<uint8_t> dns_build_query(uint16_t id, const std::string& domain,
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
    std::string labels = dns_build_domain_labels(domain);
    pkt.insert(pkt.end(), labels.begin(), labels.end());
    pkt.push_back((qtype >> 8) & 0xff);
    pkt.push_back(qtype & 0xff);
    pkt.push_back(0x00);  // QCLASS=IN
    pkt.push_back(0x01);

    return pkt;
}

int dns_send_query(const std::string& server, const std::string& domain,
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

    std::vector<uint8_t> query = dns_build_query(id, domain, qtype);

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

std::string dns_resolve_server(const std::string& server) {
    if (!server.empty()) return server;
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

std::string dns_ip_to_ptr_domain(const std::string& ip) {
    struct in_addr addr{};
    if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
        return std::to_string(addr.s_addr & 0xff) + "." +
               std::to_string((addr.s_addr >> 8) & 0xff) + "." +
               std::to_string((addr.s_addr >> 16) & 0xff) + "." +
               std::to_string((addr.s_addr >> 24) & 0xff) + ".in-addr.arpa";
    }
    return "";
}

namespace {

static std::string parse_rdata(const uint8_t* ans, int anslen, int& pos,
                                int rdlen, int type) {
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
        rdata = dns_parse_name(ans, anslen, pos);
        pos = saved + rdlen;
    } else if (type == T_MX && rdlen >= 2) {
        uint16_t pref = (ans[pos] << 8) | ans[pos + 1];
        pos += 2;
        int saved = pos;
        std::string target = dns_parse_name(ans, anslen, pos);
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
        std::string mname = dns_parse_name(ans, anslen, pos);
        std::string rname = dns_parse_name(ans, anslen, pos);
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
        std::string target = dns_parse_name(ans, anslen, pos);
        pos = saved + rdlen;
        rdata = std::to_string(prio) + " " +
                std::to_string(weight) + " " +
                std::to_string(port) + " " + target;
    } else {
        pos += rdlen;
        rdata = "[unsupported type " + std::to_string(type) + "]";
    }
    return rdata;
}

}  // namespace

int dns_parse_response(const uint8_t* ans, int anslen, DnsResponse& out) {
    if (anslen < 12) {
        return 1;
    }

    out.id       = (ans[0] << 8) | ans[1];
    out.flags    = (ans[2] << 8) | ans[3];
    out.rcode    = out.flags & 0x0f;
    out.qdcount  = (ans[4] << 8) | ans[5];
    out.ancount  = (ans[6] << 8) | ans[7];
    out.nscount  = (ans[8] << 8) | ans[9];
    out.arcount  = (ans[10] << 8) | ans[11];
    out.recv_size = anslen;

    if (out.rcode != 0) {
        return 1;
    }

    // Parse question section
    int pos = 12;
    for (uint16_t i = 0; i < out.qdcount && pos < anslen; ++i) {
        std::string qname = dns_parse_name(ans, anslen, pos);
        pos += 4;  // QTYPE + QCLASS
        out.question_names.push_back(qname);
    }

    auto parse_section = [&](std::vector<DnsRecord>& records) {
        for (uint16_t i = 0; i < out.ancount && pos < anslen; ++i) {
            DnsRecord rec;
            rec.name = dns_parse_name(ans, anslen, pos);
            if (pos + 10 > anslen) break;

            int type = (ans[pos] << 8) | ans[pos + 1];
            pos += 2;
            pos += 2;  // CLASS
            rec.ttl = ((uint32_t)ans[pos] << 24) | ((uint32_t)ans[pos + 1] << 16) |
                      ((uint32_t)ans[pos + 2] << 8) | ans[pos + 3];
            pos += 4;
            uint16_t rdlen = (ans[pos] << 8) | ans[pos + 1];
            pos += 2;

            if (pos + rdlen > anslen) break;

            rec.type = type;
            rec.rdata = parse_rdata(ans, anslen, pos, rdlen, type);
            records.push_back(rec);
        }
    };

    parse_section(out.answers);
    parse_section(out.authority);
    parse_section(out.additional);

    return 0;
}
