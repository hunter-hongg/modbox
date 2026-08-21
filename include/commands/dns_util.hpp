#ifndef DNS_UTIL_HPP
#define DNS_UTIL_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace dns {
// Default DNS port (53).
constexpr int kDnsPort = 53;
// Maximum DNS response buffer size.
constexpr int kMaxResponse = 4096;
}  // namespace dns

using dns::kDnsPort;
using dns::kMaxResponse;

// DNS record type names used by both nslookup and dig.
const char* dns_type_name(int qtype);

// Parse a compressed DNS name from a buffer. Updates pos on success.
std::string dns_parse_name(const uint8_t* ans, int anslen, int& pos);

// Build the label-encoded form of a domain name for a DNS query.
std::string dns_build_domain_labels(const std::string& domain);

// Build a standard DNS query packet (header + question section).
std::vector<uint8_t> dns_build_query(uint16_t id, const std::string& domain,
                                     uint16_t qtype);

// Send a DNS query over UDP and receive the response.
// Returns negative on error, positive on success (bytes received).
int dns_send_query(const std::string& server, const std::string& domain,
                   uint16_t qtype, uint8_t* resp, int resp_size);

// Resolve the DNS server to query. Uses provided server if non-empty,
// otherwise reads /etc/resolv.conf, falling back to 127.0.0.53.
std::string dns_resolve_server(const std::string& server);

// Convert an IPv4 address to its in-addr.arpa reverse lookup domain.
std::string dns_ip_to_ptr_domain(const std::string& ip);

// ─────────────────────────────────────────────────────────────────────────────
// DNS response data structures
// ─────────────────────────────────────────────────────────────────────────────

struct DnsRecord {
    std::string name;
    uint32_t ttl  = 0;
    int       type = 0;   // T_A, T_NS, …
    std::string rdata;
};

struct DnsResponse {
    uint16_t id        = 0;
    uint16_t flags     = 0;
    uint16_t rcode     = 0;
    uint16_t qdcount   = 0;
    uint16_t ancount   = 0;
    uint16_t nscount   = 0;
    uint16_t arcount   = 0;
    std::vector<std::string> question_names;   // one per question entry
    std::vector<DnsRecord>   answers;
    std::vector<DnsRecord>   authority;
    std::vector<DnsRecord>   additional;
    int recv_size      = 0;
};

// Parse a raw DNS response into structured data.
// Returns 0 on success, non-zero on error (short response, non-zero rcode).
int dns_parse_response(const uint8_t* ans, int anslen, DnsResponse& out);

#endif // DNS_UTIL_HPP
