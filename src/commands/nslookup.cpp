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
#include "commands/dns_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

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

    std::string server = dns_resolve_server(dns_server);

    // For PTR queries with an IP address, convert to reverse DNS format
    if (qtype == T_PTR) {
        std::string ptr_domain = dns_ip_to_ptr_domain(domain);
        if (!ptr_domain.empty()) {
            domain = ptr_domain;
        }
    }

    uint8_t ans[kMaxResponse] = {};
    int n = dns_send_query(server, domain, qtype, ans, sizeof(ans));
    if (n < 0) {
        fprintf(stderr, "nslookup: query failed (%s)\n", strerror(errno));
        return 1;
    }

    printf("Server:\t\t%s\n", server.c_str());
    printf("Address:\t%s#%d\n\n", server.c_str(), kDnsPort);

    DnsResponse resp;
    if (dns_parse_response(ans, n, resp) == 0) {
        for (const auto& rec : resp.answers) {
            printf("%-40s\t%d\t%s\t%s\n", rec.name.c_str(), rec.ttl,
                   dns_type_name(rec.type), rec.rdata.c_str());
        }
    }
    return 0;
}

REGISTER_COMMAND("nslookup", nslookup_command, "Query DNS name servers");
