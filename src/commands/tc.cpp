#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/gen_stats.h>

#include "commands/tc.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct TcOptions {
    bool show_stats = false;   // -s / --stats
    bool show_details = false; // -d / --details
    bool json_mode = false;    // -json
    bool pretty = false;       // -pretty
    std::string object;        // qdisc | class | filter
    std::string ifname;        // optional dev <if>
    std::string parent;        // optional parent <handle> (filter)
};

// ---------------------------------------------------------------------------
// Parsed record (one qdisc / class / filter)
// ---------------------------------------------------------------------------

struct TcEntry {
    int ifindex = 0;
    std::string kind;
    uint32_t handle = 0;
    uint32_t parent_handle = 0;
    uint32_t refcnt = 0;
    bool root = false;
    bool ingress = false;
    bool has_stats2 = false;
    uint64_t bytes = 0;
    uint64_t packets = 0;
    uint32_t drops = 0;
    uint32_t overlimits = 0;
    uint32_t requeues = 0;
    uint32_t qlen = 0;
    uint64_t backlog = 0;
    std::vector<uint8_t> options_raw; // raw TCA_OPTIONS payload, for -d decoding
    uint16_t filter_priority = 0;
    uint16_t filter_protocol = 0;
};

// ---------------------------------------------------------------------------
// rtattr parsing
// ---------------------------------------------------------------------------

static void parse_stats2(TcEntry* e, struct rtattr* rta) {
    e->has_stats2 = true;
    int len = RTA_PAYLOAD(rta);
    struct rtattr* sub = reinterpret_cast<struct rtattr*>(RTA_DATA(rta));
    for (; RTA_OK(sub, len); sub = RTA_NEXT(sub, len)) {
        const void* d = RTA_DATA(sub);
        switch (sub->rta_type) {
            case TCA_STATS_BASIC: {
                const auto* b = reinterpret_cast<const struct gnet_stats_basic*>(d);
                e->bytes = b->bytes;
                e->packets = b->packets;
                break;
            }
            case TCA_STATS_QUEUE: {
                const auto* q = reinterpret_cast<const struct gnet_stats_queue*>(d);
                e->qlen = q->qlen;
                e->backlog = q->backlog;
                e->drops = q->drops;
                e->requeues = q->requeues;
                e->overlimits = q->overlimits;
                break;
            }
            default:
                break;
        }
    }
}

