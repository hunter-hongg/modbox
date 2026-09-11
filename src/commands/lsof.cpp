#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <argtable3.h>

#include "commands/lsof.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------

struct LsofEntry {
    pid_t  pid      = 0;
    char   comm[256] = {};
    char   user[64]  = {};
    char   fd[32]    = {};
    char   type[16]  = {};
    char   device[32] = {};
    char   size_off[32] = {};
    char   node[32]   = {};
    char   name[1024] = {};
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string get_username(uid_t uid) {
    const struct passwd* pw = getpwuid(uid);
    if (pw != nullptr) { return std::string(pw->pw_name);
}
    char buf[16];
    (void)snprintf(buf, sizeof(buf), "%u", uid);
    return buf;
}

static bool read_pid_uid(pid_t pid, uid_t& out_uid) {
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (f == nullptr) { return false;
}
    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), f) != nullptr) {
        if (strncmp(line, "Uid:", 4) == 0) {
            int r;
            int e;
            int s;
            int ff;
            if (sscanf(line, "Uid: %d %d %d %d", &r, &e, &s, &ff) >= 1) {
                out_uid = static_cast<uid_t>(e);
                found = true;
            }
            break;
        }
    }
    (void)fclose(f);
    return found;
}

static std::string read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == nullptr) { return {};
}
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    (void)fclose(f);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) { buf[--n] = '\0';
}
    return std::string(buf);
}

// Extract a value from fdinfo for a given key (e.g. "ino", "flags", "pos").
static std::string read_fdinfo_value(pid_t pid, int fd, const char* key) {
    char path[128];
    (void)snprintf(path, sizeof(path), "/proc/%d/fdinfo/%d", pid, fd);
    std::string content = read_file(path);
    std::string needle = key;
    needle += "\t";
    size_t const pos = content.find(needle);
    if (pos == std::string::npos) { return {};
}
    size_t val_start = pos + needle.size();
    while (val_start < content.size() && content[val_start] == ' ') { val_start++;
}
    size_t val_end = content.find('\n', val_start);
    if (val_end == std::string::npos) { val_end = content.size();
}
    return content.substr(val_start, val_end - val_start);
}

