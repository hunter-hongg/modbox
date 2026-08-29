#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <algorithm>
#include <functional>

#include <string>
#include <unordered_map>
#include <vector>

#include <argtable3.h>

#include "commands/pstree.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/cmd_error.hpp"

namespace {

struct ProcInfo {
    pid_t pid = 0;
    pid_t ppid = 0;
    uid_t uid = 0;          // effective uid (for -u)
    char comm[256] = {0};   // short name from /proc/<pid>/stat
    char cmd[4096] = {0};   // full command line from /proc/<pid>/cmdline
};

// Read /proc/<pid>/stat, /proc/<pid>/status, /proc/<pid>/cmdline.
// Returns false if the process is not readable.
bool read_proc(pid_t pid, ProcInfo& info) {
    memset(&info, 0, sizeof(info));
    info.pid = pid;

    char path[256];
    FILE* fp = nullptr;

    // --- stat: (comm) ppid uid... ---
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) return false;

    char stat_buf[4096];
    bool ok = false;
    if (fgets(stat_buf, sizeof(stat_buf), fp)) {
        const char* p = stat_buf;
        while (*p && *p != ' ') p++;       // skip pid
        while (*p == ' ') p++;
        if (*p == '(') {
            p++;
            const char* cstart = p;
            while (*p && *p != ')') p++;
            size_t clen = (size_t)(p - cstart);
            if (clen < sizeof(info.comm) - 1) {
                memcpy(info.comm, cstart, clen);
                info.comm[clen] = '\0';
            }
            p++;  // skip ')'
        }
        while (*p == ' ') p++;
        // state ppid pgrp session ... -> we only need ppid.
        int parsed = sscanf(p, "%*c %d", &info.ppid);
        ok = (parsed >= 1);
    }
    fclose(fp);
    if (!ok) return false;

    // --- status: effective uid (Uid: line, 2nd field) ---
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                int r = 0, e = 0, s = 0, f = 0;
                if (sscanf(line, "Uid: %d %d %d %d", &r, &e, &s, &f) >= 2) {
                    info.uid = (uid_t)e;
                }
                break;
            }
        }
        fclose(fp);
    }

    // --- cmdline: NUL-separated args ---
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    fp = fopen(path, "r");
    if (fp) {
        size_t n = fread(info.cmd, 1, sizeof(info.cmd) - 1, fp);
        fclose(fp);
        if (n > 0) {
            for (size_t i = 0; i < n; i++) {
                if (info.cmd[i] == '\0') info.cmd[i] = ' ';
            }
            if (info.cmd[n - 1] == ' ') info.cmd[n - 1] = '\0';
        } else {
            // Kernel thread / no cmdline -> fall back to comm.
            strncpy(info.cmd, info.comm, sizeof(info.cmd) - 1);
            info.cmd[sizeof(info.cmd) - 1] = '\0';
        }
    } else {
        strncpy(info.cmd, info.comm, sizeof(info.cmd) - 1);
        info.cmd[sizeof(info.cmd) - 1] = '\0';
    }

    return true;
}

const char* username_for(uid_t uid, char* buf, size_t buflen) {
    struct passwd* pw = getpwuid(uid);
    if (pw) return pw->pw_name;
    snprintf(buf, buflen, "%u", (unsigned)uid);
    return buf;
}
std::string username_of(uid_t uid) {
    char buf[256];
    return username_for(uid, buf, sizeof(buf));
}

// Print one line of the tree. `spacer` is the leading whitespace already
// excluding the connector; `top` suppresses the connector for root nodes of a
// displayed tree (so they start at column 0).
void print_node(const ProcInfo& info,
                const std::string& spacer,
                bool top,
                bool last,
                bool show_pid,
                bool show_args,
                bool show_user,
                const std::string& parent_user) {
    std::string line = spacer;
    if (!top) line += last ? "`----" : "|----";

    line += info.comm;
    if (show_pid) {
        char pbuf[32];
        snprintf(pbuf, sizeof(pbuf), "(%d)", info.pid);
        line += pbuf;
    }
    if (show_user) {
        char ubuf[256];
        const char* uname = username_for(info.uid, ubuf, sizeof(ubuf));
        if (uname != parent_user) {
            line += "(";
            line += uname;
            line += ")";
        }
    }
    if (show_args && info.cmd[0] != '\0') {
        const char* args = info.cmd;
        // cmdline echoes the command name as its first token; skip it so we
        // don't print the name twice (e.g. "sleep sleep 120").
        size_t clen = strlen(info.comm);
        if (strncmp(info.cmd, info.comm, clen) == 0 && info.cmd[clen] == ' ') {
            args = info.cmd + clen + 1;
        }
        if (*args != '\0') {
            line += " ";
            line += args;
        }
    }
    printf("%s\n", line.c_str());
}

}  // namespace

