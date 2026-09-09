#include "commands/nc.hpp"

#include <argtable3.h>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// ── Options ─────────────────────────────────────────────────────────────────

struct NCOptions {
    bool listen = false;
    bool keep = false;
    bool udp = false;
    bool zero_io = false;
    bool verbose = false;
    bool reuse_addr = false;
    bool prefer_ipv6 = false;

    std::string host;
    int port = 0;
    int source_port = 0;

    int timeout = 0;
    double interval = 0.0;
    std::string exec_cmd;
};

volatile sig_atomic_t g_stop = 0;

void on_sigint(int) { g_stop = 1; }
void on_sigpipe(int) { /* ignore */ }

// RAII: install the SIGINT/SIGPIPE handlers for the command's lifetime and
// restore SIG_DFL when it returns, so no handler state leaks beyond nc.
struct SignalGuard {
    SignalGuard() {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_sigint;
        (void)sigaction(SIGINT, &sa, nullptr);
        sa.sa_handler = on_sigpipe;
        (void)sigaction(SIGPIPE, &sa, nullptr);
    }
    ~SignalGuard() {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        (void)sigaction(SIGINT, &sa, nullptr);
        (void)sigaction(SIGPIPE, &sa, nullptr);
    }
    SignalGuard(const SignalGuard&) = delete;
    SignalGuard& operator=(const SignalGuard&) = delete;
};

// ── Helpers ─────────────────────────────────────────────────────────────────

bool is_numeric(const std::string& s) {
    if (s.empty()) { return false; }
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) { return false; }
    }
    return true;
}

// ── Relay ───────────────────────────────────────────────────────────────────

// Bidirectional relay between stdin and the socket using poll().
// Works for TCP and for UDP (connected client sockets send plain send();
// unconnected listener sockets only reply to a peer that has sent first).
int relay(int sock, bool udp_mode) {
    char buf[65536];

    struct sockaddr_storage peer{};
    socklen_t peer_len = 0;
    bool udp_connected = true;
    if (udp_mode) {
        socklen_t plen = sizeof(peer);
        udp_connected = (getsockopt(sock, SOL_SOCKET, SO_PEERNAME,
                                    reinterpret_cast<struct sockaddr*>(&peer), &plen) == 0);
        peer_len = plen;
    }

    for (;;) {
        if (g_stop) { break; }

        struct pollfd pfds[2];
        pfds[0].fd = sock;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        pfds[1].fd = STDIN_FILENO;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;

        int pr = poll(pfds, 2, 100);
        if (pr < 0) {
            if (errno == EINTR) { continue; }
            break;
        }
        if (pr == 0) { continue; }

        if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n;
            if (udp_mode) {
                socklen_t alen = sizeof(peer);
                n = recvfrom(sock, buf, sizeof(buf), 0,
                             reinterpret_cast<struct sockaddr*>(&peer), &alen);
                if (n > 0) { peer_len = alen; }
            } else {
                n = recv(sock, buf, sizeof(buf), 0);
            }
            if (n <= 0) { break; }
            (void)write(STDOUT_FILENO, buf, static_cast<size_t>(n));
        }

        if (pfds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) { break; }
            if (udp_mode && !udp_connected) {
                // Unconnected UDP cannot address a peer until it has sent
                // first; drop stdin until a datagram arrives.
                if (peer_len > 0) {
                    (void)sendto(sock, buf, static_cast<size_t>(n), 0,
                                 reinterpret_cast<struct sockaddr*>(&peer), peer_len);
                }
            } else {
                (void)send(sock, buf, static_cast<size_t>(n), MSG_NOSIGNAL);
            }
        }
    }
    return 0;
}


// ── Resolution ─────────────────────────────────────────────────────────────

struct Resolved {
    int fd = -1;
    std::string ip;
    int port = 0;
};

// Shared getaddrinfo hint setup for connect and listener modes.
struct addrinfo build_hints(const NCOptions* opts, bool passive) {
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = opts->prefer_ipv6 ? AF_INET6 : AF_UNSPEC;
    if (passive) {
        hints.ai_flags = AI_PASSIVE;
    }
    if (opts->prefer_ipv6) {
        hints.ai_flags |= AI_V4MAPPED | AI_ADDRCONFIG;
    }
    hints.ai_socktype = opts->udp ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = opts->udp ? IPPROTO_UDP : IPPROTO_TCP;
    return hints;
}

