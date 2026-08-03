#include "commands/audit2allow.hpp"

#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <set>
#include <tuple>
#include <vector>
#include <unistd.h>
#include <libgen.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

// ── Data structures ──────────────────────────────────────────────────────────

struct AvcDenial {
    std::string scontext;
    std::string tcontext;
    std::string tclass;
    std::vector<std::string> perms;
    std::string comm;
    std::string raw_line;
    // Extracted short types from contexts
    std::string source_type;
    std::string target_type;
};

// Key for deduplicating rules
struct RuleKey {
    std::string source_type;
    std::string target_type;
    std::string tclass;
    std::string perms; // sorted, semicolon-separated

    bool operator<(const RuleKey& o) const {
        return std::tie(source_type, target_type, tclass, perms) <
               std::tie(o.source_type, o.target_type, o.tclass, o.perms);
    }
    bool operator==(const RuleKey& o) const {
        return std::tie(source_type, target_type, tclass, perms) ==
               std::tie(o.source_type, o.target_type, o.tclass, o.perms);
    }
};

// Accumulated rule: key -> rule data
struct Rule {
    RuleKey key;
};

// ── Options ──────────────────────────────────────────────────────────────────

struct Audit2AllowOptions {
    // Input source
    const char* input_file = nullptr;
    int read_all = 0;       // -a
    int read_boot = 0;      // -b
    int read_dmesg = 0;     // -d
    int read_lastreload = 0; // -l

    // Output
    const char* output_file = nullptr;
    const char* module_name = nullptr;
    int module_package = 0; // -M

    // Rule type
    int dontaudit = 0;     // -D
    int requires_only = 0; // -r
    int reference_style = 0; // -R (warn + fallback)
    int cil_output = 0;    // -C

    // Filtering
    const char* type_regex = nullptr; // -t

    // Explanation mode
    int why = 0;       // -w
    int explain = 0;   // -e
    int verbose = 0;   // -v (warning, no-op)

    // Unsupported / ignored
    const char* perm_map = nullptr;
    const char* interface_info = nullptr;
    int xperms = 0; // -x (warning, no-op)
    int noreference = 0; // -N (no-op)
};

// ── AVC Parsing ──────────────────────────────────────────────────────────────

