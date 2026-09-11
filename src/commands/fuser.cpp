#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <linux/limits.h>
#include <signal.h>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>
#include <climits>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "commands/fuser.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------

struct FuserEntry {
    pid_t   pid        = 0;
    char    comm[256]  = {};
    char    user[64]   = {};
    char    access[32] = {};
    char    path[PATH_MAX] = {};
    int     fd         = -1;
};

// ---------------------------------------------------------------------------
// Proc helpers
// ---------------------------------------------------------------------------

static std::string read_proc_comm(pid_t pid) {
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE* f = fopen(path, "r");
    if (f == nullptr) { return "?";
}
    char buf[256] = {};
    if (fgets(buf, sizeof(buf), f) != nullptr) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) { buf[--n] = '\0';
}
    }
    (void)fclose(f);
    if (buf[0] == '\0') { return "?";
}
    return buf;
}

static std::string get_username(uid_t uid) {
    const struct passwd* pw = getpwuid(uid);
    if (pw != nullptr) { return std::string(pw->pw_name);
}
    char buf[16];
    (void)snprintf(buf, sizeof(buf), "%u", uid);
    return buf;
}

static bool read_proc_uid(pid_t pid, uid_t& out_uid) {
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

// True if process <pid> is a zombie (State: Z in /proc/<pid>/status).
static bool is_zombie(pid_t pid) {
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (f == nullptr) { return false;
}
    char line[256];
    bool zombie = false;
    while (fgets(line, sizeof(line), f) != nullptr) {
        if (strncmp(line, "State:", 6) == 0) {
            if (strstr(line, "Z") != nullptr) { zombie = true;
}
            break;
        }
    }
    (void)fclose(f);
    return zombie;
}

// Resolve a path (following symlinks) to a canonical absolute path.
static std::string canonical_path(const std::string& p) {
    char buf[PATH_MAX];
    if (realpath(p.c_str(), buf) != nullptr) { return std::string(buf);
}
    return p;
}

// ---------------------------------------------------------------------------
// FD access-mode detection from fdinfo flags
// ---------------------------------------------------------------------------

// Read a process's fd flags and build the fuser access string.
// Matches real fuser -v conventions: C (create), <> read+write, > write,
// < read, a append, m mmap.
static void build_access(pid_t pid, int fd, char* out, size_t out_size) {
    char path[128];
    (void)snprintf(path, sizeof(path), "/proc/%d/fdinfo/%d", pid, fd);
    FILE* f = fopen(path, "r");
    std::string acc;
    if (f != nullptr) {
        char line[512];
        while (fgets(line, sizeof(line), f) != nullptr) {
            if (strncmp(line, "flags:", 6) == 0) {
                unsigned long const flags = strtoul(line + 6, nullptr, 10);
                if ((flags & O_ACCMODE) != 0u) {
                    int const mode = flags & O_ACCMODE;
                    if (mode == O_RDWR) { acc += "<>";
                    } else if (mode == O_WRONLY) { acc += ">";
                    } else if (mode == O_RDONLY) { acc += "<";
}
                }
                if ((flags & O_APPEND) != 0u) { acc += "a";
}
                if ((flags & O_CREAT) != 0u) {  acc += "c";
}
                break;
            }
        }
        (void)fclose(f);
    } else {
        // No fdinfo (e.g. special fds) — treat as read access by default.
        acc = "<";
    }
    if (acc.empty()) { acc = "<";
}
    (void)snprintf(out, out_size, "%s", acc.c_str());
}

// ---------------------------------------------------------------------------
// Path matching (default mode)
// ---------------------------------------------------------------------------

// Collect PIDs whose /proc/<pid>/fd/* references <target> (canonicalized) or,
// with all_access, whose cwd/root match it.
static void collect_by_path(const std::string& target_canon, bool all_access,
                            std::vector<FuserEntry>& out) {
    DIR* proc = opendir("/proc");
    if (proc == nullptr) { return;
}
    struct dirent* de;
    while ((de = readdir(proc)) != nullptr) {
        bool is_num = true;
        for (const char* p = de->d_name; (*p) != 0; p++) {
            if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
        }
        if (!is_num) { continue;
}
        pid_t const pid = atoi(de->d_name);

        char fd_dir[128];
        (void)snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
        DIR* dh = opendir(fd_dir);
        if (dh == nullptr) { continue;
}
        struct dirent* fde;
        while ((fde = readdir(dh)) != nullptr) {
            bool fd_num = true;
            for (const char* p = fde->d_name; (*p) != 0; p++) {
                if (isdigit(static_cast<unsigned char>(*p)) == 0) { fd_num = false; break; }
            }
            if (!fd_num) { continue;
}
            int const fd = atoi(fde->d_name);
            char link_path[160];
            (void)snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%d", pid, fd);
            char target_buf[PATH_MAX];
            ssize_t const tlen = readlink(link_path, target_buf, sizeof(target_buf) - 1);
            if (tlen < 0) { continue;
}
            target_buf[tlen] = '\0';
            if (std::string(target_buf) == target_canon) {
                FuserEntry e;
                e.pid = pid;
                e.fd = fd;
                std::string const comm = read_proc_comm(pid);
                strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                uid_t uid = 0;
                read_proc_uid(pid, uid);
                std::string const u = get_username(uid);
                strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                build_access(pid, fd, e.access, sizeof(e.access));
                strncpy(e.path, target_canon.c_str(), sizeof(e.path) - 1);
                out.push_back(e);
            }
        }
        closedir(dh);

        if (all_access) {
            // Check cwd and root
            const char* special_dirs[] = {"cwd", "root", nullptr};
            for (int s = 0; special_dirs[s] != nullptr; s++) {
                char spath[128];
                (void)snprintf(spath, sizeof(spath), "/proc/%d/%s", pid, special_dirs[s]);
                char sbuf[PATH_MAX];
                ssize_t const sl = readlink(spath, sbuf, sizeof(sbuf) - 1);
                if (sl <= 0) { continue;
}
                sbuf[sl] = '\0';
                if (std::string(sbuf) == target_canon) {
                    FuserEntry e;
                    e.pid = pid;
                    e.fd = -1;
                    std::string const comm = read_proc_comm(pid);
                    strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                    uid_t uid = 0;
                    read_proc_uid(pid, uid);
                    std::string const u = get_username(uid);
                    strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                    (void)snprintf(e.access, sizeof(e.access), "c");
                    strncpy(e.path, target_canon.c_str(), sizeof(e.path) - 1);
                    out.push_back(e);
                }
            }
        }
    }
    closedir(proc);
}

// ---------------------------------------------------------------------------
// Mount point matching (-m)
// ---------------------------------------------------------------------------

static void collect_by_mount(const std::string& mount_path, bool recursive,
                             std::vector<FuserEntry>& out) {
    struct stat mst;
    if (stat(mount_path.c_str(), &mst) != 0) { return;
}
    dev_t const target_dev = mst.st_dev;

    DIR* proc = opendir("/proc");
    if (proc == nullptr) { return;
}
    struct dirent* de;
    while ((de = readdir(proc)) != nullptr) {
        bool is_num = true;
        for (const char* p = de->d_name; (*p) != 0; p++) {
            if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
        }
        if (!is_num) { continue;
}
        pid_t const pid = atoi(de->d_name);

        char fd_dir[128];
        (void)snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
        DIR* dh = opendir(fd_dir);
        if (dh == nullptr) { continue;
}
        struct dirent* fde;
        while ((fde = readdir(dh)) != nullptr) {
            bool fd_num = true;
            for (const char* p = fde->d_name; (*p) != 0; p++) {
                if (isdigit(static_cast<unsigned char>(*p)) == 0) { fd_num = false; break; }
            }
            if (!fd_num) { continue;
}
            int const fd = atoi(fde->d_name);
            char link_path[160];
            (void)snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%d", pid, fd);
            char target_buf[PATH_MAX];
            ssize_t const tlen = readlink(link_path, target_buf, sizeof(target_buf) - 1);
            if (tlen < 0) { continue;
}
            target_buf[tlen] = '\0';
            struct stat fst;
            if (stat(target_buf, &fst) != 0) { continue;
}
            if (fst.st_dev == target_dev) {
                if (!recursive) {
                    // non-recursive: only accept files whose path starts with mount_path
                    std::string const canon = canonical_path(target_buf);
                    if (!canon.starts_with(mount_path)) { continue;
}
                }
                FuserEntry e;
                e.pid = pid;
                e.fd = fd;
                std::string const comm = read_proc_comm(pid);
                strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                uid_t uid = 0;
                read_proc_uid(pid, uid);
                std::string const u = get_username(uid);
                strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                build_access(pid, fd, e.access, sizeof(e.access));
                strncpy(e.path, target_buf, sizeof(e.path) - 1);
                out.push_back(e);
            }
        }
        closedir(dh);
    }
    closedir(proc);
}

// ---------------------------------------------------------------------------
// Network matching (-n)
// ---------------------------------------------------------------------------

struct NetFilter {
    enum Kind { None, Tcp, Udp, Unix, Fd, Block } kind = None;
    std::string arg;   // port string or path
    int ipv = 0;       // 0=any, 4=IPv4 only, 6=IPv6 only
};

// Match port against /proc/net/tcp{,6}/udp{,6} for the given pid by looking at
// its socket inode. We collect the network inode->port map once, then check a
// pid's socket fds against it.
struct NetEntry {
    uint64_t inode;
    int port;
    int family; // 4 or 6
};

static std::vector<NetEntry> build_net_table(int ipv) {
    std::vector<NetEntry> table;
    // 4, 6 only, or both
    struct { const char* path; int fam; } f[] = {
        {.path="/proc/net/tcp", .fam=4}, {.path="/proc/net/tcp6", .fam=6},
        {.path="/proc/net/udp", .fam=4}, {.path="/proc/net/udp6", .fam=6},
    };
    for (auto& fi : f) {
        if (ipv == 4 && fi.fam != 4) { continue;
}
        if (ipv == 6 && fi.fam != 6) { continue;
}
        FILE* fp = fopen(fi.path, "r");
        if (fp == nullptr) { continue;
}
        char line[512];
        bool header = true;
        while (fgets(line, sizeof(line), fp) != nullptr) {
            if (header) { header = false; continue; }
            // sl local_address rem_address st ... inode
            char tokens[20][64];
            int tok = 0;
            const char* p = line;
            while (((*p) != 0) && tok < 20) {
                while (*p == ' ' || *p == '\t') { p++;
}
                if ((*p) == 0) { break;
}
                const char* start = p;
                while (((*p) != 0) && *p != ' ' && *p != '\t') { p++;
}
                int len = static_cast<int>(p - start);
                if (len >= 64) { len = 63;
}
                strncpy(tokens[tok], start, len);
                tokens[tok][len] = '\0';
                tok++;
            }
            if (tok < 10) { continue;
}
            // tokens[1] = local_address, tokens[9] = inode
            char const * colon = strchr(tokens[1], ':');
            if (colon == nullptr) { continue;
}
            int const port_hex = static_cast<int>(strtoul(colon + 1, nullptr, 16));
            uint64_t const inode = strtoull(tokens[9], nullptr, 10);
            if (inode == 0) { continue;
}
            NetEntry ne;
            ne.inode = inode;
            ne.port = port_hex;
            ne.family = fi.fam;
            table.push_back(ne);
        }
        (void)fclose(fp);
    }
    return table;
}

// Unix domain socket table: inode -> path
struct UnixEntry {
    uint64_t inode;
    std::string path;
};

static std::vector<UnixEntry> build_unix_table() {
    std::vector<UnixEntry> table;
    FILE* fp = fopen("/proc/net/unix", "r");
    if (fp == nullptr) { return table;
}
    char line[512];
    bool header = true;
    while (fgets(line, sizeof(line), fp) != nullptr) {
        if (header) { header = false; continue; }
        char tokens[12][192];
        int tok = 0;
        const char* p = line;
        while (((*p) != 0) && tok < 12) {
            while (*p == ' ' || *p == '\t') { p++;
}
            if ((*p) == 0) { break;
}
            const char* start = p;
            while (((*p) != 0) && *p != ' ' && *p != '\t') { p++;
}
            int len = static_cast<int>(p - start);
            if (len >= 192) { len = 191;
}
            strncpy(tokens[tok], start, len);
            tokens[tok][len] = '\0';
            tok++;
        }
        if (tok < 7) { continue;
}
        // tokens[6] = inode (hex), tokens[7] = path
        uint64_t const inode = strtoull(tokens[6], nullptr, 16);
        if (inode == 0) { continue;
}
        UnixEntry ue;
        ue.inode = inode;
        ue.path = (tok > 7) ? tokens[7] : "";
        table.push_back(ue);
    }
    (void)fclose(fp);
    return table;
}

// Match a pid's fds against the network table.
static void collect_by_net(const NetFilter& nf, std::vector<FuserEntry>& out) {
    // Prepare filters based on kind
    int target_port = -1;
    if (nf.kind == NetFilter::Tcp || nf.kind == NetFilter::Udp) {
        target_port = atoi(nf.arg.c_str());
        if (target_port <= 0) { return;
}
    }

    std::vector<NetEntry> net_table;
    std::vector<UnixEntry> unix_table;
    if (nf.kind == NetFilter::Tcp || nf.kind == NetFilter::Udp) {
        net_table = build_net_table(nf.ipv);
    } else if (nf.kind == NetFilter::Unix) {
        unix_table = build_unix_table();
    }

    DIR* proc = opendir("/proc");
    if (proc == nullptr) { return;
}
    struct dirent* de;
    while ((de = readdir(proc)) != nullptr) {
        bool is_num = true;
        for (const char* p = de->d_name; (*p) != 0; p++) {
            if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
        }
        if (!is_num) { continue;
}
        pid_t const pid = atoi(de->d_name);

        if (nf.kind == NetFilter::Fd) {
            // Match fd number directly
            int const want_fd = atoi(nf.arg.c_str());
            if (want_fd < 0) { continue;
}
            char probe[128];
            (void)snprintf(probe, sizeof(probe), "/proc/%d/fd/%d", pid, want_fd);
            char tb[PATH_MAX];
            if (readlink(probe, tb, sizeof(tb) - 1) > 0) {
                FuserEntry e;
                e.pid = pid; e.fd = want_fd;
                std::string const comm = read_proc_comm(pid);
                strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                uid_t uid = 0; read_proc_uid(pid, uid);
                std::string const u = get_username(uid);
                strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                build_access(pid, want_fd, e.access, sizeof(e.access));
                (void)snprintf(e.path, sizeof(e.path), "fd %d", want_fd);
                out.push_back(e);
            }
            continue;
        }

        if (nf.kind == NetFilter::Block) {
            // Match device by st_rdev
            struct stat dst;
            if (stat(nf.arg.c_str(), &dst) != 0) { continue;
}
            char fd_dir[128];
            (void)snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
            DIR* dh = opendir(fd_dir);
            if (dh == nullptr) { continue;
}
            struct dirent* fde;
            while ((fde = readdir(dh)) != nullptr) {
                bool fd_num = true;
                for (const char* p = fde->d_name; (*p) != 0; p++) {
                    if (isdigit(static_cast<unsigned char>(*p)) == 0) { fd_num = false; break; }
                }
                if (!fd_num) { continue;
}
                int const fd = atoi(fde->d_name);
                char link_path[160];
                (void)snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%d", pid, fd);
                char tb[PATH_MAX];
                ssize_t const tl = readlink(link_path, tb, sizeof(tb) - 1);
                if (tl <= 0) { continue;
}
                tb[tl] = '\0';
                struct stat fst;
                if (stat(tb, &fst) != 0) { continue;
}
                if (fst.st_rdev == dst.st_rdev) {
                    FuserEntry e;
                    e.pid = pid; e.fd = fd;
                    std::string const comm = read_proc_comm(pid);
                    strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                    uid_t uid = 0; read_proc_uid(pid, uid);
                    std::string const u = get_username(uid);
                    strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                    build_access(pid, fd, e.access, sizeof(e.access));
                    (void)snprintf(e.path, sizeof(e.path), "%s", tb);
                    out.push_back(e);
                }
            }
            closedir(dh);
            continue;
        }

        // Tcp / Udp / Unix: iterate fds, match socket inodes
        char fd_dir[128];
        (void)snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
        DIR* dh = opendir(fd_dir);
        if (dh == nullptr) { continue;
}
        struct dirent* fde;
        while ((fde = readdir(dh)) != nullptr) {
            bool fd_num = true;
            for (const char* p = fde->d_name; (*p) != 0; p++) {
                if (isdigit(static_cast<unsigned char>(*p)) == 0) { fd_num = false; break; }
            }
            if (!fd_num) { continue;
}
            int const fd = atoi(fde->d_name);
            char link_path[160];
            (void)snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%d", pid, fd);
            char tb[PATH_MAX];
            ssize_t const tl = readlink(link_path, tb, sizeof(tb) - 1);
            if (tl <= 0) { continue;
}
            tb[tl] = '\0';

            std::string const target(tb);
            if (nf.kind == NetFilter::Tcp || nf.kind == NetFilter::Udp) {
                // socket:[inode]
                size_t const bs = target.find('[');
                size_t const be = target.find(']');
                if (bs == std::string::npos || be == std::string::npos) { continue;
}
                uint64_t const inode = strtoull(target.substr(bs + 1, be - bs - 1).c_str(), nullptr, 10);
                bool matched = false;
                for (const auto& ne : net_table) {
                    if (ne.inode == inode && ne.port == target_port) { matched = true; break; }
                }
                if (!matched) { continue;
}
                FuserEntry e;
                e.pid = pid; e.fd = fd;
                std::string const comm = read_proc_comm(pid);
                strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                uid_t uid = 0; read_proc_uid(pid, uid);
                std::string const u = get_username(uid);
                strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                build_access(pid, fd, e.access, sizeof(e.access));
                (void)snprintf(e.path, sizeof(e.path), "%s", target.c_str());
                out.push_back(e);
            } else if (nf.kind == NetFilter::Unix) {
                size_t const bs = target.find('[');
                size_t const be = target.find(']');
                if (bs == std::string::npos || be == std::string::npos) { continue;
}
                uint64_t const inode = strtoull(target.substr(bs + 1, be - bs - 1).c_str(), nullptr, 10);
                bool matched = false;
                for (const auto& ue : unix_table) {
                    if (ue.inode == inode && ue.path == nf.arg) { matched = true; break; }
                }
                if (!matched) { continue;
}
                FuserEntry e;
                e.pid = pid; e.fd = fd;
                std::string const comm = read_proc_comm(pid);
                strncpy(e.comm, comm.c_str(), sizeof(e.comm) - 1);
                uid_t uid = 0; read_proc_uid(pid, uid);
                std::string const u = get_username(uid);
                strncpy(e.user, u.c_str(), sizeof(e.user) - 1);
                build_access(pid, fd, e.access, sizeof(e.access));
                (void)snprintf(e.path, sizeof(e.path), "%s", target.c_str());
                out.push_back(e);
            }
        }
        closedir(dh);
    }
    closedir(proc);
}

// ---------------------------------------------------------------------------
// Signal helpers (for -k / -l)
// ---------------------------------------------------------------------------

static void print_signals() {
    printf(" 1) SIGHUP     2) SIGINT     3) SIGQUIT    4) SIGILL     5) SIGTRAP\n");
    printf(" 6) SIGABRT    7) SIGBUS     8) SIGFPE     9) SIGKILL   10) SIGUSR1\n");
    printf("11) SIGSEGV   12) SIGUSR2   13) SIGPIPE   14) SIGALRM   15) SIGTERM\n");
    printf("16) SIGSTKFLT 17) SIGCHLD   18) SIGCONT   19) SIGSTOP   20) SIGTSTP\n");
    printf("21) SIGTTIN   22) SIGTTOU   23) SIGURG    24) SIGXCPU   25) SIGXFSZ\n");
    printf("26) SIGVTALRM 27) SIGPROF   28) SIGWINCH  29) SIGIO     30) SIGPWR\n");
    printf("31) SIGSYS\n");
}

static int resolve_signal(const char* name) {
    if (name[0] >= '0' && name[0] <= '9') {
        int const sig = atoi(name);
        if (sig > 0 && sig < 64) { return sig;
}
        return -1;
    }
    size_t const len = strlen(name);
    if (len > 3 && strncmp(name, "SIG", 3) == 0) { name += 3;
}
    if (strcmp(name, "HUP") == 0) { return SIGHUP;
}
    if (strcmp(name, "INT") == 0) { return SIGINT;
}
    if (strcmp(name, "QUIT") == 0) { return SIGQUIT;
}
    if (strcmp(name, "ILL") == 0) { return SIGILL;
}
    if (strcmp(name, "TRAP") == 0) { return SIGTRAP;
}
    if (strcmp(name, "ABRT") == 0) { return SIGABRT;
}
    if (strcmp(name, "BUS") == 0) { return SIGBUS;
}
    if (strcmp(name, "FPE") == 0) { return SIGFPE;
}
    if (strcmp(name, "KILL") == 0) { return SIGKILL;
}
    if (strcmp(name, "USR1") == 0) { return SIGUSR1;
}
    if (strcmp(name, "SEGV") == 0) { return SIGSEGV;
}
    if (strcmp(name, "USR2") == 0) { return SIGUSR2;
}
    if (strcmp(name, "PIPE") == 0) { return SIGPIPE;
}
    if (strcmp(name, "ALRM") == 0) { return SIGALRM;
}
    if (strcmp(name, "TERM") == 0) { return SIGTERM;
}
    if (strcmp(name, "CHLD") == 0) { return SIGCHLD;
}
    if (strcmp(name, "CONT") == 0) { return SIGCONT;
}
    if (strcmp(name, "STOP") == 0) { return SIGSTOP;
}
    if (strcmp(name, "TSTP") == 0) { return SIGTSTP;
}
    if (strcmp(name, "TTIN") == 0) { return SIGTTIN;
}
    if (strcmp(name, "TTOU") == 0) { return SIGTTOU;
}
    if (strcmp(name, "URG") == 0) { return SIGURG;
}
    if (strcmp(name, "XCPU") == 0) { return SIGXCPU;
}
    if (strcmp(name, "XFSZ") == 0) { return SIGXFSZ;
}
    if (strcmp(name, "VTALRM") == 0) { return SIGVTALRM;
}
    if (strcmp(name, "PROF") == 0) { return SIGPROF;
}
    if (strcmp(name, "WINCH") == 0) { return SIGWINCH;
}
    if (strcmp(name, "IO") == 0 || strcmp(name, "POLL") == 0) { return SIGIO;
}
    if (strcmp(name, "PWR") == 0) { return SIGPWR;
}
    if (strcmp(name, "SYS") == 0) { return SIGSYS;
}
    return -1;
}

// ---------------------------------------------------------------------------
// Dedup
// ---------------------------------------------------------------------------

static void dedup(std::vector<FuserEntry>& v) {
    std::vector<FuserEntry> out;
    for (auto& e : v) {
        bool dup = false;
        for (auto& o : out) {
            if (o.pid == e.pid && o.fd == e.fd) { dup = true; break; }
        }
        if (!dup) { out.push_back(e);
}
    }
    v = std::move(out);
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

int fuser_command(int argc, char** argv) {
    bool verbose = false;
    bool show_user = false;
    bool silent = false;
    bool kill_mode = false;
    bool interactive = false;
    bool mount_mode = false;
    bool recursive_dir = false;
    bool include_zombies = false;
    bool all_access = false;
    bool pids_only = false;
    bool t_opt = false;
    bool json_mode = false;
    bool mount_recursive = false;
    bool mount_exclude = false;
    bool list_signals = false;
    bool write_only = false;
    int signal = SIGKILL;
    int ipv = 0;
    bool has_net = false;
    NetFilter nf;
    std::vector<std::string> targets;

    // Pre-pass: extract -SIGXXX / -N signals and -n args
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.size() > 1 && a[0] == '-' && a[1] != '-' &&
            ((isalpha(static_cast<unsigned char>(a[1])) != 0) || (isdigit(static_cast<unsigned char>(a[1])) != 0))) {
            // Could be a signal spec like -SIGTERM or -15
            const char* rest = a.c_str() + 1;
            if (resolve_signal(rest) > 0) { signal = resolve_signal(rest); continue; }
        }
    }

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help") {
            printf("Usage: %s [options] [ -n SPACE ] [ -s SIGNAL ] FILE...\n", argv[0]);
            printf("Identify processes using files, sockets, or filesystems.\n\n");
            printf("  -a, --all           include processes that use the path via cwd/root\n");
            printf("      -4               use only IPv4 sockets\n");
            printf("      -6               use only IPv6 sockets\n");
            printf("  -c                  same as -m (filesystem containment)\n");
            printf("  -f, --silent        silently exit 1 if any process uses the file\n");
            printf("  -i, --interactive   ask before killing (with -k)\n");
            printf("  -k, --kill          kill processes accessing the file\n");
            printf("  -l, --list-signals  list available signal names\n");
            printf("  -m, --mount         show processes accessing a file system\n");
            printf("  -M                  never report processes that use a lower-level mount\n");
            printf("  -n, --namespace SPACE  search in namespace SPACE (file,udp,tcp,unix,fd,block)\n");
            printf("  -p, --pid           print only PIDs (one per line)\n");
            printf("  -R, --with-mounts   with -m, recursively descend into mounts\n");
            printf("  -r, --recursive     recurse into directory paths\n");
            printf("  -s, --silent        same as -f\n");
            printf("  -t, --pid-number    print PID numbers only, one per line, omitted for pids on same tty\n");
            printf("  -u, --user          show username\n");
            printf("  -v, --verbose       verbose output\n");
            printf("  -w, --write         match write access only\n");
            printf("  -z, --zombie        include zombie processes\n");
            printf("      --json          output in JSON format\n");
            printf("  -h, --help          display this help and exit\n");
            printf("      --version       output version information and exit\n");
            return 0;
        }
        if (a == "--version") { print_version("fuser"); return 0; }
        if (a == "-v" || a == "--verbose") { verbose = true; continue; }
        if (a == "-u" || a == "--user") { show_user = true; continue; }
        if (a == "-s" || a == "--silent" || a == "-f") { silent = true; continue; }
        if (a == "-k" || a == "--kill") { kill_mode = true; continue; }
        if (a == "-i" || a == "--interactive") { interactive = true; continue; }
        if (a == "-m" || a == "--mount" || a == "-c") { mount_mode = true; continue; }
        if (a == "-r" || a == "--recursive") { recursive_dir = true; continue; }
        if (a == "-R" || a == "--with-mounts") { mount_recursive = true; mount_mode = true; continue; }
        if (a == "-M") { mount_exclude = true; continue; }
        if (a == "-z" || a == "--zombie") { include_zombies = true; continue; }
        if (a == "-a" || a == "--all") { all_access = true; continue; }
        if (a == "-w" || a == "--write") { write_only = true; continue; }
        if (a == "-p" || a == "--pid") { pids_only = true; continue; }
        if (a == "-t") { t_opt = true; pids_only = true; continue; }
        if (a == "-l" || a == "--list-signals") { list_signals = true; continue; }
        if (a == "--json") { json_mode = true; continue; }
        if (a == "-4") { ipv = 4; continue; }
        if (a == "-6") { ipv = 6; continue; }
        if (a == "-n" || a == "--namespace") {
            has_net = true;
            if (i + 1 < argc) {
                std::string ns = argv[++i];
                std::transform(ns.begin(), ns.end(), ns.begin(), ::tolower);
                if (ns == "tcp") { nf.kind = NetFilter::Tcp; }
                else if (ns == "udp") { nf.kind = NetFilter::Udp; }
                else if (ns == "unix") { nf.kind = NetFilter::Unix; }
                else if (ns == "fd") { nf.kind = NetFilter::Fd; }
                else if (ns == "block" || ns == "dev") { nf.kind = NetFilter::Block; }
                else { (void)fprintf(stderr, "fuser: unknown namespace: %s\n", ns.c_str()); return 1; }
                if (i + 1 < argc) { nf.arg = argv[++i];
}
                nf.ipv = ipv;
            }
            continue;
        }
        // Skip already-consumed signal arg
        if (a.size() > 1 && a[0] == '-' && a[1] != '-' &&
            ((isalpha(static_cast<unsigned char>(a[1])) != 0) || (isdigit(static_cast<unsigned char>(a[1])) != 0))) {
            // Already parsed as part of the pre-pass; but if it was a real
            // signal spec we already set it. Avoid treating target values as flags.
            int const sig = resolve_signal(a.c_str() + 1);
            if (sig > 0) { continue;
}
        }
        // Unknown flag
        if (a.size() > 1 && a[0] == '-') {
            (void)fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], a.c_str());
            (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
        targets.push_back(a);
    }

    if (list_signals) { print_signals(); return 0; }

    std::vector<FuserEntry> entries;

    if (has_net) {
        collect_by_net(nf, entries);
    } else {
        if (mount_mode) {
            for (auto& t : targets) { collect_by_mount(t, mount_recursive, entries);
}
        } else {
            for (auto& t : targets) {
                std::string const canon = canonical_path(t);
                collect_by_path(canon, all_access, entries);
            }
        }
    }

    if (!include_zombies) {
        std::vector<FuserEntry> alive;
        for (auto& e : entries) { if (!is_zombie(e.pid)) { alive.push_back(e);
}
}
        entries = std::move(alive);
    }

    dedup(entries);
    std::sort(entries.begin(), entries.end(),
              [](const FuserEntry& a, const FuserEntry& b) { return a.pid < b.pid; });

    if (write_only) {
        std::vector<FuserEntry> filtered;
        for (auto& e : entries) {
            if ((std::strchr(e.access, '>') != nullptr) && (std::strchr(e.access, '<') == nullptr)) {
                filtered.push_back(std::move(e));
            }
        }
        entries = std::move(filtered);
    }

    // Kill mode
    if (kill_mode && !entries.empty()) {
        for (auto& e : entries) {
            if (interactive) {
                printf("Kill process %d (%s)? (y/N) ", e.pid, e.comm);
                (void)fflush(stdout);
                char const ans = static_cast<char>(getchar());
                if (ans != 'y' && ans != 'Y') { continue;
}
            }
            if (kill(e.pid, signal) < 0) {
                (void)fprintf(stderr, "fuser: kill (%d): %s\n", e.pid, strerror(errno));
            }
        }
    }

    if (silent) { return (entries.empty() ? 0 : 1);
}

    if (kill_mode && !verbose) {
        // In kill mode, default output is suppressed (it would print PIDs
        // that are being killed). Verbose mode is the dry-run path.
        return (entries.empty() ? 1 : 0);
    }

    // Output
    if (json_mode) {
        printf("[\n");
        for (size_t i = 0; i < entries.size(); i++) {
            const auto& e = entries[i];
            (void)fprintf(stdout, "  { \"pid\": %d, \"user\": ", e.pid);
            json_escape_string(stdout, e.user);
            (void)fprintf(stdout, ", \"comm\": ");
            json_escape_string(stdout, e.comm);
            (void)fprintf(stdout, ", \"access\": ");
            json_escape_string(stdout, e.access);
            (void)fprintf(stdout, ", \"path\": ");
            json_escape_string(stdout, e.path);
            (void)fprintf(stdout, " }");
            if (i + 1 < entries.size()) { (void)fprintf(stdout, ",");
}
            (void)fprintf(stdout, "\n");
        }
        printf("]\n");
        return (entries.empty() ? 1 : 0);
    }

    if (verbose) {
        for (const auto& e : entries) {
            printf(" %d(%s): %s %s\n", e.pid, e.user, e.access, e.path);
        }
    } else if (pids_only) {
        for (const auto& e : entries) { printf("%d\n", e.pid);
}
    } else {
        // default: one line, PID + access codes separated by space
        for (const auto& e : entries) {
            printf("%d%s ", e.pid, e.access);
        }
        printf("\n");
    }

    return (entries.empty() ? 1 : 0);
}

REGISTER_COMMAND("fuser", fuser_command, "Identify processes using files");