int make_socket(const struct addrinfo* p, bool reuse_addr) {
    int s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (s < 0) { return -1; }
    if (reuse_addr) {
        int one = 1;
        (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
    return s;
}

bool bind_source_port(int sock, const struct addrinfo* p, int source_port) {
    struct sockaddr_storage baddr;
    std::memset(&baddr, 0, sizeof(baddr));
    socklen_t blen;
    if (p->ai_family == AF_INET) {
        auto* a4 = reinterpret_cast<struct sockaddr_in*>(&baddr);
        a4->sin_family = AF_INET;
        a4->sin_port = htons(static_cast<uint16_t>(source_port));
        blen = sizeof(struct sockaddr_in);
    } else {
        auto* a6 = reinterpret_cast<struct sockaddr_in6*>(&baddr);
        a6->sin6_family = AF_INET6;
        a6->sin6_port = htons(static_cast<uint16_t>(source_port));
        blen = sizeof(struct sockaddr_in6);
    }
    return bind(sock, reinterpret_cast<struct sockaddr*>(&baddr), blen) == 0;
}

// connect() with an optional timeout in seconds: with a timeout the socket
// is made non-blocking and polled for writability (SO_RCVTIMEO does not
// bound connect() itself); without one it blocks as usual. Sets errno on
// failure (ETIMEDOUT on timeout).
bool connect_with_timeout(int sock, const struct addrinfo* p, int timeout) {
    if (timeout <= 0) {
        return connect(sock, p->ai_addr, p->ai_addrlen) == 0;
    }
    (void)fcntl(sock, F_SETFL, O_NONBLOCK);
    if (connect(sock, p->ai_addr, p->ai_addrlen) < 0 && errno != EINPROGRESS) {
        return false;
    }
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    int pr = poll(&pfd, 1, timeout * 1000);
    int so_error = 0;
    socklen_t elen = sizeof(so_error);
    (void)getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &elen);
    (void)fcntl(sock, F_SETFL, 0);
    if (pr <= 0) {
        errno = ETIMEDOUT;
        return false;
    }
    if (so_error != 0) {
        errno = so_error;
        return false;
    }
    return true;
}

// Resolve host:port and create+connect a socket.
bool resolve_and_connect(const NCOptions* opts, Resolved& out) {
    if (opts->host.empty()) { return false; }

    std::string port_str = std::to_string(opts->port);
    struct addrinfo hints = build_hints(opts, false);
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo(opts->host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0) {
        (void)fprintf(stderr, "nc: %s\n", gai_strerror(rc));
        return false;
    }

    int sock = -1;
    int saved_errno = 0;
    struct addrinfo* chosen = nullptr;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        if (p->ai_family != AF_INET && p->ai_family != AF_INET6) { continue; }
        sock = make_socket(p, opts->reuse_addr);
        if (sock < 0) { continue; }

        if (opts->timeout > 0) {
            struct timeval tv;
            tv.tv_sec = opts->timeout;
            tv.tv_usec = 0;
            (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            (void)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }

        if (opts->source_port > 0 && !bind_source_port(sock, p, opts->source_port)) {
            saved_errno = errno;
            close(sock);
            sock = -1;
            continue;
        }

        if (connect_with_timeout(sock, p, opts->timeout)) {
            chosen = p;
            break;
        }
        saved_errno = errno;
        close(sock);
        sock = -1;
    }

    if (sock < 0 || chosen == nullptr) {
        freeaddrinfo(res);
        out.fd = -1;
        (void)fprintf(stderr, "nc: connect to %s port %d (%s) failed: %s\n",
                      opts->host.c_str(), opts->port,
                      opts->udp ? "udp" : "tcp",
                      strerror(saved_errno));
        return false;
    }

    // Capture the resolved IP while the addrinfo list is still alive.
    std::string ip;
    if (chosen->ai_family == AF_INET) {
        struct sockaddr_in a4;
        std::memcpy(&a4, chosen->ai_addr, sizeof(a4));
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &a4.sin_addr, buf, sizeof(buf)) != nullptr) {
            ip = buf;
        }
    } else {
        struct sockaddr_in6 a6;
        std::memcpy(&a6, chosen->ai_addr, sizeof(a6));
        char buf[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &a6.sin6_addr, buf, sizeof(buf)) != nullptr) {
            ip = buf;
        }
    }
    freeaddrinfo(res);

    out.fd = sock;
    out.ip = std::move(ip);
    out.port = opts->port;
    return true;
}


// ── Listener ────────────────────────────────────────────────────────────────