// Extract a quoted string value like comm="httpd" or name="index.html"
static std::string extract_quoted(const std::string& line, const std::string& key) {
    std::string search = key + "=\"";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = line.find('"', pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

// Extract a key=value pair (unquoted)
static std::string extract_kv(const std::string& line, const std::string& key) {
    std::string search = key + "=";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = line.find(' ', pos);
    if (end == std::string::npos) end = line.find('\t', pos);
    if (end == std::string::npos) end = line.size();
    // Trim trailing commas or semicolons
    while (end > pos && (line[end - 1] == ',' || line[end - 1] == ';')) end--;
    return line.substr(pos, end - pos);
}

// Extract types from scontext=tuser:role:type:level format
static std::string extract_type_from_context(const std::string& ctx) {
    if (ctx.empty()) return "";
    std::istringstream ss(ctx);
    std::string token;
    int idx = 0;
    while (std::getline(ss, token, ':')) {
        if (idx == 2) return token; // type is the 3rd field (0=user, 1=role, 2=type)
        idx++;
    }
    return "";
}

// Parse perms from { perm1 perm2 ... } or a single perm
static std::vector<std::string> parse_perms(const std::string& perms_str) {
    std::vector<std::string> result;
    std::string s = perms_str;
    // Strip braces if present
    if (!s.empty() && s[0] == '{') {
        s = s.substr(1);
    }
    if (!s.empty() && s.back() == '}') {
        s.pop_back();
    }
    std::istringstream ss(s);
    std::string p;
    while (ss >> p) {
        if (!p.empty()) result.push_back(p);
    }
    // Sort and deduplicate
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

static std::string perms_to_string(const std::vector<std::string>& perms) {
    if (perms.empty()) return "";
    std::ostringstream oss;
    for (size_t i = 0; i < perms.size(); i++) {
        if (i > 0) oss << " ";
        oss << perms[i];
    }
    return oss.str();
}

// Try to parse a line as an AVC denial. Returns true on success.
static bool parse_avc_line(const std::string& line, AvcDenial& out) {
    out.raw_line = line;

    // Check if this looks like an AVC denial
    bool has_denied = line.find("denied") != std::string::npos;
    bool has_avc = line.find("avc:") != std::string::npos;
    if (!has_denied || !has_avc) return false;

    // Extract permissions
    std::string perms_str;
    {
        size_t pos = line.find("{");
        if (pos != std::string::npos) {
            size_t end = line.find("}", pos);
            if (end != std::string::npos) {
                perms_str = line.substr(pos, end - pos + 1);
            }
        }
    }
    out.perms = parse_perms(perms_str);
    if (out.perms.empty()) return false;

    // Extract tclass
    out.tclass = extract_kv(line, "tclass");
    if (out.tclass.empty()) return false;

    // Extract comm
    out.comm = extract_quoted(line, "comm");

    // Try full context format (scontext=... tcontext=...)
    out.scontext = extract_kv(line, "scontext");
    out.tcontext = extract_kv(line, "tcontext");

    // Try short context format (salabel=... tlabel=...)
    if (out.scontext.empty()) {
        out.scontext = extract_kv(line, "salabel");
    }
    if (out.tcontext.empty()) {
        out.tcontext = extract_kv(line, "tlabel");
    }

    // Extract source and target types from contexts
    out.source_type = extract_type_from_context(out.scontext);
    out.target_type = extract_type_from_context(out.tcontext);

    // If we couldn't get types from contexts, try srcname/source name
    if (out.source_type.empty()) {
        out.source_type = out.comm;
    }
    if (out.target_type.empty()) {
        // Try srcname or name
        std::string srcname = extract_quoted(line, "srcname");
        if (srcname.empty()) srcname = extract_quoted(line, "name");
        if (!srcname.empty()) {
            out.target_type = srcname;
        }
    }

    // If we still have no meaningful types, this line is not parseable
    if (out.source_type.empty() || out.target_type.empty()) {
        return false;
    }

    return true;
}

// ── Input reading ────────────────────────────────────────────────────────────

static std::vector<std::string> read_input(const Audit2AllowOptions* opts) {
    std::vector<std::string> lines;

    if (opts->read_dmesg) {
        // Read from dmesg
        FILE* fp = popen("dmesg 2>/dev/null", "r");
        if (!fp) {
            fprintf(stderr, "audit2allow: failed to run dmesg\n");
            exit(1);
        }
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            // Remove trailing newline
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty()) lines.push_back(line);
        }
        pclose(fp);
        return lines;
    }

    if (opts->input_file) {
        std::ifstream file(opts->input_file);
        if (!file.is_open()) {
            fprintf(stderr, "audit2allow: cannot open '%s': %s\n",
                    opts->input_file, strerror(errno));
            exit(1);
        }
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) lines.push_back(line);
        }
        return lines;
    }

    // Read from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

// ── Rule generation ──────────────────────────────────────────────────────────

static std::string perms_to_braced(const std::vector<std::string>& perms) {
    if (perms.size() == 1) return perms[0];
    std::ostringstream oss;
    oss << "{ ";
    for (size_t i = 0; i < perms.size(); i++) {
        if (i > 0) oss << " ";
        oss << perms[i];
    }
    oss << " }";
    return oss.str();
}

// Build a RuleKey from a denial
static RuleKey make_key(const AvcDenial& d) {
    RuleKey k;
    k.source_type = d.source_type;
    k.target_type = d.target_type;
    k.tclass = d.tclass;
    k.perms = perms_to_string(d.perms);
    return k;
}

