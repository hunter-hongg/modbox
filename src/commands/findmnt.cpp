// findmnt — query the kernel mount table.
//
// Data source: /proc/self/mountinfo (preferred, carries the mount ID / parent
// ID relationship needed for tree rendering) with a fallback to /proc/mounts.
// Read-only: no new dependency, no libmount, no privileges required.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <argtable3.h>

#include "commands/findmnt.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

namespace {

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct FindmntOptions {
    bool no_headings = false;    // -n / --noheadings
    bool list_mode = false;      // -l / --list
    bool raw = false;            // -r / --raw
    bool pairs = false;          // -P / --pairs
    bool json_mode = false;      // -J / --json
    bool canonicalize = false;   // -c / --canonicalize
    bool invert = false;         // -v / --invert
    bool submounts = false;      // -R / --submounts
    bool all = false;            // -a / --all (explicit; all-by-default)
    std::vector<std::string> types;      // -t / --types (repeatable)
    std::vector<std::string> columns;    // -o / --output
    std::string source_filter;           // -S / --source
    std::string target_filter;           // -T / --target
    bool has_source_filter = false;
    bool has_target_filter = false;
};

// ---------------------------------------------------------------------------
// Mount entry
// ---------------------------------------------------------------------------

struct MountEntry {
    std::string target;
    std::string source;
    std::string fstype;
    std::string options;
    std::string maj_min;
    std::string root;
    unsigned long id = 0;
    unsigned long parent = 0;
};

// Canonical column identity, resolved from a user-supplied name.
enum class Column {
    Target,
    Source,
    Fstype,
    Options,
    MajMin,
    Root,
    Id,
    Parent,
};

const char* column_header(Column c) {
    switch (c) {
        case Column::Target:   return "TARGET";
        case Column::Source:   return "SOURCE";
        case Column::Fstype:   return "FSTYPE";
        case Column::Options:  return "OPTIONS";
        case Column::MajMin:   return "MAJ:MIN";
        case Column::Root:     return "ROOT";
        case Column::Id:       return "ID";
        case Column::Parent:   return "PARENT";
    }
    return "";
}

const char* column_json_key(Column c) {
    switch (c) {
        case Column::Target:   return "target";
        case Column::Source:   return "source";
        case Column::Fstype:   return "fstype";
        case Column::Options:  return "options";
        case Column::MajMin:   return "maj:min";
        case Column::Root:     return "root";
        case Column::Id:       return "id";
        case Column::Parent:   return "parent";
    }
    return "";
}

std::string uppercase(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

// Resolve a user column name (case-insensitive, with util-linux's single-letter
// abbreviations) to a canonical Column. Returns false when unrecognized.
bool resolve_column(const std::string& name, Column* out) {
    const std::string up = uppercase(name);
    if (up == "TARGET" || up == "T") { *out = Column::Target; return true; }
    if (up == "SOURCE" || up == "S") { *out = Column::Source; return true; }
    if (up == "FSTYPE" || up == "F") { *out = Column::Fstype; return true; }
    if (up == "OPTIONS" || up == "O") { *out = Column::Options; return true; }
    if (up == "MAJ:MIN") { *out = Column::MajMin; return true; }
    if (up == "ROOT") { *out = Column::Root; return true; }
    if (up == "ID") { *out = Column::Id; return true; }
    if (up == "PARENT") { *out = Column::Parent; return true; }
    return false;
}

std::string column_value(const MountEntry& e, Column c) {
    switch (c) {
        case Column::Target:  return e.target;
        case Column::Source:  return e.source;
        case Column::Fstype:  return e.fstype;
        case Column::Options: return e.options;
        case Column::MajMin:  return e.maj_min;
        case Column::Root:    return e.root;
        case Column::Id:      return std::to_string(e.id);
        case Column::Parent:  return std::to_string(e.parent);
    }
    return "";
}

// ---------------------------------------------------------------------------
// Reading the mount table
// ---------------------------------------------------------------------------

// Split a mountinfo optional-field (e.g. "rw,relatime shared:5") into the
// option list and the trailing propagation tag.
bool is_propagation_tag(const std::string& tok) {
    return tok.starts_with("shared:") || tok.starts_with("master:")
        || tok.starts_with("propagate_from:") || tok == "unbindable";
}

// Parse a /proc/self/mountinfo line. Returns false on a malformed line.
bool parse_mountinfo_line(const std::string& line, MountEntry* out) {
    std::istringstream iss(line);
    std::string id_tok;
    std::string parent_tok;
    std::string majmin;
    if (!(iss >> id_tok >> parent_tok >> majmin >> out->root >> out->target)) {
        return false;
    }

    char* end = nullptr;
    out->id = std::strtoul(id_tok.c_str(), &end, 10);
    if (end == id_tok.c_str()) { return false; }
    out->parent = std::strtoul(parent_tok.c_str(), &end, 10);
    if (end == parent_tok.c_str()) { return false; }
    out->maj_min = majmin;

    // Optional fields, terminated by " - ". A literal "-" followed by the fstype
    // is the separator; treat any field whose value is exactly "-" as the marker.
    std::string tok;
    std::string per_mount;
    while (iss >> tok) {
        if (tok == "-") { break; }
        if (is_propagation_tag(tok)) { continue; }
        per_mount = tok;
    }
    if (tok != "-") { return false; }  // no separator found → malformed

    if (!(iss >> out->fstype >> out->source)) { return false; }

    std::string super_opts;
    if (iss >> super_opts) {
        out->options = per_mount.empty() ? super_opts : per_mount + "," + super_opts;
    } else {
        out->options = per_mount;
    }
    return true;
}

// Fallback parser for /proc/mounts (6 fields, no IDs). IDs are synthesized
// sequentially so tree rendering still works.
bool parse_mounts_line(const std::string& line, MountEntry* out, unsigned long seq) {
    std::istringstream iss(line);
    std::string dump;
    std::string passno;
    if (!(iss >> out->source >> out->target >> out->fstype >> out->options)) {
        return false;
    }
    (void)(iss >> dump >> passno);
    out->id = seq;
    out->parent = (out->target == "/") ? 0UL : 1UL;
    if (out->target == "/") { out->parent = 0UL; }
    return true;
}

std::vector<MountEntry> read_mount_table(bool* used_fallback) {
    std::vector<MountEntry> entries;
    *used_fallback = false;

    // Allow the path to be overridden so tests can inject a deterministic
    // fixture (mirrors MODBOX_SYSFS / MODBOX_IDS_DIR in lspci/lsusb).
    const char* override = std::getenv("MODBOX_MOUNTINFO");
    std::string const mountinfo = (override != nullptr) ? override : "/proc/self/mountinfo";

    std::ifstream f(mountinfo);
    if (f) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) { continue; }
            MountEntry e;
            if (parse_mountinfo_line(line, &e)) {
                entries.push_back(std::move(e));
            }
        }
        if (!entries.empty()) { return entries; }
    }

    // Fallback: /proc/mounts.
    *used_fallback = true;
    std::ifstream m("/proc/mounts");
    if (!m) { return entries; }
    std::string line;
    unsigned long seq = 1;
    while (std::getline(m, line)) {
        if (line.empty()) { continue; }
        MountEntry e;
        if (parse_mounts_line(line, &e, seq)) {
            entries.push_back(std::move(e));
            seq++;
        }
    }
    return entries;
}

