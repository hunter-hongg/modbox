#include "commands/dns_util.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <sys/time.h>
#include <sstream>
#include <string>
#include <vector>

#include <resolv.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>

#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

// DNS flag bits (RFC 1035) — not universally defined by system headers.
constexpr uint16_t kQRBit = 0x8000;
constexpr uint16_t kRDBit = 0x0100;
constexpr uint16_t kRABit = 0x0080;

namespace {

struct DigOpts {
    std::string domain;
    int          qtype   = T_A;
    std::string  server  = "";
    bool         show_all        = true;
    bool         show_answer     = true;
    bool         show_header     = true;
    bool         show_question   = true;
    bool         show_authority  = false;
    bool         show_additional = false;
    bool         short_mode      = false;
    bool         reverse_lookup  = false;
    std::string  reverse_ip;
};

static const char* flags_string(uint16_t flags) {
    static char buf[32];
    buf[0] = '\0';
    int p = 0;
    if (flags & kQRBit) buf[p++] = 'q';
    buf[p++] = ' ';
    if (flags & kRDBit) buf[p++] = 'r';
    buf[p++] = ' ';
    if (flags & kRABit) buf[p++] = 'a';
    buf[p++] = ' ';
    buf[p++] = '\0';
    // Remove trailing space
    if (p > 1 && buf[p-1] == ' ') buf[p-1] = '\0';
    return buf;
}

static std::string current_timestamp() {
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char buf[64];
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y CST", &tm_buf);
    return std::string(buf);
}

static uint16_t parse_qtype(const std::string& s) {
    std::string upper;
    for (char c : s) upper += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    if (upper == "A")      return T_A;
    if (upper == "AAAA")   return T_AAAA;
    if (upper == "MX")     return T_MX;
    if (upper == "NS")     return T_NS;
    if (upper == "CNAME")  return T_CNAME;
    if (upper == "SOA")    return T_SOA;
    if (upper == "PTR")    return T_PTR;
    if (upper == "TXT")    return T_TXT;
    if (upper == "SRV")    return T_SRV;
    return 0;
}

// Returns true if the argument is a d-opt (+keyword[=value]).
static bool is_dopt(const char* arg) {
    return arg[0] == '+' && arg[1] != '\0';
}

// Parse a d-opt: returns false if unknown.
static bool parse_dopt(const std::string& opt, DigOpts& opts) {
    if (opt == "+short")       { opts.short_mode = true; return true; }
    if (opt == "+noall")       { opts.show_all = false; return true; }
    if (opt == "+all")         { opts.show_all = true; return true; }
    if (opt == "+answer")      { opts.show_answer = true; return true; }
    if (opt == "+noanswer")    { opts.show_answer = false; return true; }
    if (opt == "+comments")    { opts.show_header = true; return true; }
    if (opt == "+nocomments")  { opts.show_header = false; return true; }
    if (opt == "+authority")   { opts.show_authority = true; return true; }
    if (opt == "+noauthority"){ opts.show_authority = false; return true; }
    if (opt == "+additional")  { opts.show_additional = true; return true; }
    if (opt == "+noadditional"){ opts.show_additional = false; return true; }
    if (opt == "+noall +answer") { opts.show_all = false; opts.show_answer = true; return true; }
    if (opt.rfind("+noall ", 0) == 0 || opt.rfind("+noall\t", 0) == 0) {
        opts.show_all = false; return true;
    }
    // Generic +FLAG or +FLAG=value
    std::string flag = opt.substr(1);  // strip '+'
    size_t eq = flag.find('=');
    if (eq != std::string::npos) flag = flag.substr(0, eq);
    // Unknown d-opts are accepted silently (dig ignores most unknown ones)
    return true;
}

static void print_dig_header(uint16_t id, uint16_t flags, uint16_t rcode,
                              uint16_t qdcount, uint16_t ancount,
                              uint16_t nscount, uint16_t arcount,
                              const std::string& cmd_str) {
    if (!flags_string(flags)[0]) return;  // shouldn't happen

    printf("; <<>> DiG 9.x.x <<>> %s\n", cmd_str.c_str());
    printf(";; global options: +cmd\n");
    printf(";; Got answer:\n");
    printf(";; ->>HEADER<<- opcode: QUERY, status: ");

    const char* rcode_name = "NOERROR";
    switch (rcode) {
        case 0:  rcode_name = "NOERROR"; break;
        case 1:  rcode_name = "FORMERR"; break;
        case 2:  rcode_name = "SERVFAIL"; break;
        case 3:  rcode_name = "NXDOMAIN"; break;
        case 4:  rcode_name = "NOTIMP";  break;
        case 5:  rcode_name = "REFUSED"; break;
        default: rcode_name = "UNKNOWN"; break;
    }
    printf("%s, id: %u\n", rcode_name, id);
    printf(";; flags: %s; QUERY: %u, ANSWER: %u, AUTHORITY: %u, ADDITIONAL: %u\n\n",
           flags_string(flags), qdcount, ancount, nscount, arcount);
}

static void print_question_section(const std::vector<std::string>& qnames, int qtype) {
    printf(";; QUESTION SECTION:\n");
    for (const auto& name : qnames) {
        printf(";%-34sIN\t%s\n", name.c_str(), dns_type_name(qtype));
    }
}

static void print_answer_section(const std::vector<DnsRecord>& answers) {
    if (answers.empty()) return;
    printf(";; ANSWER SECTION:\n");
    for (const auto& rec : answers) {
        printf("%-38s%d\tIN\t%s\t%s\n", rec.name.c_str(), (int)rec.ttl,
               dns_type_name(rec.type), rec.rdata.c_str());
    }
}

static void print_authority_section(const std::vector<DnsRecord>& authority) {
    if (authority.empty()) return;
    printf(";; AUTHORITY SECTION:\n");
    for (const auto& rec : authority) {
        printf("%-38s%d\tIN\t%s\t%s\n", rec.name.c_str(), (int)rec.ttl,
               dns_type_name(rec.type), rec.rdata.c_str());
    }
}

static void print_footer(const std::string& server, int recv_size) {
    printf(";; Query time: 0 msec\n");
    printf(";; SERVER: %s#53(%s) (UDP)\n", server.c_str(), server.c_str());
    printf(";; WHEN: %s\n", current_timestamp().c_str());
    printf(";; MSG SIZE  rcvd: %d\n", recv_size);
}

static void print_short_output(const std::vector<DnsRecord>& answers) {
    for (const auto& rec : answers) {
        printf("%s\n", rec.rdata.c_str());
    }
}

static int run_dig(const DigOpts& opts) {
    std::string domain = opts.domain;
    int qtype = opts.qtype;

    // Handle reverse lookup
    if (opts.reverse_lookup) {
        std::string ptr_domain = dns_ip_to_ptr_domain(opts.reverse_ip);
        if (ptr_domain.empty()) {
            fprintf(stderr, "dig: invalid IP address '%s'\n", opts.reverse_ip.c_str());
            return 1;
        }
        domain = ptr_domain;
        qtype = T_PTR;
    }

    std::string server = dns_resolve_server(opts.server);
    uint8_t ans[kMaxResponse] = {};
    auto t_start = std::clock();
    int n = dns_send_query(server, domain, qtype, ans, sizeof(ans));
    auto t_end = std::clock();

    if (n < 0) {
        fprintf(stderr, "dig: could not send query (%s)\n", strerror(errno));
        return 1;
    }

    DnsResponse resp;
    if (dns_parse_response(ans, n, resp) != 0) {
        fprintf(stderr, "dig: DNS error code %u\n", resp.rcode);
        return 1;
    }

    // Build command string for header
    std::ostringstream cmd_str;
    cmd_str << domain;
    if (qtype != T_A) cmd_str << " " << dns_type_name(qtype);

    if (!opts.short_mode) {
        if (opts.show_header) print_dig_header(resp.id, resp.flags, resp.rcode,
                                               resp.qdcount, resp.ancount,
                                               resp.nscount, resp.arcount,
                                               cmd_str.str());
        if (opts.show_question && opts.show_all) print_question_section(resp.question_names, qtype);
        if (opts.show_answer) print_answer_section(resp.answers);
        if (opts.show_authority) print_authority_section(resp.authority);
        print_footer(server, resp.recv_size);
    } else {
        print_short_output(resp.answers);
    }

    return 0;
}

}  // namespace