// Group denials into rules, deduplicating by key
static std::vector<Rule> build_rules(const std::vector<AvcDenial>& denials) {
    std::map<RuleKey, Rule> rule_map;

    for (const auto& d : denials) {
        RuleKey k = make_key(d);
        auto it = rule_map.find(k);
        if (it == rule_map.end()) {
            Rule r;
            r.key = k;
            rule_map[k] = r;
        }
    }

    std::vector<Rule> result;
    for (auto& [k, r] : rule_map) {
        result.push_back(std::move(r));
    }
    return result;
}

// ── Output ───────────────────────────────────────────────────────────────────

// Key for grouping rules by (source_type, target_type, tclass)
struct GroupKey {
    std::string source_type;
    std::string target_type;
    std::string tclass;

    bool operator<(const GroupKey& o) const {
        return std::tie(source_type, target_type, tclass) <
               std::tie(o.source_type, o.target_type, o.tclass);
    }
    bool operator==(const GroupKey& o) const {
        return std::tie(source_type, target_type, tclass) ==
               std::tie(o.source_type, o.target_type, o.tclass);
    }
};

// Accumulated permissions for a group
struct RuleGroup {
    GroupKey key;
    std::set<std::string> perms;
};

static void emit_traditional(const std::vector<Rule>& rules, bool dontaudit, FILE* out) {
    const char* keyword = dontaudit ? "dontaudit" : "allow";

    // Group by (source_type, target_type, tclass) and merge perms
    std::map<GroupKey, RuleGroup> groups;
    for (const auto& rule : rules) {
        GroupKey gk;
        gk.source_type = rule.key.source_type;
        gk.target_type = rule.key.target_type;
        gk.tclass = rule.key.tclass;
        auto it = groups.find(gk);
        if (it == groups.end()) {
            RuleGroup rg;
            rg.key = gk;
            std::istringstream ss(rule.key.perms);
            std::string p;
            while (ss >> p) rg.perms.insert(p);
            groups[gk] = rg;
        } else {
            std::istringstream ss(rule.key.perms);
            std::string p;
            while (ss >> p) it->second.perms.insert(p);
        }
    }

    // Group by source_type for comm headers
    std::map<std::string, std::vector<const RuleGroup*>> by_source;
    for (const auto& [gk, rg] : groups) {
        by_source[gk.source_type].push_back(&rg);
    }

    bool first = true;
    for (const auto& [src, group] : by_source) {
        if (!first) fputs("\n", out);
        first = false;
        fprintf(out, "#============= %s ==============\n", src.c_str());
        for (const auto* rg : group) {
            fprintf(out, "%s %s %s:%s ",
                    keyword,
                    rg->key.source_type.c_str(),
                    rg->key.target_type.c_str(),
                    rg->key.tclass.c_str());
            if (rg->perms.size() == 1) {
                fprintf(out, "%s;\n", (*rg->perms.begin()).c_str());
            } else {
                fprintf(out, "{");
                for (const auto& p : rg->perms) {
                    fprintf(out, " %s", p.c_str());
                }
                fprintf(out, " };\n");
            }
        }
    }
    if (!groups.empty()) fputc('\n', out);
}

static void emit_require_block(const std::vector<Rule>& rules, FILE* out) {
    // Collect all unique types and all unique (class, perm) entries
    std::set<std::string> all_types;
    // class -> sorted unique perms
    std::map<std::string, std::set<std::string>> class_perms;

    for (const auto& rule : rules) {
        all_types.insert(rule.key.source_type);
        all_types.insert(rule.key.target_type);
        std::istringstream ss(rule.key.perms);
        std::string perm;
        while (ss >> perm) {
            class_perms[rule.key.tclass].insert(perm);
        }
    }

    fputs("require {\n", out);
    // Emit types first
    for (const auto& t : all_types) {
        fprintf(out, "\ttype %s;\n", t.c_str());
    }
    // Then emit class entries
    for (const auto& [cls, perms] : class_perms) {
        if (perms.size() == 1) {
            fprintf(out, "\tclass %s %s;\n", cls.c_str(), (*perms.begin()).c_str());
        } else {
            fprintf(out, "\tclass %s {", cls.c_str());
            for (const auto& p : perms) {
                fprintf(out, " %s", p.c_str());
            }
            fprintf(out, " };\n");
        }
    }
    fputs("}\n\n", out);
}