// ---------------------------------------------------------------------------
// Parsing helpers for option arguments
// ---------------------------------------------------------------------------

void split_commas(const std::string& s, std::vector<std::string>* out) {
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t const comma = s.find(',', pos);
        std::string const tok = (comma == std::string::npos)
            ? s.substr(pos)
            : s.substr(pos, comma - pos);
        if (!tok.empty()) { out->push_back(tok); }
        if (comma == std::string::npos) { break; }
        pos = comma + 1;
    }
}

// A target filter matches the exact mount point, or (for a path query) mounts
// beneath it. A string-prefix comparison is deliberately NOT used: it would
// wrongly nest /varfoo under /var.
bool target_matches(const MountEntry& e, const std::string& want, bool submounts_only) {
    if (e.target == want) { return true; }
    if (want == "/") { return submounts_only ? (e.target != "/") : true; }
    if (e.target.size() > want.size()
        && e.target.starts_with(want)
        && e.target[want.size()] == '/') {
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Tree ordering
// ---------------------------------------------------------------------------

// A tree is stored as index relationships rather than nested copies: nesting
// node structs by value would drop children attached to a node after it was
// copied into its parent.
struct MountTree {
    std::vector<std::vector<size_t>> children;  // per-entry child indices
    std::vector<size_t> roots;
};

// Build the mount hierarchy from parent/child IDs. Entries whose parent is not
// present in the set become roots, preserving input order. Orphaned entries
// (e.g. from the /proc/mounts fallback) are always attached as roots.
void build_tree(const std::vector<MountEntry>& entries, MountTree* tree) {
    const size_t n = entries.size();
    tree->children.assign(n, {});
    std::vector<bool> has_parent(n, false);

    for (size_t i = 0; i < n; i++) {
        const unsigned long pid = entries[i].parent;
        if (pid == 0 || pid == entries[i].id) { continue; }
        for (size_t j = 0; j < n; j++) {
            if (entries[j].id == pid) {
                tree->children[j].push_back(i);
                has_parent[i] = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < n; i++) {
        if (!has_parent[i]) { tree->roots.push_back(i); }
    }
}

// Flatten the tree in depth-first order, recording each node's depth together
// with whether it is the last child of its parent (used to draw tree glyphs).
struct FlatNode {
    size_t index = 0;
    int depth = 0;
    bool last = true;
};

void flatten(const MountTree& tree, std::vector<FlatNode>* out) {
    struct StackItem {
        size_t index;
        int depth;
        bool last;
    };
    // Roots are pushed in reverse so the first root is processed first.
    std::vector<StackItem> stack;
    for (size_t r = tree.roots.size(); r > 0; r--) {
        stack.push_back({.index = tree.roots[r - 1], .depth = 0, .last = r == 1});
    }
    while (!stack.empty()) {
        const StackItem item = stack.back();
        stack.pop_back();
        out->push_back({.index = item.index, .depth = item.depth, .last = item.last});

        const std::vector<size_t>& kids = tree.children[item.index];
        for (size_t k = kids.size(); k > 0; k--) {
            stack.push_back({.index = kids[k - 1], .depth = item.depth + 1, .last = k == 1});
        }
    }
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

std::string pad(const std::string& s, size_t width) {
    if (s.size() >= width) { return s; }
    return s + std::string(width - s.size(), ' ');
}

// The tree prefix for a row: indentation plus the branch glyphs that connect
// it to its ancestors. The root level carries no glyph.
std::string tree_prefix(const std::vector<FlatNode>& rows, size_t r) {
    const FlatNode& node = rows[r];
    if (node.depth == 0) { return ""; }

    // Determine, for each ancestor level, whether that ancestor still has a
    // following sibling (and therefore needs a vertical continuation bar).
    std::vector<bool> ancestor_last(static_cast<size_t>(node.depth), true);
    for (int depth = node.depth - 1; depth >= 0; depth--) {
        for (size_t p = r; p > 0; p--) {
            if (rows[p - 1].depth == depth) {
                ancestor_last[static_cast<size_t>(depth)] = rows[p - 1].last;
                break;
            }
        }
    }

    // Emit from the outermost level inward: ancestors contribute either a bar
    // or blank space, and the node's own level contributes the branch glyph.
    std::string prefix;
    for (int depth = 0; depth < node.depth; depth++) {
        prefix += ancestor_last[static_cast<size_t>(depth)] ? "   " : "\u2502  ";
    }
    prefix += node.last ? "\u2514\u2500 " : "\u251c\u2500 ";
    return prefix;
}

void print_table(const std::vector<FlatNode>& rows,
                 const std::vector<MountEntry>& entries,
                 const std::vector<Column>& cols,
                 const FindmntOptions* opts) {
    if (cols.empty()) { return; }

    // Compute per-column width from the visible (prefixed) first-column text.
    std::vector<size_t> widths(cols.size(), 0);
    std::vector<std::string> first_cells;
    first_cells.reserve(rows.size());

    for (size_t r = 0; r < rows.size(); r++) {
        const MountEntry& e = entries[rows[r].index];
        std::string const first = tree_prefix(rows, r) + column_value(e, cols[0]);
        first_cells.push_back(first);
        widths[0] = std::max(widths[0], first.size());
    }
    if (!opts->no_headings) {
        widths[0] = std::max(widths[0], std::string(column_header(cols[0])).size());
    }

    for (size_t c = 1; c < cols.size(); c++) {
        widths[c] = std::string(column_header(cols[c])).size();
        for (const auto& row : rows) {
            widths[c] = std::max(widths[c],
                                 column_value(entries[row.index], cols[c]).size());
        }
    }

    if (!opts->no_headings) {
        for (size_t c = 0; c < cols.size(); c++) {
            printf("%s", pad(column_header(cols[c]), widths[c]).c_str());
            if (c + 1 < cols.size()) { printf(" "); }
        }
        printf("\n");
    }

    for (size_t r = 0; r < rows.size(); r++) {
        const MountEntry& e = entries[rows[r].index];
        for (size_t c = 0; c < cols.size(); c++) {
            std::string const cell = (c == 0) ? first_cells[r]
                                              : column_value(e, cols[c]);
            printf("%s", pad(cell, widths[c]).c_str());
            if (c + 1 < cols.size()) { printf(" "); }
        }
        printf("\n");
    }
}

void print_pairs(const std::vector<FlatNode>& rows,
                 const std::vector<MountEntry>& entries,
                 const std::vector<Column>& cols,
                 bool uppercase_keys) {
    for (const auto& row : rows) {
        const MountEntry& e = entries[row.index];
        for (size_t c = 0; c < cols.size(); c++) {
            std::string key = column_header(cols[c]);
            key = uppercase_keys ? key : std::string(column_json_key(cols[c]));
            printf("%s=", key.c_str());
            json_escape_string(stdout, column_value(e, cols[c]).c_str());
            if (c + 1 < cols.size()) { printf(" "); }
        }
        printf("\n");
    }
}

void print_canonical(const std::vector<FlatNode>& rows,
                     const std::vector<MountEntry>& entries) {
    for (const auto& row : rows) {
        const MountEntry& e = entries[row.index];
        printf("%s %s %s\n", e.target.c_str(), e.source.c_str(), e.fstype.c_str());
    }
}

void print_json(const std::vector<FlatNode>& rows,
                const std::vector<MountEntry>& entries,
                const std::vector<Column>& cols) {
    printf("[");
    for (size_t r = 0; r < rows.size(); r++) {
        const MountEntry& e = entries[rows[r].index];
        printf("%s{", (r == 0) ? "" : ",");
        for (size_t c = 0; c < cols.size(); c++) {
            if (c > 0) { printf(", "); }
            json_emit_str(stdout, column_json_key(cols[c]),
                          column_value(e, cols[c]).c_str(), true);
        }
        printf("}");
    }
    printf("]\n");
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

void print_help(const char* prog) {
    printf("Usage: %s [options] [device|mountpoint]\n", prog);
    printf("\n");
    printf("Find a (mounted) filesystem, or list all mounted filesystems.\n");
    printf("Reads /proc/self/mountinfo (falling back to /proc/mounts).\n");
    printf("\n");
    printf("  -a, --all                list all filesystems (default)\n");
    printf("  -c, --canonicalize       print (target, source, fstype) per line\n");
    printf("  -J, --json               use JSON output format\n");
    printf("  -l, --list               print a flat list instead of a tree\n");
    printf("  -n, --noheadings         don't print a column header\n");
    printf("  -o, --output <list>      output columns (default: TARGET,SOURCE,FSTYPE,OPTIONS)\n");
    printf("  -P, --pairs              print KEY=\"value\" pairs\n");
    printf("  -r, --raw                print lowercase key=\"value\" pairs\n");
    printf("  -R, --submounts          print only the mount and its submounts\n");
    printf("  -S, --source <value>     filter by source device\n");
    printf("  -T, --target <value>     filter by mount point\n");
    printf("  -t, --types <list>       limit to the listed filesystem types\n");
    printf("  -v, --invert             invert the --types filter\n");
    printf("  -V, --version            output version information and exit\n");
    printf("      --help               display this help and exit\n");
    printf("\n");
    printf("Available columns: TARGET, SOURCE, FSTYPE, OPTIONS, MAJ:MIN, ROOT, ID, PARENT\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                       list all mounts as a tree\n", prog);
    printf("  %s /var                  show the mount for /var and its submounts\n", prog);
    printf("  %s -t ext4 -o TARGET,SOURCE\n", prog);
    printf("  %s -J | modbox jq '.length'\n", prog);
}

int exit_usage(const char* prog, const std::string& msg) {
    (void)fprintf(stderr, "%s: %s\n", prog, msg.c_str());
    (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    return 2;
}

// ---------------------------------------------------------------------------
// Command
// ---------------------------------------------------------------------------

// Apply the --types / --source / --target filters, keeping entries that pass.
std::vector<MountEntry> apply_filters(const std::vector<MountEntry>& entries,
                                      const FindmntOptions& opts) {
    std::vector<MountEntry> selected;
    for (const auto& e : entries) {
        if (!opts.types.empty()) {
            bool const in_types = std::ranges::find(opts.types, e.fstype) != opts.types.end();
            if (opts.invert ? in_types : !in_types) { continue; }
        }
        if (opts.has_source_filter && e.source != opts.source_filter) { continue; }
        if (opts.has_target_filter
            && !target_matches(e, opts.target_filter, false)) { continue; }
        selected.push_back(e);
    }
    return selected;
}

// A positional argument behaves like --target when it looks like a path,
// otherwise like --source (spec §25/§9).
std::vector<MountEntry> apply_positional(const std::vector<MountEntry>& selected,
                                         const std::string& positional) {
    if (positional.empty()) { return selected; }
    std::vector<MountEntry> narrowed;
    const bool by_path = positional.starts_with("/");
    for (const auto& e : selected) {
        if (by_path ? target_matches(e, positional, false) : e.source == positional) {
            narrowed.push_back(e);
        }
    }
    return narrowed;
}

// Resolve --output column names, defaulting to the util-linux column set.
// Returns false (after reporting) when a name is unknown.
bool resolve_columns(const FindmntOptions& opts, const char* prog,
                     std::vector<Column>* cols) {
    if (opts.columns.empty()) {
        *cols = {Column::Target, Column::Source, Column::Fstype, Column::Options};
        return true;
    }
    for (const auto& name : opts.columns) {
        Column c;
        if (!resolve_column(name, &c)) {
            exit_usage(prog, "unknown column '" + name + "'");
            return false;
        }
        cols->push_back(c);
    }
    return true;
}

int run_findmnt(const FindmntOptions& opts, const char* prog,
                const std::vector<std::string>& positionals) {
    bool used_fallback = false;
    std::vector<MountEntry> const entries = read_mount_table(&used_fallback);
    if (entries.empty()) {
        (void)fprintf(stderr, "%s: can't read mount table\n", prog);
        return 1;
    }

    std::vector<MountEntry> selected = apply_filters(entries, opts);

    std::string positional;
    if (!positionals.empty()) { positional = positionals[0]; }
    selected = apply_positional(selected, positional);

    // -R / --submounts on a target query drops the mount itself, leaving only
    // the mounts beneath it.
    if (opts.submounts && positional.starts_with("/") && positional != "/") {
        std::vector<MountEntry> narrowed;
        for (const auto& e : selected) {
            if (e.target != positional) { narrowed.push_back(e); }
        }
        selected = std::move(narrowed);
    }

    std::vector<Column> cols;
    if (!resolve_columns(opts, prog, &cols)) { return 2; }

    // No match is a clean, quiet non-zero exit (spec §25) so shell
    // conditionals work, and canonicalize prints its own field set.
    if (selected.empty()) { return 1; }

    // --- ordering ----------------------------------------------------------
    std::vector<FlatNode> rows;
    if (opts.list_mode || opts.canonicalize) {
        for (size_t i = 0; i < selected.size(); i++) {
            rows.push_back({.index = i, .depth = 0, .last = true});
        }
    } else {
        MountTree tree;
        build_tree(selected, &tree);
        flatten(tree, &rows);
        // Defensive: never drop an entry if the ID graph was inconsistent.
        if (rows.size() != selected.size()) {
            rows.clear();
            for (size_t i = 0; i < selected.size(); i++) {
                rows.push_back({.index = i, .depth = 0, .last = true});
            }
        }
    }

    // --- render ------------------------------------------------------------
    if (opts.canonicalize) {
        print_canonical(rows, selected);
    } else if (opts.json_mode) {
        print_json(rows, selected, cols);
    } else if (opts.pairs) {
        print_pairs(rows, selected, cols, true);
    } else if (opts.raw) {
        print_pairs(rows, selected, cols, false);
    } else {
        print_table(rows, selected, cols, &opts);
    }
    return 0;
}

}  // namespace

int findmnt_command(int argc, char** argv) {
    FindmntOptions opts;

    struct arg_lit* opt_all = arg_lit0("a", "all", "list all filesystems (default)");
    struct arg_lit* opt_canon = arg_lit0("c", "canonicalize", "print (target, source, fstype)");
    struct arg_lit* opt_json = arg_lit0("J", "json", "use JSON output format");
    struct arg_lit* opt_list = arg_lit0("l", "list", "print a flat list");
    struct arg_lit* opt_nohead = arg_lit0("n", "noheadings", "don't print a column header");
    struct arg_str* opt_output = arg_strn("o", "output", "<list>", 0, 32, "output columns");
    struct arg_lit* opt_pairs = arg_lit0("P", "pairs", "print KEY=\"value\" pairs");
    struct arg_lit* opt_raw = arg_lit0("r", "raw", "print lowercase key=\"value\" pairs");
    struct arg_lit* opt_sub = arg_lit0("R", "submounts", "print the mount and its submounts");
    struct arg_str* opt_source = arg_str0("S", "source", "<value>", "filter by source device");
    struct arg_str* opt_target = arg_str0("T", "target", "<value>", "filter by mount point");
    struct arg_str* opt_types = arg_strn("t", "types", "<list>", 0, 32, "limit to filesystem types");
    struct arg_lit* opt_invert = arg_lit0("v", "invert", "invert the --types filter");
    struct arg_lit* opt_fstab = arg_lit0("F", "fstab", "search in /etc/fstab (unsupported)");
    struct arg_lit* opt_version = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* opt_help = arg_lit0(nullptr, "help", "display this help and exit");
    struct arg_str* opt_pos = arg_strn(nullptr, nullptr, "<device|mountpoint>", 0, 1,
                                       "device or mountpoint to search for");
    struct arg_end* end = arg_end(20);

    ArgTable table({opt_all, opt_canon, opt_json, opt_list, opt_nohead, opt_output,
                    opt_pairs, opt_raw, opt_sub, opt_source, opt_target, opt_types,
                    opt_invert, opt_fstab, opt_version, opt_help, opt_pos, end});

    int const nerrors = table.parse(argc, argv);
    if (nerrors != 0) {
        return print_arg_errors(end, "findmnt");
    }

    if (opt_help->count > 0) { print_help("findmnt"); return 0; }
    if (opt_version->count > 0) { print_version("findmnt"); return 0; }

    // /etc/fstab parsing is explicitly out of scope (spec Out of Scope).
    if (opt_fstab->count > 0) {
        return exit_usage("findmnt", "--fstab is not supported");
    }

    opts.all = opt_all->count > 0;
    opts.canonicalize = opt_canon->count > 0;
    opts.json_mode = opt_json->count > 0;
    opts.list_mode = opt_list->count > 0;
    opts.no_headings = opt_nohead->count > 0;
    opts.pairs = opt_pairs->count > 0;
    opts.raw = opt_raw->count > 0;
    opts.submounts = opt_sub->count > 0;
    opts.invert = opt_invert->count > 0;

    if (opt_source->count > 0) {
        opts.source_filter = opt_source->sval[0];
        opts.has_source_filter = true;
    }
    if (opt_target->count > 0) {
        opts.target_filter = opt_target->sval[0];
        opts.has_target_filter = true;
    }
    for (int i = 0; i < opt_types->count; i++) {
        split_commas(opt_types->sval[i], &opts.types);
    }
    for (int i = 0; i < opt_output->count; i++) {
        split_commas(opt_output->sval[i], &opts.columns);
    }

    std::vector<std::string> positionals;
    for (int i = 0; i < opt_pos->count; i++) {
        positionals.emplace_back(opt_pos->sval[i]);
    }

    // A positional argument alongside an explicit --source/--target is
    // ambiguous, matching util-linux's rejection.
    if (!positionals.empty() && (opts.has_source_filter || opts.has_target_filter)) {
        return exit_usage("findmnt",
                          "cannot combine a positional argument with --source/--target");
    }

    if (!opts.columns.empty()) {
        for (const auto& name : opts.columns) {
            Column c;
            if (!resolve_column(name, &c)) {
                return exit_usage("findmnt", "unknown column '" + name + "'");
            }
        }
    }

    // --raw and --pairs are mutually redundant; last-wins is the util-linux
    // behaviour, so make --raw the winner only when it was given last is not
    // observable here — prefer an explicit error to avoid silent surprise.
    if (opts.raw && opts.pairs) {
        return exit_usage("findmnt", "--raw and --pairs are mutually exclusive");
    }

    return run_findmnt(opts, "findmnt", positionals);
}

REGISTER_COMMAND("findmnt", findmnt_command, "Find a filesystem by device or mount point");