int run_listener(const NCOptions* opts) {
    std::string port_str = std::to_string(opts->port);
    struct addrinfo hints = build_hints(opts, true);
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo(nullptr, port_str.c_str(), &hints, &res);
    if (rc != 0) {
        (void)fprintf(stderr, "nc: %s\n", gai_strerror(rc));
        return 1;
    }

    int lsock = -1;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        if (p->ai_family != AF_INET && p->ai_family != AF_INET6) { continue; }
        lsock = make_socket(p, true);
        if (lsock < 0) { continue; }
        if (bind(lsock, p->ai_addr, p->ai_addrlen) == 0) { break; }
        close(lsock);
        lsock = -1;
    }
    freeaddrinfo(res);

    if (lsock < 0) {
        (void)fprintf(stderr, "nc: bind to port %d failed\n", opts->port);
        return 1;
    }

    if (!opts->udp) {
        if (listen(lsock, 5) < 0) {
            (void)fprintf(stderr, "nc: listen failed: %s\n", strerror(errno));
            close(lsock);
            return 1;
        }
    }

    if (opts->verbose) {
        (void)fprintf(stderr, "Listening on [::] %d\n", opts->port);
    }

    for (;;) {
        if (g_stop) { break; }

        if (opts->udp) {
            relay(lsock, true);
            break;
        }

        struct pollfd pfd;
        pfd.fd = lsock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, 100);
        if (pr < 0) {
            if (errno == EINTR || g_stop) { break; }
            continue;
        }
        if (pr == 0) { continue; }

        int csock = accept(lsock, nullptr, nullptr);
        if (csock < 0) {
            if (errno == EINTR || g_stop) { break; }
            continue;
        }
        relay(csock, false);
        close(csock);
        if (!opts->keep) { break; }
    }

    close(lsock);
    return 0;
}


// ── Main command ────────────────────────────────────────────────────────────