static void emit_module(const std::vector<Rule>& rules, const char* modname, bool dontaudit, FILE* out) {
    fprintf(out, "module %s 1.0;\n\n", modname);
    emit_require_block(rules, out);
    emit_traditional(rules, dontaudit, out);
}

static void emit_simple(const std::vector<Rule>& rules, bool dontaudit, FILE* out) {
    emit_traditional(rules, dontaudit, out);
}

static void emit_require_only(const std::vector<Rule>& rules, bool dontaudit, FILE* out) {
    emit_require_block(rules, out);
    emit_traditional(rules, dontaudit, out);
}

static void emit_why(const std::vector<AvcDenial>& denials, FILE* out) {
    for (size_t i = 0; i < denials.size(); i++) {
        const auto& d = denials[i];
        if (i > 0) fputc('\n', out);
        // Emit the original line as comment
        fprintf(out, "# %s\n", d.raw_line.c_str());
        fprintf(out, "    # comm=%s", d.comm.c_str());
        // Try to get name/srcname
        std::string name = extract_quoted(d.raw_line, "name");
        if (name.empty()) name = extract_quoted(d.raw_line, "srcname");
        if (!name.empty()) fprintf(out, "  name=%s", name.c_str());
        std::string dev = extract_kv(d.raw_line, "dev");
        if (!dev.empty()) fprintf(out, "  dev=%s", dev.c_str());
        std::string ino = extract_kv(d.raw_line, "ino");
        if (!ino.empty()) fprintf(out, "  ino=%s", ino.c_str());
        fputc('\n', out);
        if (!d.scontext.empty()) {
            fprintf(out, "    # source %s\n", d.scontext.c_str());
        }
        if (!d.tcontext.empty()) {
            fprintf(out, "    # target %s\n", d.tcontext.c_str());
        }
        fprintf(out, "    # known false positives: 0\n");
    }
    if (!denials.empty()) fputc('\n', out);
}

// ── Argument parsing ─────────────────────────────────────────────────────────