int pstree_command(int argc, char** argv) {
    const char* prog = argv[0];

    struct arg_lit* p_opt = arg_lit0("p", "show-pids", "show PIDs in parentheses");
    struct arg_lit* a_opt = arg_lit0("a", "args", "show command line arguments");
    struct arg_lit* u_opt = arg_lit0("u", "uid-info", "show uid transitions");
    struct arg_lit* s_opt = arg_lit0("s", "show-parents", "show parents of the selected process");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_str* pid_args = arg_strn(nullptr, nullptr, "<pid>", 0, 64, "root PID(s) of the subtree(s)");
    struct arg_end* end = arg_end(20);

    ArgTable at({p_opt, a_opt, u_opt, s_opt, help_opt, version_opt, pid_args, end});
    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [PID]...\n", prog);
        printf("Display a tree of processes.\n");
        printf("\n");
        printf("  -a, --args              show command line arguments\n");
        printf("  -h, --help              display this help and exit\n");
        printf("  -p, --show-pids         show PIDs in parentheses\n");
        printf("  -s, --show-parents      show parents of the selected process\n");
        printf("  -u, --uid-info          show uid transitions\n");
        printf("  -V, --version           output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("pstree");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, prog);
    }

    bool show_pid = p_opt->count > 0;
    bool show_args = a_opt->count > 0;
    bool show_user = u_opt->count > 0;
    bool show_parents = s_opt->count > 0;

    // Parse requested PID roots (ascending, de-duplicated).
    std::vector<pid_t> roots;
    for (int i = 0; i < pid_args->count; i++) {
        pid_t pid = (pid_t)atoi(pid_args->sval[i]);
        if (pid > 0) roots.push_back(pid);
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

    if (show_parents && roots.empty()) {
        return cmd_error(prog, "-s requires a PID argument");
    }

    // Read all processes from /proc.
    std::unordered_map<pid_t, ProcInfo> procs;
    std::unordered_map<pid_t, std::vector<pid_t>> children;

    DIR* dir = opendir("/proc");
    if (!dir) {
        return cmd_error(prog, "cannot open /proc");
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        bool is_num = true;
        for (const char* p = entry->d_name; *p; p++) {
            if (!isdigit((unsigned char)*p)) { is_num = false; break; }
        }
        if (!is_num) continue;
        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid <= 0) continue;
        ProcInfo info;
        if (!read_proc(pid, info)) continue;
        procs[pid] = info;
        children[info.ppid].push_back(pid);
    }
    // Determine the roots to start each displayed tree from.
    // - No PID given: each process whose parent is unknown/0 (forest roots).
    // - PID(s) given: those PIDs. With -s, each root is preceded by its
    //   ancestor chain (deepest first), and we only descend into that root's
    //   own subtree (not the whole forest).
    std::vector<pid_t> display_roots;
    if (roots.empty()) {
        for (const auto& kv : procs) {
            if (procs.find(kv.second.ppid) == procs.end()) {
                display_roots.push_back(kv.first);
            }
        }
        std::sort(display_roots.begin(), display_roots.end());
    } else if (!show_parents) {
        display_roots = roots;
    }

    // Sort children by PID for deterministic output.
    for (auto& kv : children) {
        std::sort(kv.second.begin(), kv.second.end());
    }

    // Emit a subtree rooted at `pid`. `top` is true for the displayed tree's
    // root (no connector, no leading spacer). Each level appends 5 columns:
    // either "|    " to continue a branch or "     " to end it, before the
    // next child's connector.
    std::function<void(pid_t, const std::string&, bool, bool, const std::string&)> emit =
        [&](pid_t pid, const std::string& spacer, bool top, bool last,
            const std::string& parent_user) {
            auto it = procs.find(pid);
            if (it == procs.end()) return;
            const ProcInfo& info = it->second;
            print_node(info, spacer, top, last, show_pid, show_args, show_user, parent_user);

            char ubuf[256];
            const char* uname = username_for(info.uid, ubuf, sizeof(ubuf));

            std::string child_spacer = spacer;
            if (!top) child_spacer += last ? "     " : "|    ";
            const auto& ch = children[pid];
            for (size_t i = 0; i < ch.size(); i++) {
                emit(ch[i], child_spacer, false, (i + 1 == ch.size()), uname);
            }
        };

    if (!roots.empty() && show_parents) {
        // For each requested PID: print the ancestor chain init..target as a
        // single linear path (each node descends only into the next chain
        // element), then emit the target's real subtree below it.
        for (size_t ri = 0; ri < roots.size(); ri++) {
            pid_t target = roots[ri];
            std::vector<pid_t> chain;
            pid_t cur = target;
            while (cur > 0 && procs.find(cur) != procs.end()) {
                chain.push_back(cur);
                cur = procs[cur].ppid;
            }
            std::reverse(chain.begin(), chain.end());  // init -> target

            // Print the chain as consecutive single-child links. A chain node
            // (except the last) is the only child shown for its parent, so it
            // is always `last` and its spacer gains "     " before the next
            // level's connector.
            std::string spacer;
            for (size_t i = 0; i < chain.size(); i++) {
                bool top = (i == 0);
                bool last = (i + 1 == chain.size());
                const std::string& parent_user =
                    (i == 0) ? std::string() : username_of(procs[chain[i - 1]].uid);
                print_node(procs[chain[i]], spacer, top, last,
                           show_pid, show_args, show_user, parent_user);
                if (!top) spacer += "     ";  // node is sole child -> pad
            }
            // Emit the target's real subtree (all its children) under it.
            const auto& ch = children[target];
            std::string child_spacer = spacer + "|    ";
            for (size_t i = 0; i < ch.size(); i++) {
                emit(ch[i], child_spacer, false, (i + 1 == ch.size()),
                     username_of(procs[target].uid));
            }
        }
    } else {
        for (size_t i = 0; i < display_roots.size(); i++) {
            emit(display_roots[i], "", true, (i + 1 == display_roots.size()), "");
        }
    }

    return 0;
}

REGISTER_COMMAND("pstree", pstree_command, "Display a tree of processes");