int nc_command_impl(const char* prog, int argc, char** argv) {
    // Pre-scan for the negative-port syntax (host -8080) and glued
    // short-option values (-w2, -p12345, -i0.5, -c<cmd>).
    std::vector<std::string> pre_args;
    pre_args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        std::string a = argv[i];
        if (a.size() >= 2 && a[0] == '-' && a[1] != '-' && a[1] != '6' &&
            is_numeric(a.substr(1))) {
            // argtable3 treats "-<digits>" as an unknown option; the negative
            // form is only meaningful as a port, so strip the dash.
            pre_args.push_back(a.substr(1));
        } else if (a.size() >= 3 && a[0] == '-' && a[1] != '-') {
            char opt = a[1];
            if ((opt == 'w' || opt == 'p' || opt == 'i' || opt == 'c') && a[2] != '-') {
                pre_args.push_back(std::string("-") + opt);
                pre_args.push_back(a.substr(2));
            } else {
                pre_args.push_back(a);
            }
        } else {
            pre_args.push_back(a);
        }
    }

    std::vector<std::string> arg_store(pre_args.begin(), pre_args.end());
    std::vector<char*> new_argv;
    for (auto& s : arg_store) { new_argv.push_back(s.data()); }
    new_argv.push_back(nullptr);
    // arg_store already includes argv[0] (the program name), so the count is
    // the size itself — same convention the dispatcher hands every command.
    int new_argc = static_cast<int>(arg_store.size());

    g_stop = 0;
    SignalGuard signals;

    struct arg_lit* help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt =
        arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* listen_opt =
        arg_lit0("l", "listen", "listen for incoming connection");
    struct arg_lit* keep_opt =
        arg_lit0("k", "keep-open", "with -l, accept multiple clients");
    struct arg_lit* udp_opt =
        arg_lit0("u", "udp", "use UDP");
    struct arg_lit* zero_opt =
        arg_lit0("z", "zero-i/o", "scan for open ports");
    struct arg_lit* verbose_opt =
        arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* reuse_opt =
        arg_lit0("d", "reuseaddr", "set SO_REUSEADDR");
    struct arg_lit* ipv6_opt =
        arg_lit0("6", "ipv6", "prefer IPv6");
    struct arg_int* port_opt =
        arg_int0("p", "source-port", "<port>", "source port");
    struct arg_int* timeout_opt =
        arg_int0("w", "timeout", "<secs>", "connect timeout");
    struct arg_str* interval_opt =
        arg_str0("i", "delay", "<secs>", "delay between lines");
    struct arg_str* exec_opt =
        arg_str0("c", "exec", "<cmd>", "execute command after connect");
    struct arg_str* host_arg =
        arg_str0(nullptr, nullptr, "<host>", "destination host");
    struct arg_str* port_arg =
        arg_str0(nullptr, nullptr, "<port>", "destination port");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, listen_opt, keep_opt, udp_opt,
                 zero_opt, verbose_opt, reuse_opt, ipv6_opt,
                 port_opt, timeout_opt, interval_opt, exec_opt,
                 host_arg, port_arg, end});
    int nerrors = at.parse(new_argc, new_argv.data());

    if (help_opt->count > 0) {
        (void)printf(
            "Usage: %s [OPTIONS] [HOST] [PORT]\n\n"
            "Open a TCP/UDP connection and relay stdin/stdout.\n\n"
            "Options:\n"
            "  -c CMD          execute CMD after connection\n"
            "  -d              set SO_REUSEADDR\n"
            "  -i SECS         delay between lines from stdin\n"
            "  -k              with -l, keep accepting after each connection\n"
            "  -l              listen for incoming connection\n"
            "  -p PORT         source port\n"
            "  -u              use UDP\n"
            "  -v              verbose output\n"
            "  -w SECS         connect timeout\n"
            "  -z              zero-I/O scan for open ports\n"
            "  -6              prefer IPv6\n"
            "  -h, --help      display this help and exit\n"
            "  -V, --version   output version information and exit\n",
            prog);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("nc");
        return 0;
    }

    if (nerrors > 0) {
        print_arg_errors(end, prog);
        return 2;
    }

    // Build options struct from parsed args
    NCOptions opts;
    opts.listen = (listen_opt->count > 0);
    opts.keep = (keep_opt->count > 0);
    opts.udp = (udp_opt->count > 0);
    opts.zero_io = (zero_opt->count > 0);
    opts.verbose = (verbose_opt->count > 0);
    opts.reuse_addr = (reuse_opt->count > 0);
    opts.prefer_ipv6 = (ipv6_opt->count > 0);
    opts.timeout = timeout_opt->ival[0];
    opts.interval = interval_opt->count > 0 ? std::atof(interval_opt->sval[0]) : 0.0;
    opts.exec_cmd = exec_opt->count > 0 ? exec_opt->sval[0] : "";

    // Positional args: `host port` for connect mode, `port` for listen mode
    // (`nc -l 8080`). argtable3's arg_str0 groups take at most one value, so
    // the two positionals land in separate groups.
    if (opts.listen) {
        if (port_arg->count > 0 && is_numeric(port_arg->sval[0])) {
            opts.port = std::atoi(port_arg->sval[0]);
        } else if (host_arg->count > 0 && is_numeric(host_arg->sval[0])) {
            opts.port = std::atoi(host_arg->sval[0]);
        }
    } else {
        if (host_arg->count > 0) {
            opts.host = host_arg->sval[0];
        }
        if (port_arg->count > 0) {
            // Negative-port form (host -8080) is normalized to a plain
            // number by the argv pre-scan; atoi on a negative value fails
            // the port validation below.
            opts.port = std::atoi(port_arg->sval[0]);
        }
        if (port_opt->count > 0) {
            opts.source_port = port_opt->ival[0];
        }
    }

    if (opts.listen) {
        if (opts.port <= 0) {
            (void)fprintf(stderr, "%s: missing port\n", prog);
            return 2;
        }
    } else {
        if (opts.host.empty() || opts.port <= 0) {
            (void)fprintf(stderr, "%s: missing host or port\n", prog);
            return 2;
        }
    }

    if (opts.listen) {
        return run_listener(&opts);
    }

    Resolved resolved;
    if (!resolve_and_connect(&opts, resolved)) {
        return 1;
    }

    int fd = resolved.fd;
    const char* proto = opts.udp ? "udp" : "tcp";
    std::string service = std::to_string(opts.port);
    struct servent* se = getservbyport(opts.port, proto);
    if (se != nullptr && se->s_name != nullptr) { service = se->s_name; }

    // One success line per spec: -z prints it (host) to stdout; otherwise
    // -v prints it (resolved ip) to stderr.
    if (opts.zero_io) {
        (void)printf("Connection to %s %d port [%s/%s] succeeded!\n",
                     opts.host.c_str(), opts.port, proto, service.c_str());
        close(fd);
        return 0;
    }

    if (opts.verbose) {
        (void)fprintf(stderr, "Connection to %s %d port [%s/%s] succeeded!\n",
                      resolved.ip.c_str(), opts.port, proto, service.c_str());
    }
    if (opts.interval > 0.0) {
        char buf[65536];
        while (fgets(buf, sizeof(buf), stdin)) {
            if (g_stop) { break; }
            // Socket is always connected here (connect mode), so plain
            // send() works for both TCP and UDP.
            (void)send(fd, buf, strlen(buf), MSG_NOSIGNAL);
            usleep(static_cast<useconds_t>(opts.interval * 1000000));
        }
        close(fd);
        return 0;
    }

    if (!opts.exec_cmd.empty()) {
        const pid_t pid = fork();
        if (pid == 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            execl("/bin/sh", "sh", "-c", opts.exec_cmd.c_str(),
                  static_cast<char*>(nullptr));
            _exit(127);
        }
        if (pid < 0) {
            (void)fprintf(stderr, "nc: fork failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        int status;
        waitpid(pid, &status, 0);
        close(fd);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }

    relay(fd, opts.udp);
    close(fd);
    return 0;
}

}  // namespace

int nc_command(int argc, char** argv) {
    return nc_command_impl("nc", argc, argv);
}

static int netcat_command(int argc, char** argv) {
    return nc_command_impl("netcat", argc, argv);
}

REGISTER_COMMAND("nc", nc_command, "Open a TCP/UDP connection")
REGISTER_COMMAND("netcat", netcat_command, "Open a TCP/UDP connection")