static void parse_rtattr(TcEntry* e, struct rtattr* rta) {
    switch (rta->rta_type) {
        case TCA_KIND:
            e->kind = std::string(reinterpret_cast<const char*>(RTA_DATA(rta)));
            break;
        case TCA_OPTIONS: {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(RTA_DATA(rta));
            size_t const n = RTA_PAYLOAD(rta);
            e->options_raw.assign(p, p + n);
            break;
        }
        case TCA_STATS2:
            parse_stats2(e, rta);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Netlink dump
// ---------------------------------------------------------------------------

static int nl_dump(int type, std::vector<TcEntry>& out) {
    int const fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        (void)fprintf(stderr, "tc: cannot open netlink socket: %s\n", strerror(errno));
        return 1;
    }
    struct sockaddr_nl addr{};
    addr.nl_family = AF_NETLINK;
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        (void)fprintf(stderr, "tc: cannot bind netlink socket: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    char buf[8192];
    struct nlmsghdr* nh = reinterpret_cast<struct nlmsghdr*>(buf);
    nh->nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    nh->nlmsg_type = static_cast<uint16_t>(type);
    nh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nh->nlmsg_seq = 1;
    struct tcmsg* tc = reinterpret_cast<struct tcmsg*>(NLMSG_DATA(nh));
    memset(tc, 0, sizeof(*tc));
    tc->tcm_family = AF_UNSPEC;

    if (send(fd, buf, nh->nlmsg_len, 0) < 0) {
        (void)fprintf(stderr, "tc: netlink send failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    bool done = false;
    while (!done) {
        ssize_t len = recv(fd, buf, sizeof(buf), 0);
        if (len < 0) {
            (void)fprintf(stderr, "tc: netlink recv failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        struct nlmsghdr* cur = reinterpret_cast<struct nlmsghdr*>(buf);
        for (; NLMSG_OK(cur, len); cur = NLMSG_NEXT(cur, len)) {
            if (cur->nlmsg_type == NLMSG_DONE) { done = true; break; }
            if (cur->nlmsg_type == NLMSG_ERROR) {
                close(fd);
                return 1;
            }
            struct tcmsg* m = reinterpret_cast<struct tcmsg*>(NLMSG_DATA(cur));
            TcEntry e;
            e.ifindex = m->tcm_ifindex;
            e.handle = m->tcm_handle;
            e.parent_handle = m->tcm_parent;
            e.refcnt = m->tcm_info;  // tcm_info carries refcnt in dumps
            e.root = (m->tcm_parent == 0xFFFFFFFFU);
            e.ingress = (m->tcm_parent == 0xFFFFFFF1U);

            int rta_len = TCA_PAYLOAD(cur);
            struct rtattr* rta = TCA_RTA(m);
            for (; RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
                parse_rtattr(&e, rta);
            }
            out.push_back(std::move(e));
        }
    }
    close(fd);
    return 0;
}

// ---------------------------------------------------------------------------
// Handle / parent formatting (matches upstream tc)
// ---------------------------------------------------------------------------

static void format_handle(char* out, size_t n, uint32_t h) {
    // major:minor hex; a zero handle is "0:", and a zero minor is omitted
    // (e.g. 0x00010000 -> "1:"), matching upstream tc formatting.
    if (h == 0) {
        (void)snprintf(out, n, "0:");
        return;
    }
    unsigned const major = (h >> 16) & 0xFFFF;
    unsigned const minor = h & 0xFFFF;
    if (minor == 0) { (void)snprintf(out, n, "%x:", major);
    } else { (void)snprintf(out, n, "%x:%x", major, minor);
}
}

static const char* parent_word(uint32_t parent) {
    if (parent == 0xFFFFFFFFU) { return "root";
}
    if (parent == 0xFFFFFFF1U) { return "ingress";
}
    return nullptr; // has a real parent handle
}

// ---------------------------------------------------------------------------
// -d detail decoding for header-available qdiscs
// ---------------------------------------------------------------------------

static std::string decode_details(const TcEntry& e, const TcOptions* opts) {
    if (!opts->show_details) { return "";
}
    if (e.kind == "pfifo" || e.kind == "bfifo") {
        if (e.options_raw.size() >= sizeof(struct tc_fifo_qopt)) {
            const auto* q = reinterpret_cast<const struct tc_fifo_qopt*>(e.options_raw.data());
            char s[64];
            (void)snprintf(s, sizeof(s), "limit %up", q->limit);
            return s;
        }
    }
    if (e.kind == "prio" && e.options_raw.size() >= sizeof(struct tc_prio_qopt)) {
        const auto* q = reinterpret_cast<const struct tc_prio_qopt*>(e.options_raw.data());
        char s[64];
        (void)snprintf(s, sizeof(s), "bands %u priomap 0x", q->bands);
        for (int i = 0; i < TC_PRIO_MAX; i++) { (void)snprintf(s + strlen(s), 64 - strlen(s), "%x", q->priomap[i]);
}
        return s;
    }
    // Unknown kind: show the raw options as hex so -d still reveals the payload.
    if (!e.options_raw.empty()) {
        char hx[3];
        std::string s = "options ";
        for (size_t i = 0; i < e.options_raw.size(); i++) {
            (void)snprintf(hx, sizeof(hx), "%02x", e.options_raw[i]);
            s += hx;
        }
        return s;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Output — text
// ---------------------------------------------------------------------------

static void print_text(const std::vector<TcEntry>& entries, const TcOptions* opts) {
    char hbuf[16];
    char pbuf[16];
    for (const auto& e : entries) {
        format_handle(hbuf, sizeof(hbuf), e.handle);
        const char* pw = parent_word(e.parent_handle);
        char dev[IF_NAMESIZE] = "?";
        if_indextoname(e.ifindex, dev);

        printf("%s %s %s dev %s ",
               opts->object.c_str(), e.kind.c_str(), hbuf, dev);
        if (pw != nullptr) { { printf("%s ", pw);
        } } else {
            format_handle(pbuf, sizeof(pbuf), e.parent_handle);
            printf("parent %s ", pbuf);
        }
        // refcnt is meaningful for qdiscs/classes; for filters tcm_info packs
        // protocol+priority, so upstream omits refcnt there.
        if (opts->object != "filter") { printf("refcnt %u", e.refcnt);
}

        if (opts->object == "filter") {
            printf(" protocol %04x", e.filter_protocol);
            printf(" pref %u", e.filter_priority);
        }
        printf("\n");

        if (opts->show_details) {
            std::string const d = decode_details(e, opts);
            if (!d.empty()) { printf("\t%s\n", d.c_str());
}
        }

        if (opts->show_stats && e.has_stats2) {
            printf(" Sent %llu bytes %llu pkt (dropped %u, overlimits %u requeues %u) \n",
                   static_cast<unsigned long long>(e.bytes), static_cast<unsigned long long>(e.packets),
                   e.drops, e.overlimits, e.requeues);
            printf(" backlog %llub %up requeues %u\n",
                   static_cast<unsigned long long>(e.backlog), e.qlen, e.requeues);
        }
    }
}

// ---------------------------------------------------------------------------
// Output — JSON
// ---------------------------------------------------------------------------

static void print_json(const std::vector<TcEntry>& entries, const TcOptions* opts) {
    const char* ind = opts->pretty ? "  " : "";
    const char* nl = opts->pretty ? "\n" : "";
    printf("[%s", nl);
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& e = entries[i];
        char dev[IF_NAMESIZE] = "?";
        if_indextoname(e.ifindex, dev);
        char hbuf[16];
        format_handle(hbuf, sizeof(hbuf), e.handle);

        printf("%s{%s", ind, nl);
        bool first = true;
        auto sep = [&]() { if (!first) { printf(", "); 
}first = false; };
        sep(); json_emit_str(stdout, "kind", e.kind.c_str(), true);
        sep(); json_emit_str(stdout, "handle", hbuf, true);
        sep(); json_emit_str(stdout, "dev", dev, true);
        const char* pw = parent_word(e.parent_handle);
        if (pw != nullptr) { sep(); json_emit_str(stdout, "parent", pw, true); }
        else {
            char pbuf[16];
            format_handle(pbuf, sizeof(pbuf), e.parent_handle);
            sep(); json_emit_str(stdout, "parent", pbuf, true);
        }
        if (opts->object != "filter") {
            sep(); json_emit_int(stdout, "refcnt", e.refcnt, true);
        }
        if (opts->object == "filter") {
            sep(); json_emit_int(stdout, "protocol", e.filter_protocol, true);
            sep(); json_emit_int(stdout, "pref", e.filter_priority, true);
        }
        if (opts->show_stats && e.has_stats2) {
            sep();
            printf("%s\"stats\": {%s", ind, nl);
            json_emit_uint64(stdout, "bytes", e.bytes, true); printf(", ");
            json_emit_uint64(stdout, "packets", e.packets, true); printf(", ");
            json_emit_int(stdout, "drops", e.drops, true); printf(", ");
            json_emit_int(stdout, "overlimits", e.overlimits, true); printf(", ");
            json_emit_int(stdout, "requeues", e.requeues, true); printf(", ");
            json_emit_uint64(stdout, "backlog", e.backlog, true); printf(", ");
            json_emit_int(stdout, "qlen", e.qlen, true);
            printf("%s}", ind);
        }
        printf("%s}%s", nl, (i + 1 < entries.size()) ? "," : "");
        printf("%s", nl);
    }
    printf("]%s", nl);
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

static void print_help(const char* prog) {
    printf("Usage: %s [OPTIONS] OBJECT { show } [ OPTIONS ]\n", prog);
    printf("       %s [ -s | -d ] [ -json [ -pretty ] ] qdisc [ show ] [ dev NAME ]\n", prog);
    printf("       %s [ -s | -d ] [ -json [ -pretty ] ] class [ show ] [ dev NAME ]\n", prog);
    printf("       %s [ -s | -d ] [ -json [ -pretty ] ] filter [ show ] [ dev NAME ] [ parent HANDLE ]\n", prog);
    printf("\nOBJECT := { qdisc | class | filter }\n");
    printf("\nOPTIONS:\n");
    printf("  -s, --stats     print statistics\n");
    printf("  -d, --details   print detailed parameters\n");
    printf("  -json           output in JSON format\n");
    printf("  -pretty         indent JSON output (only with -json)\n");
    printf("  -h, --help      display this help and exit\n");
    printf("  --version       output version information and exit\n");
}

// ---------------------------------------------------------------------------
// Command
// ---------------------------------------------------------------------------

int tc_command(int argc, char** argv) {
    TcOptions opts;

    // Manual parsing: tc has a subcommand grammar ("tc qdisc show dev lo")
    // that argtable3 cannot model, so we parse flags ourselves and collect the
    // positional tokens for subcommand handling (mirroring ip.cpp).
    std::vector<std::string> pos;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { print_help(argv[0]); return 0; }
        if (a == "--version") { print_version("tc"); return 0; }
        if (a == "-s" || a == "--stats" || a == "-stats") { opts.show_stats = true; continue; }
        if (a == "-d" || a == "--details" || a == "-details") { opts.show_details = true; continue; }
        if (a == "-json") { opts.json_mode = true; continue; }
        if (a == "-pretty") { opts.pretty = true; continue; }
        if (!a.empty() && a[0] == '-') {
            (void)fprintf(stderr, "tc: unrecognized option '%s'\n", a.c_str());
            return 1;
        }
        pos.push_back(a);
    }

    if (pos.empty()) {
        (void)fprintf(stderr, "tc: need an object: qdisc, class, or filter\n");
        (void)fprintf(stderr, "Try 'tc --help' for more information.\n");
        return 1;
    }

    opts.object = pos[0];
    if (opts.object != "qdisc" && opts.object != "class" && opts.object != "filter") {
        (void)fprintf(stderr, "tc: unknown object \"%s\". Use: qdisc, class, filter\n", opts.object.c_str());
        return 1;
    }

    for (size_t i = 1; i < pos.size(); i++) {
        const std::string& a = pos[i];
        if (a == "show" || a == "list") { continue;
}
        if (a == "help") { print_help(argv[0]); return 0; }
        if (a == "dev" && i + 1 < pos.size()) { opts.ifname = pos[++i]; continue; }
        if (a == "parent" && i + 1 < pos.size()) { opts.parent = pos[++i]; continue; }
        (void)fprintf(stderr, "tc: unexpected argument '%s'\n", a.c_str());
        return 1;
    }

    // Resolve the device name up front so an unknown device is a clear error
    // (spec §19) rather than silently empty output.
    if (!opts.ifname.empty() && if_nametoindex(opts.ifname.c_str()) == 0) {
        (void)fprintf(stderr, "tc: %s: no such device\n", opts.ifname.c_str());
        return 1;
    }

    int const nl_type = (opts.object == "qdisc") ? RTM_GETQDISC
               : (opts.object == "class") ? RTM_GETTCLASS
               : RTM_GETTFILTER;

    std::vector<TcEntry> entries;
    if (nl_dump(nl_type, entries) != 0) { return 1;
}

    // Deterministic order: by ifindex, then handle.
    std::sort(entries.begin(), entries.end(),
              [](const TcEntry& a, const TcEntry& b) {
                  if (a.ifindex != b.ifindex) { return a.ifindex < b.ifindex;
}
                  return a.handle < b.handle;
              });

    std::vector<TcEntry> filtered;
    for (auto& e : entries) {
        char dev[IF_NAMESIZE] = "?";
        if_indextoname(e.ifindex, dev);
        if (!opts.ifname.empty() && opts.ifname != dev) { continue;
}
        if (!opts.parent.empty()) {
            if (opts.parent == "root") {
                if (!e.root) { continue;
}
            } else {
                char pbuf[16];
                format_handle(pbuf, sizeof(pbuf), e.parent_handle);
                if (pbuf != opts.parent) { continue;
}
            }
        }
        filtered.push_back(std::move(e));
    }

    if (opts.json_mode) {
        print_json(filtered, &opts);
    } else {
        print_text(filtered, &opts);
    }
    return 0;
}

REGISTER_COMMAND("tc", tc_command, "Show traffic control (qdisc/class/filter) state");