// Infer the lsof TYPE string from the symlink target.
static void infer_type(const std::string& target, char* type_str, size_t ts_size) {
    if (target.find("socket:[") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "SOCK");
    } else if (target.find("pipe:[") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "PIPE");
    } else if (target.find("anon_inode:[eventpoll]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("anon_inode:[timerfd]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("anon_inode:[eventfd]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("anon_inode:inotify") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("anon_inode:[io_uring]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("anon_inode:[signalfd]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("anon_inode:[pidfd]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "UNK");
    } else if (target.find("[anon") != std::string::npos ||
               target.find("[heap]") != std::string::npos ||
               target.find("[stack]") != std::string::npos ||
               target.find("[vvar]") != std::string::npos ||
               target.find("[vsyscall]") != std::string::npos) {
        (void)snprintf(type_str, ts_size, "MEM");
    } else if (target.starts_with("memfd:")) {
        (void)snprintf(type_str, ts_size, "REG");
    } else if (target.starts_with("file:")) {
        (void)snprintf(type_str, ts_size, "REG");
    } else if (target.starts_with("/dev/")) {
        (void)snprintf(type_str, ts_size, "CHR");
    } else if (target.empty() || target == "(deleted)") {
        (void)snprintf(type_str, ts_size, "UNK");
    } else {
        (void)snprintf(type_str, ts_size, "REG");
    }
}

// Check whether the fd target matches a special /proc symlink.
// Returns true if a special token was assigned.
static bool check_special_fd(pid_t pid, int fd, const std::string& target,
                              char* out_token, size_t token_size,
                              char* out_type, size_t type_size) {
    char path[128];
    char buf[2048];
    ssize_t slen;

    // cwd
    (void)snprintf(path, sizeof(path), "/proc/%d/cwd", pid);
    slen = readlink(path, buf, sizeof(buf) - 1);
    if (slen > 0) {
        buf[slen] = '\0';
        if (target == buf) {
            (void)snprintf(out_token, token_size, "cwd");
            (void)snprintf(out_type, type_size, "DIR");
            return true;
        }
    }

    // root
    (void)snprintf(path, sizeof(path), "/proc/%d/root", pid);
    slen = readlink(path, buf, sizeof(buf) - 1);
    if (slen > 0) {
        buf[slen] = '\0';
        if (target == buf) {
            (void)snprintf(out_token, token_size, "rtd");
            (void)snprintf(out_type, type_size, "DIR");
            return true;
        }
    }

    // exe
    (void)snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    slen = readlink(path, buf, sizeof(buf) - 1);
    if (slen > 0) {
        buf[slen] = '\0';
        if (target == buf) {
            (void)snprintf(out_token, token_size, "txt");
            (void)snprintf(out_type, type_size, "REG");
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Socket resolution from /proc/net
// ---------------------------------------------------------------------------

struct SocketEntry {
    uint64_t inode;
    std::string local_addr;
    int local_port;
    std::string remote_addr;
    int remote_port;
};

static std::unordered_map<uint64_t, SocketEntry> g_socket_cache;
static bool g_socket_cache_valid = false;

static void rebuild_socket_cache() {
    g_socket_cache.clear();
    g_socket_cache_valid = true;

    // /proc/net/tcp format (space-separated):
    // sl local_address rem_address st tx_queue rx_queue tr tm->when retrnsmt uid timeout inode ...
    // Fields (1-indexed): 1=sl, 2=local_addr, 3=rem_addr, 4=state, 5-9=queues, 10=inode
    const char* net_files[] = {"/proc/net/tcp", "/proc/net/tcp6", "/proc/net/udp", "/proc/net/udp6", nullptr};
    for (int fi = 0; net_files[fi] != nullptr; fi++) {
        FILE* f = fopen(net_files[fi], "r");
        if (f == nullptr) { continue;
}
        char line[512];
        bool header = true;
        while (fgets(line, sizeof(line), f) != nullptr) {
            if (header) { header = false; continue; }

            // Extract tokens manually
            char tokens[20][64];
            int argc = 0;
            const char* p = line;
            while (((*p) != 0) && argc < 20) {
                while (*p == ' ' || *p == '\t') { p++;
}
                if ((*p) == 0) { break;
}
                const char* start = p;
                while (((*p) != 0) && *p != ' ' && *p != '\t') { p++;
}
                int len = p - start;
                if (len >= 64) { len = 63;
}
                strncpy(tokens[argc], start, len);
                tokens[argc][len] = '\0';
                argc++;
            }
            if (argc < 10) { continue;
}

            // tokens[1] = local_address (HEX_IP:HEX_PORT)
            // tokens[9] = inode (decimal)
            uint64_t const inode_val = strtoull(tokens[9], nullptr, 10);
            if (inode_val == 0) { continue;
}

            // Parse local_address
            char* colon = strchr(tokens[1], ':');
            if (colon == nullptr) { continue;
}
            *colon = '\0';

            uint32_t hex_ip = 0;
            uint32_t hex_port = 0;
            if (sscanf(tokens[1], "%08x", &hex_ip) != 1) { continue;
}
            if (sscanf(colon + 1, "%x", &hex_port) != 1) { continue;
}

            SocketEntry se;
            se.inode = inode_val;
            se.local_port = hex_port;
            se.local_addr = std::to_string(hex_ip & 0xFF) + "." +
                            std::to_string((hex_ip >> 8) & 0xFF) + "." +
                            std::to_string((hex_ip >> 16) & 0xFF) + "." +
                            std::to_string((hex_ip >> 24) & 0xFF);

            // Parse remote address if present
            if (argc > 3) {
                char* rcolon = strchr(tokens[3], ':');
                if (rcolon != nullptr) {
                    *rcolon = '\0';
                    uint32_t rip = 0;
                    uint32_t rp = 0;
                    if (sscanf(tokens[3], "%08x", &rip) == 1) {
                        se.remote_addr = std::to_string(rip & 0xFF) + "." +
                                         std::to_string((rip >> 8) & 0xFF) + "." +
                                         std::to_string((rip >> 16) & 0xFF) + "." +
                                         std::to_string((rip >> 24) & 0xFF);
                    }
                    if (sscanf(rcolon + 1, "%x", &rp) == 1) { se.remote_port = rp;
}
                }
            }
            g_socket_cache[inode_val] = se;
        }
        (void)fclose(f);
    }

    // Also parse /proc/net/unix for Unix domain sockets
    {
        FILE* f = fopen("/proc/net/unix", "r");
        if (f != nullptr) {
            char line[512];
            bool header = true;
            while (fgets(line, sizeof(line), f) != nullptr) {
                if (header) { header = false; continue; }
                char tokens[12][128];
                int argc = 0;
                const char* p = line;
                while (((*p) != 0) && argc < 12) {
                    while (*p == ' ' || *p == '\t') { p++;
}
                    if ((*p) == 0) { break;
}
                    const char* start = p;
                    while (((*p) != 0) && *p != ' ' && *p != '\t') { p++;
}
                    int len = p - start;
                    if (len >= 128) { len = 127;
}
                    strncpy(tokens[argc], start, len);
                    tokens[argc][len] = '\0';
                    argc++;
                }
                // unix format: state refcount type flags path inode ...
                // We need the inode (typically token 6 or later, hex)
                if (argc < 7) { continue;
}
                // Find the hex inode — it's usually near the end
                for (int i = argc - 1; i >= 0; i--) {
                    char* endptr;
                    uint64_t const val = strtoull(tokens[i], &endptr, 16);
                    if (endptr != tokens[i] && *endptr == '\0' && val > 0) {
                        SocketEntry se;
                        se.inode = val;
                        se.local_addr = (argc > 5) ? tokens[5] : "";
                        se.local_port = 0;
                        g_socket_cache[val] = se;
                        break;
                    }
                }
            }
            (void)fclose(f);
        }
    }
}

static std::string resolve_socket_name(uint64_t inode) {
    if (!g_socket_cache_valid) { rebuild_socket_cache();
}
    auto it = g_socket_cache.find(inode);
    if (it == g_socket_cache.end()) { return "";
}
    const auto& se = it->second;
    char buf[256];
    (void)snprintf(buf, sizeof(buf), "%s:%d", se.local_addr.c_str(), se.local_port);
    return std::string(buf);
}

// Extract inode number from a socket:[...] target string.
static uint64_t extract_socket_inode(const std::string& target) {
    size_t const start = target.find('[');
    size_t const end = target.find(']');
    if (start == std::string::npos || end == std::string::npos) { return 0;
}
    std::string const num_str = target.substr(start + 1, end - start - 1);
    try {
        return std::stoull(num_str, nullptr, 10);
    } catch (...) {
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

static std::vector<LsofEntry> read_proc_fd(pid_t pid) {
    std::vector<LsofEntry> entries;

    // Read comm
    char comm[256] = {};
    {
        char comm_path[64];
        (void)snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        FILE* f = fopen(comm_path, "r");
        if (f != nullptr) {
            if (fgets(comm, sizeof(comm), f) != nullptr) {
                size_t cl = strlen(comm);
                while (cl > 0 && (comm[cl-1] == '\n' || comm[cl-1] == '\r')) { comm[--cl] = '\0';
}
            }
            (void)fclose(f);
        }
    }
    if (comm[0] == '\0') { strcpy(comm, "?");
}

    // Read UID
    uid_t uid = 0;
    read_pid_uid(pid, uid);
    std::string const username = get_username(uid);

    // Iterate /proc/<pid>/fd/
    char fd_dir[128];
    (void)snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
    DIR* fd_dirh = opendir(fd_dir);
    if (fd_dirh == nullptr) { return entries;
}

    struct dirent* de;
    while ((de = readdir(fd_dirh)) != nullptr) {
        bool is_num = true;
        for (const char* p = de->d_name; (*p) != 0; p++) {
            if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
        }
        if (!is_num) { continue;
}
        int const fd = atoi(de->d_name);

        LsofEntry e;
        e.pid = pid;
        strncpy(e.comm, comm, sizeof(e.comm) - 1);
        e.comm[sizeof(e.comm) - 1] = '\0';
        strncpy(e.user, username.c_str(), sizeof(e.user) - 1);
        e.user[sizeof(e.user) - 1] = '\0';

        // Read symlink target
        char link_path[128];
        (void)snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%d", pid, fd);
        char target_buf[2048];
        ssize_t const tlen = readlink(link_path, target_buf, sizeof(target_buf) - 1);
        if (tlen < 0) { continue;
}
        target_buf[tlen] = '\0';
        std::string const target(target_buf);

        // Skip the fd directory itself (opened internally by opendir)
        char self_fd_path[256];
        (void)snprintf(self_fd_path, sizeof(self_fd_path), "/proc/%d/fd", pid);
        if (target == self_fd_path) { continue;
}

        // Determine FD token and type
        char fd_token[32] = {};
        char type_str[16] = {};
        if (!check_special_fd(pid, fd, target, fd_token, sizeof(fd_token), type_str, sizeof(type_str))) {
            (void)snprintf(fd_token, sizeof(fd_token), "%d", fd);
            infer_type(target, type_str, sizeof(type_str));
        }
        strncpy(e.fd, fd_token, sizeof(e.fd) - 1);
        e.fd[sizeof(e.fd) - 1] = '\0';
        strncpy(e.type, type_str, sizeof(e.type) - 1);
        e.type[sizeof(e.type) - 1] = '\0';

        // Device: mnt_id from fdinfo
        std::string const mnt_id = read_fdinfo_value(pid, fd, "mnt_id");
        if (!mnt_id.empty()) {
            (void)snprintf(e.device, sizeof(e.device), "%s", mnt_id.c_str());
        }

        // Node: inode from fdinfo
        std::string const ino = read_fdinfo_value(pid, fd, "ino");
        if (!ino.empty()) {
            (void)snprintf(e.node, sizeof(e.node), "%s", ino.c_str());
        }

        // Size/off: pos from fdinfo
        std::string const pos = read_fdinfo_value(pid, fd, "pos");
        if (!pos.empty()) {
            (void)snprintf(e.size_off, sizeof(e.size_off), "%s", pos.c_str());
        }

        // For socket entries, resolve the address:port if possible
        if (std::string(e.type) == "SOCK") {
            uint64_t const inode = extract_socket_inode(target);
            if (inode > 0) {
                std::string const resolved = resolve_socket_name(inode);
                if (!resolved.empty()) {
                    strncpy(e.name, resolved.c_str(), sizeof(e.name) - 1);
                    e.name[sizeof(e.name) - 1] = '\0';
                } else {
                    strncpy(e.name, target.c_str(), sizeof(e.name) - 1);
                    e.name[sizeof(e.name) - 1] = '\0';
                }
            } else {
                strncpy(e.name, target.c_str(), sizeof(e.name) - 1);
                e.name[sizeof(e.name) - 1] = '\0';
            }
        } else {
            strncpy(e.name, target.c_str(), sizeof(e.name) - 1);
            e.name[sizeof(e.name) - 1] = '\0';
        }

        entries.push_back(e);
    }
    closedir(fd_dirh);
    return entries;
}

// ---------------------------------------------------------------------------
// Filter helpers
// ---------------------------------------------------------------------------

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) { c = tolower(static_cast<unsigned char>(c));
}
    return r;
}

static std::string normalize_type(const std::string& t) {
    std::string lt = to_lower(t);
    if (lt == "regular" || lt == "reg") { return "reg";
}
    if (lt == "socket" || lt == "sock") { return "sock";
}
    if (lt == "directory" || lt == "dir") { return "dir";
}
    if (lt == "network" || lt == "net") {  return "net";
}
    // "pipe" and "fifo" are synonyms — normalize to "pipe" since we use PIPE as the type string
    if (lt == "pipe" || lt == "fifo") { return "pipe";
}
    return lt;
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

int lsof_command(int argc, char** argv) {
    struct arg_lit*  help_opt     = arg_lit0("h", "help",    "display this help and exit");
    struct arg_lit*  version_opt  = arg_lit0("v", "version", "output version information and exit");
    struct arg_lit*  json_opt     = arg_lit0(NULL, "json",    "output in JSON format");
    struct arg_lit*  heading_opt  = arg_lit0("H", "heading",  "print column names");
    struct arg_lit*  i_opt        = arg_lit0("i", NULL,      "show only network files");
    struct arg_lit*  n_opt        = arg_lit0("n", NULL,      "don't resolve hostnames");
    struct arg_lit*  P_opt        = arg_lit0("P", NULL,      "don't resolve port numbers");
    struct arg_lit*  F_opt        = arg_lit0("F", NULL,      "field output format");
    struct arg_lit*  R_opt        = arg_lit0("R", NULL,      "show reference counts");
    struct arg_lit*  a_opt        = arg_lit0("a", NULL,      "AND logic between filters");
    struct arg_lit*  all_opt      = arg_lit0(NULL, "all",     "show all open FDs (default)");
    struct arg_str*  p_opt        = arg_str0("p", "pid",     "<PID>",    "show FDs for process <PID>");
    struct arg_str*  c_opt        = arg_str0("c", "command", "<CMD>",    "show FDs for processes named <CMD>");
    struct arg_str*  u_opt        = arg_str0("u", "user",    "<USER>",   "show FDs for user <USER>");
    struct arg_str*  t_opt        = arg_str0("t", "type",    "<TYPE>",   "show only open files of type <TYPE>");
    struct arg_str*  d_opt        = arg_str0("d", "fd",      "<FD>",     "show only FD <FD> (number or token)");
    struct arg_int*  r_opt        = arg_int0("r", NULL,      "<N>",     "repeat output N times");
    struct arg_end*  end          = arg_end(20);

    // Preprocess argv: extract numeric positional args and rewrite as -p flags
    // so argtable3 doesn't reject them as unexpected arguments.
    std::vector<char*> new_argv;
    new_argv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        bool is_known_flag = false;
        const char* known[] = {"-p","--pid","-c","--command","-u","--user","-t","--type",
                               "-d","--fd","-r","-H","--heading","-F","-R","-a","-i","-n","-P",
                               "--json","--help","--version","-v","--all",nullptr};
        for (int j = 0; known[j] != nullptr; j += 2) {
            if (strcmp(argv[i], known[j]) == 0 || strcmp(argv[i], known[j+1]) == 0) {
                is_known_flag = true; break;
            }
        }
        if (is_known_flag) {
            new_argv.push_back(argv[i]);
            // If flag takes a value, include the next arg too
            if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pid") == 0) {
                if (i + 1 < argc) { new_argv.push_back(argv[i+1]); i++; }
            } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--command") == 0) {
                if (i + 1 < argc) { new_argv.push_back(argv[i+1]); i++; }
            } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--user") == 0) {
                if (i + 1 < argc) { new_argv.push_back(argv[i+1]); i++; }
            } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0) {
                if (i + 1 < argc) { new_argv.push_back(argv[i+1]); i++; }
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--fd") == 0) {
                if (i + 1 < argc) { new_argv.push_back(argv[i+1]); i++; }
            } else if (strcmp(argv[i], "-r") == 0) {
                if (i + 1 < argc) { new_argv.push_back(argv[i+1]); i++; }
            }
        } else {
            // Check if it's a numeric positional arg (PID)
            bool is_num = true;
            for (const char* p = argv[i]; (*p) != 0; p++) {
                if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
            }
            if (is_num) {
                new_argv.push_back(const_cast<char*>("-p"));
                new_argv.push_back(argv[i]);
            } else {
                new_argv.push_back(argv[i]);
            }
        }
    }
    int const new_argc = static_cast<int>(new_argv.size());
    char** new_argv_ptr = new_argv.data();

    ArgTable at({help_opt, version_opt, all_opt, json_opt, heading_opt, i_opt, n_opt, P_opt, F_opt, R_opt, a_opt,
                 p_opt, c_opt, u_opt, t_opt, d_opt, r_opt, end});
    int const nerrors = at.parse(new_argc, new_argv_ptr);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [PID]...\n", argv[0]);
        printf("List open file descriptors.\n\n");
        printf("  -p, --pid <PID>      show FDs for process <PID>\n");
        printf("  -c, --command <CMD>  show FDs for processes named <CMD>\n");
        printf("  -u, --user <USER>    show FDs for user <USER>\n");
        printf("  -t, --type <TYPE>    show only open files of type <TYPE>\n");
        printf("                     (REG,CHR,BLK,FIFO,SOCK,DIR,NET)\n");
        printf("  -d, --fd <FD>        show only FD <FD> (number or token)\n");
        printf("  -i                   show only network files\n");
        printf("  -n                   don't resolve hostnames\n");
        printf("  -P                   don't resolve port numbers\n");
        printf("  -F                   field output format\n");
        printf("  -R                   show reference counts\n");
        printf("  -a                   AND logic between filters\n");
        printf("  -r <N>               repeat output N times with 1s delay\n");
        printf("  -H, --heading        print column names\n");
        printf("      --json           output in JSON format\n");
        printf("  -h, --help           display this help and exit\n");
        printf("      --version        output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("lsof");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    bool json_mode      = json_opt->count > 0;
    bool const heading        = heading_opt->count > 0;
    bool field_mode     = F_opt->count > 0;
    bool show_refcount  = R_opt->count > 0;
    bool net_only       = i_opt->count > 0;
    bool and_logic      = a_opt->count > 0;
    int  const repeat_count   = (r_opt->count > 0) ? r_opt->ival[0] : 1;

    std::string cmd_filter  = (c_opt->count > 0) ? c_opt->sval[0] : "";
    std::string user_filter = (u_opt->count > 0) ? u_opt->sval[0] : "";
    std::string type_filter = (t_opt->count > 0) ? t_opt->sval[0] : "";
    std::string fd_filter   = (d_opt->count > 0) ? d_opt->sval[0] : "";

    // Collect target PIDs: -p flags + positional numeric args from argv
    std::vector<pid_t> target_pids;
    if (p_opt->count > 0) {
        for (int i = 0; i < p_opt->count; i++) {
            target_pids.push_back(atoi(p_opt->sval[i]));
        }
    }
    // Accept purely numeric positional args as PIDs
    for (int i = 1; i < argc; i++) {
        bool is_flag_arg = false;
        const char* flag_names[] = {"-p","--pid","-c","--command","-u","--user","-t","--type",
                                     "-d","--fd","-r","-H","--heading","-F","-R","-a","-i","-n","-P",
                                     "--json","--help","--version",nullptr};
        for (int j = 0; flag_names[j] != nullptr; j += 2) {
            if (strcmp(argv[i], flag_names[j]) == 0 || strcmp(argv[i], flag_names[j+1]) == 0) {
                is_flag_arg = true; break;
            }
        }
        if (is_flag_arg) { continue;
}
        bool is_num = true;
        for (const char* p = argv[i]; (*p) != 0; p++) {
            if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
        }
        if (is_num) { target_pids.push_back(atoi(argv[i]));
}
    }

    // If no -p given, scan all PIDs in /proc
    if (target_pids.empty()) {
        DIR* proc = opendir("/proc");
        if (proc != nullptr) {
            struct dirent* de;
            while ((de = readdir(proc)) != nullptr) {
                bool is_num = true;
                for (const char* p = de->d_name; (*p) != 0; p++) {
                    if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
                }
                if (is_num) { target_pids.push_back(atoi(de->d_name));
}
            }
            closedir(proc);
        }
    }

    // Lambda: check if a single filter predicate matches an entry
    auto match_cmd  = [&](const LsofEntry& e)  { return cmd_filter.empty()  || to_lower(std::string(e.comm)).find(to_lower(cmd_filter))  != std::string::npos; };
    auto match_user = [&](const LsofEntry& e)  { return user_filter.empty() || to_lower(std::string(e.user)).find(to_lower(user_filter)) != std::string::npos; };
    auto match_type = [&](const LsofEntry& e)  {
        if (type_filter.empty()) { return true;
}
        std::string const norm = normalize_type(type_filter);
        std::string const et   = to_lower(std::string(e.type));
        return et.find(norm) != std::string::npos || norm == et;
    };
    auto match_fd   = [&](const LsofEntry& e)  { return fd_filter.empty()   || std::string(e.fd) == fd_filter; };
    auto match_net  = [&](const LsofEntry& e)  {
        if (!net_only) { return true;
}
        std::string const et = to_lower(std::string(e.type));
        return et.find("sock") != std::string::npos || et.find("net") != std::string::npos;
    };

    // Apply filters with requested logic (AND vs OR)
    auto apply_filters = [&](const std::vector<LsofEntry>& all) -> std::vector<LsofEntry> {
        std::vector<LsofEntry> filtered;
        bool const any_filter = !cmd_filter.empty() || !user_filter.empty() ||
                          !type_filter.empty() || !fd_filter.empty() || net_only;
        if (!any_filter) { return all;
}
        for (const auto& e : all) {
            bool const cm  = match_cmd(e);
            bool const us  = match_user(e);
            bool const ty  = match_type(e);
            bool const fd  = match_fd(e);
            bool const nt  = match_net(e);
            if (and_logic) {
                if (cm && us && ty && fd && nt) { filtered.push_back(e);
}
            } else {
                // OR: entry passes if it matches at least one active filter
                bool const matched_any = (!cmd_filter.empty() && cm) ||
                                   (!user_filter.empty() && us) ||
                                   (!type_filter.empty() && ty)  ||
                                   (!fd_filter.empty()  && fd)   ||
                                   (net_only && nt);
                if (matched_any) { filtered.push_back(e);
}
            }
        }
        return filtered;
    };

    // Collect and deduplicate entries
    auto collect_entries = [&]() -> std::vector<LsofEntry> {
        std::vector<LsofEntry> all;
        for (pid_t pid : target_pids) {
            if (pid <= 0) { continue;
}
            char probe[64];
            (void)snprintf(probe, sizeof(probe), "/proc/%d/fd", pid);
            DIR* probe_dir = opendir(probe);
            if (!probe_dir) { continue;
}
            closedir(probe_dir);
            auto ents = read_proc_fd(pid);
            all.insert(all.end(), ents.begin(), ents.end());
        }
        // Deduplicate by (pid, fd)
        std::vector<LsofEntry> deduped;
        std::unordered_map<std::string, bool> seen;
        for (const auto& e : all) {
            std::string const key = std::to_string(e.pid) + ":" + e.fd;
            if (seen.count(key)) { continue;
}
            seen[key] = true;
            deduped.push_back(e);
        }
        return deduped;
    };

    // Output helpers
    auto print_header = [&]() {
        if (field_mode) { return;
}
        printf("%-15s %6s %6s %4s %4s %8s %10s %6s %s\n",
               "COMMAND", "PID", "USER", "FD", "TYPE", "DEVICE", "SIZE/OFF", "NODE", "NAME");
    };

    auto print_row = [&](const LsofEntry& e) {
        if (field_mode) {
            printf("p%d\nf%s\nn%s\n", e.pid, e.fd, e.name);
            printf("a%s\nu%s\nt%s\nD%s\ns%s\ni%s\n",
                   e.comm, e.user, e.type, e.device, e.size_off, e.node);
            if (show_refcount) {
                int fdnum = 0;
                (void)sscanf(e.fd, "%d", &fdnum);
                std::string const lock_info = read_fdinfo_value(e.pid, fdnum, "lock:");
                printf("r%s\n", lock_info.empty() ? "0" : lock_info.c_str());
            }
            printf("---\n");
            return;
        }
        if (json_mode) {
            (void)fprintf(stdout, "  {\n");
            (void)fprintf(stdout, "    \"pid\": %d,\n", e.pid);
            (void)fprintf(stdout, "    \"comm\": ");
            json_escape_string(stdout, e.comm);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"user\": ");
            json_escape_string(stdout, e.user);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"fd\": ");
            json_escape_string(stdout, e.fd);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"type\": ");
            json_escape_string(stdout, e.type);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"device\": ");
            json_escape_string(stdout, e.device);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"size_off\": ");
            json_escape_string(stdout, e.size_off);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"node\": ");
            json_escape_string(stdout, e.node);
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"name\": ");
            json_escape_string(stdout, e.name);
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "  }");
            return;
        }
        printf("%-15s %6d %6s %4s %4s %8s %10s %6s %s\n",
               e.comm, e.pid, e.user, e.fd, e.type, e.device, e.size_off, e.node, e.name);
    };

    // Main loop (handles -r repeat)
    for (int rep = 0; rep < repeat_count; rep++) {
        if (rep > 0) { printf("\n==== %d ==== \n", rep + 1);
}

        std::vector<LsofEntry> const deduped = collect_entries();
        std::vector<LsofEntry> filtered  = apply_filters(deduped);

        if (!field_mode && !json_mode && heading) { print_header();
}
        if (json_mode) {
            printf("[\n");
            for (size_t i = 0; i < filtered.size(); i++) {
                print_row(filtered[i]);
                if (i + 1 < filtered.size()) { printf(",\n");
                } else { printf("\n");
}
            }
            printf("]\n");
        } else {
            for (const auto& e : filtered) { print_row(e);
}
        }

        if (repeat_count > 1 && rep < repeat_count - 1) { sleep(1);
}
    }

    return 0;
}

REGISTER_COMMAND("lsof", lsof_command, "List open file descriptors");