int audit2allow_command(int argc, char** argv) {
    struct arg_file* input_opt = arg_file0("i", "input", "INPUT", "read input from INPUT");
    struct arg_lit* read_all_opt = arg_lit0("a", "all", "read input from audit log");
    struct arg_lit* read_boot_opt = arg_lit0("b", "boot", "read input since last boot");
    struct arg_lit* read_dmesg_opt = arg_lit0("d", "dmesg", "read input from dmesg");
    struct arg_lit* read_last_opt = arg_lit0("l", "lastreload", "read input since last reload");
    struct arg_str* module_opt = arg_str0("m", "module", "MODULE", "set module name");
    struct arg_str* module_pkg_opt = arg_str0("M", "module-package", "MODULE_PACKAGE", "generate module package");
    struct arg_file* output_opt = arg_file0("o", "output", "OUTPUT", "append output to OUTPUT");
    struct arg_lit* dontaudit_opt = arg_lit0("D", "dontaudit", "generate dontaudit rules");
    struct arg_lit* requires_opt = arg_lit0("r", "requires", "generate require statements");
    struct arg_lit* reference_opt = arg_lit0("R", "reference", "generate refpolicy style output");
    struct arg_lit* cil_opt = arg_lit0("C", "cil", "generate CIL output");
    struct arg_lit* noreference_opt = arg_lit0("N", "noreference", "do not generate refpolicy style");
    struct arg_str* type_opt = arg_str0("t", "type", "TYPE", "only process messages matching TYPE");
    struct arg_lit* why_opt = arg_lit0("w", "why", "translate AVC messages to descriptions");
    struct arg_lit* explain_opt = arg_lit0("e", "explain", "fully explain generated output");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "explain generated output");
    struct arg_str* perm_map_opt = arg_str0(NULL, "perm-map", "PERM_MAP", "permission map file");
    struct arg_str* iface_info_opt = arg_str0(NULL, "interface-info", "INTERFACE_INFO", "interface info file");
    struct arg_lit* xperms_opt = arg_lit0("x", "xperms", "generate extended permission rules");
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_end* end = arg_end(20);

    ArgTable at({input_opt, read_all_opt, read_boot_opt, read_dmesg_opt,
                 read_last_opt, module_opt, module_pkg_opt, output_opt,
                 dontaudit_opt, requires_opt, reference_opt, cil_opt,
                 noreference_opt, type_opt, why_opt, explain_opt, verbose_opt,
                 perm_map_opt, iface_info_opt, xperms_opt, help_opt,
                 version_opt, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: audit2allow [options]\n");
        printf("\n");
        printf("  -a, --all              read input from audit log\n");
        printf("  -b, --boot             read input since last boot\n");
        printf("  -d, --dmesg            read input from dmesg\n");
        printf("  -i INPUT, --input=INPUT  read input from INPUT\n");
        printf("  -l, --lastreload       read input since last reload\n");
        printf("  -m MODULE, --module=MODULE  set module name\n");
        printf("  -M MODULE, --module-package=MODULE  generate module package\n");
        printf("  -o OUTPUT, --output=OUTPUT  append output to OUTPUT\n");
        printf("  -D, --dontaudit        generate dontaudit rules\n");
        printf("  -r, --requires         generate require statements\n");
        printf("  -R, --reference        generate refpolicy style output\n");
        printf("  -C, --cil              generate CIL output\n");
        printf("  -N, --noreference      do not generate refpolicy style output\n");
        printf("  -t TYPE, --type=TYPE   only process messages matching TYPE\n");
        printf("  -w, --why              translate AVC messages to descriptions\n");
        printf("  -e, --explain          fully explain generated output\n");
        printf("  -v, --verbose          explain generated output\n");
        printf("      --perm-map=PERM_MAP   permission map file\n");
        printf("      --interface-info=INTERFACE_INFO  interface info file\n");
        printf("  -x, --xperms           generate extended permission rules\n");
        printf("      --help             display this help and exit\n");
        printf("      --version          output version information and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        for (int i = 0; i < end->count; i++) {
            const char* argval = end->argval[i] ? end->argval[i] : "";
            if (end->error[i] == ARG_ELONGOPT) {
                fprintf(stderr, "audit2allow: unrecognized option '%s'\n", argval);
            } else {
                fprintf(stderr, "audit2allow: unexpected argument '%s'\n", argval);
            }
        }
        fprintf(stderr, "Try 'audit2allow --help' for more information.\n");
        return 1;
    }

    if (version_opt->count > 0) {
        print_version("audit2allow");
        return 0;
    }

    // Validate mutual exclusions
    int input_count = input_opt->count + read_all_opt->count +
                      read_boot_opt->count + read_dmesg_opt->count +
                      read_last_opt->count;
    if (input_count > 1) {
        fprintf(stderr, "audit2allow: conflicting input sources specified\n");
        return 1;
    }

    if (module_pkg_opt->count > 0 && (output_opt->count > 0 || module_opt->count > 0)) {
        fprintf(stderr, "audit2allow: --module-package conflicts with --output/--module\n");
        return 1;
    }

    // Build options
    Audit2AllowOptions opts;
    opts.input_file = (input_opt->count > 0) ? input_opt->filename[0] : nullptr;
    opts.read_all = read_all_opt->count > 0;
    opts.read_boot = read_boot_opt->count > 0;
    opts.read_dmesg = read_dmesg_opt->count > 0;
    opts.read_lastreload = read_last_opt->count > 0;
    opts.module_name = (module_opt->count > 0) ? module_opt->sval[0] : nullptr;
    opts.module_package = module_pkg_opt->count > 0;
    opts.output_file = (output_opt->count > 0) ? output_opt->filename[0] : nullptr;
    opts.dontaudit = dontaudit_opt->count > 0;
    opts.requires_only = requires_opt->count > 0;
    opts.reference_style = reference_opt->count > 0;
    opts.cil_output = cil_opt->count > 0;
    opts.type_regex = (type_opt->count > 0) ? type_opt->sval[0] : nullptr;
    opts.why = why_opt->count > 0;
    opts.explain = explain_opt->count > 0;
    opts.verbose = verbose_opt->count > 0;
    opts.perm_map = (perm_map_opt->count > 0) ? perm_map_opt->sval[0] : nullptr;
    opts.interface_info = (iface_info_opt->count > 0) ? iface_info_opt->sval[0] : nullptr;
    opts.xperms = xperms_opt->count > 0;
    opts.noreference = noreference_opt->count > 0;

    // Warn about unsupported/ignored flags
    if (opts.read_all || opts.read_boot || opts.read_lastreload) {
        fprintf(stderr, "audit2allow: libaudit not available\n");
        return 1;
    }
    if (opts.module_package) {
        fprintf(stderr, "audit2allow: --module-package is not supported\n");
        return 1;
    }
    if (opts.cil_output) {
        fprintf(stderr, "audit2allow: --cil is not supported\n");
        return 1;
    }
    if (opts.reference_style) {
        fprintf(stderr, "audit2allow: --reference is not fully supported, using traditional output\n");
    }
    if (opts.xperms) {
        fprintf(stderr, "audit2allow: --xperms is not supported, ignoring\n");
    }
    if (opts.perm_map) {
        fprintf(stderr, "audit2allow: --perm-map is not supported, ignoring\n");
    }
    if (opts.interface_info) {
        fprintf(stderr, "audit2allow: --interface-info is not supported, ignoring\n");
    }
    if (opts.explain) {
        // Fall back to why
        opts.why = 1;
    }
    if (opts.verbose) {
        fprintf(stderr, "audit2allow: --verbose is not supported, ignoring\n");
    }

    // Check input
    if (input_count == 0 && isatty(STDIN_FILENO)) {
        fprintf(stderr, "audit2allow: no input specified (use -i FILE or pipe input)\n");
        return 1;
    }

    // Read and parse input
    std::vector<std::string> lines = read_input(&opts);

    // Apply type filter
    std::regex type_re(opts.type_regex ? opts.type_regex : ".*");

    // Parse AVC denials
    std::vector<AvcDenial> denials;
    for (const auto& line : lines) {
        // Apply type filter
        std::smatch match;
        if (opts.type_regex && !std::regex_search(line, match, type_re)) {
            continue;
        }

        AvcDenial d;
        if (parse_avc_line(line, d)) {
            denials.push_back(std::move(d));
        }
    }

    // Build rules
    std::vector<Rule> rules = build_rules(denials);

    // Open output
    FILE* out = stdout;
    FILE* outfile_handle = nullptr;
    if (opts.output_file) {
        outfile_handle = fopen(opts.output_file, "ab");
        if (!outfile_handle) {
            fprintf(stderr, "audit2allow: cannot open '%s': %s\n",
                    opts.output_file, strerror(errno));
            return 1;
        }
        out = outfile_handle;
    }

    // Emit output
    if (opts.why) {
        emit_why(denials, out);
    } else if (opts.module_name) {
        emit_module(rules, opts.module_name, opts.dontaudit, out);
    } else if (opts.requires_only) {
        emit_require_only(rules, opts.dontaudit, out);
    } else {
        emit_simple(rules, opts.dontaudit, out);
    }

    if (opts.output_file) {
        if (outfile_handle) fclose(outfile_handle);
    }

    return 0;
}

REGISTER_COMMAND("audit2allow", audit2allow_command,
                 "Generate SELinux policy allow rules from audit logs");