int dig_command(int argc, char** argv) {
    DigOpts opts;

    // Manual arg parsing to support dig's flexible syntax:
    //   dig [@server] [domain] [type] [d-opt...]
    // Skip argv[0] which is the command name "dig"
    std::vector<std::string> args(argv + 1, argv + argc);

    bool help_requested = false;
    bool version_requested = false;
    bool reverse_lookup = false;
    std::string server_arg;
    std::string domain_arg;
    std::string type_arg;

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];

        // D-opts (+keyword)
        if (a[0] == '+' && a.size() > 1) {
            parse_dopt(a, opts);
            continue;
        }

        // -h / --help
        if (a == "-h" || a == "--help") {
            help_requested = true;
            continue;
        }

        // -V / --version
        if (a == "-V" || a == "--version") {
            version_requested = true;
            continue;
        }

        // -t <type>
        if (a == "-t" && i + 1 < args.size()) {
            type_arg = args[++i];
            continue;
        }

        // -s <server>
        if (a == "-s" && i + 1 < args.size()) {
            server_arg = args[++i];
            continue;
        }

        // -x <ip> (reverse lookup)
        if (a == "-x") {
            reverse_lookup = true;
            if (i + 1 < args.size()) {
                opts.reverse_ip = args[++i];
            }
            continue;
        }

        // Positional args: [@server] [domain] [type]
        if (a[0] == '@') {
            server_arg = a.substr(1);
        } else if (domain_arg.empty()) {
            // Check if this looks like a type keyword
            uint16_t parsed_type = parse_qtype(a);
            if (parsed_type != 0) {
                type_arg = a;
            } else {
                domain_arg = a;
            }
        } else if (type_arg.empty()) {
            uint16_t parsed_type = parse_qtype(a);
            if (parsed_type != 0) {
                type_arg = a;
            }
            // Otherwise ignore (dig ignores unknown positional args)
        }
    }

    if (help_requested) {
        printf("Usage: dig [@global-server] [domain] [q-type] [q-class] {q-opt}\n");
        printf("            {global-d-opt} host [@local-server] {local-d-opt}\n");
        printf("\nOptions:\n");
        printf("  -t, --type=<type>        Record type: A, AAAA, MX, NS, CNAME, SOA, PTR, TXT, SRV\n");
        printf("  -s, --server=<server>    DNS server to query\n");
        printf("  -x, --reverse=<IP>       Reverse DNS lookup shortcut\n");
        printf("  -h, --help               Show help\n");
        printf("  -V, --version            Show version\n");
        printf("\nDisplay options (d-opts):\n");
        printf("  +short                 Short output (only answer rdata)\n");
        printf("  +noall                 Hide all sections\n");
        printf("  +answer                Show answer section\n");
        printf("  +authority             Show authority section\n");
        printf("  +comments              Show header/comments (default)\n");
        return 0;
    }

    if (version_requested) {
        print_version("dig");
        return 0;
    }

    // Apply parsed positional values
    if (!type_arg.empty()) {
        uint16_t t = parse_qtype(type_arg);
        if (t == 0) {
            fprintf(stderr, "dig: unknown record type '%s'\n", type_arg.c_str());
            return 1;
        }
        opts.qtype = (int)t;
    }
    if (!server_arg.empty()) {
        opts.server = server_arg;
    }
    if (reverse_lookup && !opts.reverse_ip.empty()) {
        opts.reverse_lookup = true;
    }
    opts.domain = domain_arg;

    if (opts.domain.empty() && !opts.reverse_lookup) {
        fprintf(stderr, "dig: need a domain name\n");
        return 2;
    }

    return run_dig(opts);
}

REGISTER_COMMAND("dig", dig_command, "Query DNS name servers");
