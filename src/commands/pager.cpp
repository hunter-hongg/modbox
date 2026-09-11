#include <asm-generic/ioctls.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/poll.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <csignal>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>

#include "commands/pager.hpp"

#define DEFAULT_TERM_HEIGHT 24
#define DEFAULT_TERM_WIDTH 80

constexpr int kPollMs = 50;
constexpr int kBackspace = 127;
constexpr int kDelete = 8;
constexpr int kBufSize = 256;
constexpr int kStatusTtl = 2;

// ── shared raw-mode / fd plumbing (legacy + less) ───────────────────────────

static struct termios orig_termios;
static bool termios_saved = false;
static int pager_kbd_fd = -1;

static void pager_restore_terminal() {
    if (termios_saved && pager_kbd_fd >= 0) {
        tcsetattr(pager_kbd_fd, TCSAFLUSH, &orig_termios);
        termios_saved = false;
    }
}

static void pager_signal_handler(int sig) {
    pager_restore_terminal();
    (void)signal(sig, SIG_DFL);
    (void)raise(sig);
}

static int pager_enable_raw_mode(int fd) {
    struct termios raw;
    if (tcgetattr(fd, &orig_termios) < 0) {
        return -1;
    }
    pager_kbd_fd = fd;
    termios_saved = true;
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) {
        return -1;
    }
    return 0;
}

static int get_term_height() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return ws.ws_row;
    }
    return DEFAULT_TERM_HEIGHT;
}

static int get_term_width() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return DEFAULT_TERM_WIDTH;
}

static int open_keyboard_fd() {
    if (isatty(STDIN_FILENO) != 0) {
        return STDIN_FILENO;
    }
    const int fd = open("/dev/tty", O_RDONLY);
    if (fd >= 0) {
        return fd;
    }
    return -1;
}

// Key codes above the ASCII range to distinguish escape sequences.
enum {
    KEY_UP     = 1000,
    KEY_DOWN   = 1001,
    KEY_PGUP   = 1002,
    KEY_PGDOWN = 1003,
    KEY_ESC    = 27
};

// Read one logical key, decoding the common xterm escape sequences.
static int read_key(int fd) {
    unsigned char c;
    if (read(fd, &c, 1) != 1) {
        return -1;
    }
    if (c == static_cast<unsigned char>(KEY_ESC)) {
        struct pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
        if (poll(&pfd, 1, kPollMs) > 0) {
            unsigned char c2;
            if (read(fd, &c2, 1) == 1 && c2 == '[') {
                unsigned char c3;
                if (read(fd, &c3, 1) == 1) {
                    switch (c3) {
                        case 'A': return KEY_UP;
                        case 'B': return KEY_DOWN;
                        case '5': {
                            unsigned char c4;
                            (void)read(fd, &c4, 1);
                            return KEY_PGUP;
                        }
                        case '6': {
                            unsigned char c4;
                            (void)read(fd, &c4, 1);
                            return KEY_PGDOWN;
                        }
                        default: break;
                    }
                }
            }
        }
        return KEY_ESC;
    }
    return static_cast<int>(c);
}

// ── legacy entry point (cat --less) — behaviour unchanged ──────────────────

void pager_run(const std::vector<std::string>& lines) {
    const int total = static_cast<int>(lines.size());
    if (total == 0) {
        return;
    }

    const int kbd_fd = open_keyboard_fd();
    const bool need_close_kbd = (kbd_fd >= 0 && kbd_fd != STDIN_FILENO);

    if (kbd_fd < 0) {
        for (int i = 0; i < total; i++) {
            printf("%s\n", lines[i].c_str());
        }
        return;
    }

    if (pager_enable_raw_mode(kbd_fd) < 0) {
        if (need_close_kbd) { close(kbd_fd); }
        for (int i = 0; i < total; i++) {
            printf("%s\n", lines[i].c_str());
        }
        return;
    }

    (void)atexit(pager_restore_terminal);
    (void)signal(SIGINT, pager_signal_handler);
    (void)signal(SIGTERM, pager_signal_handler);
    (void)signal(SIGQUIT, pager_signal_handler);

    const int term_h = get_term_height();
    int display_rows = term_h - 1;
    display_rows = std::max(display_rows, 1);

    int top = 0;
    int cursor = 0;

    while (true) {
        printf("\033[H\033[J");

        int end = top + display_rows;
        end = std::min(end, total);

        for (int i = top; i < end; i++) {
            if (i == cursor) {
                printf(">");
            } else {
                printf(" ");
            }
            printf("%s\n", lines[i].c_str());
        }

        const int percent = (total > 1) ? (cursor * 100 / (total - 1)) : 0;
        printf("\033[7m--modbox-- line %d of %d (%d%%) -- j:down k:up q:quit\033[0m",
               cursor + 1, total, percent);
        (void)fflush(stdout);

        char c;
        if (read(kbd_fd, &c, 1) != 1) {
            break;
        }

        if (c == 'q') {
            break;
        }

        if (c == 'j' && cursor < total - 1) {
            cursor++;
            if (cursor >= top + display_rows) {
                top++;
            }
        } else if (c == 'k' && cursor > 0) {
            cursor--;
            if (cursor < top) {
                top--;
            }
        }
    }

    pager_restore_terminal();

    if (need_close_kbd) {
        close(kbd_fd);
    }

    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGTERM, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);
}

// ── configurable entry point (less) ─────────────────────────────────────────

namespace {

struct Row {
    bool separator;
    int view;
    int local_line;
    const std::string* text;
};

struct PagerState {
    const std::vector<PagerView>& views;
    const PagerOptions* cfg;
    std::vector<Row> rows;
    int total = 0;
    int term_h = DEFAULT_TERM_HEIGHT;
    int term_w = DEFAULT_TERM_WIDTH;
    int display_rows = 1;
    int top = 0;
    int cursor = 0;
    std::string status;
    int status_ttl = 0;
    std::string last_pattern;
    int last_dir = 0;

    explicit PagerState(const std::vector<PagerView>& v, const PagerOptions* c)
        : views(v), cfg(c) {}

    void build_rows() {
        rows.clear();
        for (size_t vi = 0; vi < views.size(); vi++) {
            if (vi > 0 && views.size() > 1) {
                rows.push_back({.separator = true, .view = 0, .local_line = 0, .text = nullptr});
            }
            for (size_t li = 0; li < views[vi].lines.size(); li++) {
                rows.push_back({.separator = false,
                                .view = static_cast<int>(vi),
                                .local_line = static_cast<int>(li),
                                .text = &views[vi].lines[li]});
            }
        }
        total = static_cast<int>(rows.size());
    }

    [[nodiscard]] bool is_content(int i) const {
        return i >= 0 && i < total && !rows[i].separator;
    }

    [[nodiscard]] int clamp_top() const {
        if (total <= display_rows) { return 0; }
        if (top > total - display_rows) { return total - display_rows; }
        if (top < 0) { return 0; }
        return top;
    }

    [[nodiscard]] int gutter() const { return cfg->line_numbers ? 8 : 0; }

    bool do_search(int dir) {
        if (last_pattern.empty()) {
            status = "No previous regular expression";
            status_ttl = kStatusTtl;
            return false;
        }
        std::regex re;
        try {
            const bool apply_icase = cfg->force_case_insensitive ||
                               (cfg->smartcase &&
                                last_pattern.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos);
            re.assign(last_pattern,
                      apply_icase ? std::regex::icase : std::regex::ECMAScript);
        } catch (...) {
            status = "Invalid pattern";
            status_ttl = kStatusTtl;
            return false;
        }
        const int start = (dir > 0) ? cursor + 1 : cursor - 1;
        if (dir > 0) {
            for (int i = start; i < total; i++) {
                if (is_content(i) && std::regex_search(*rows[i].text, re)) {
                    cursor = i;
                    return true;
                }
            }
        } else {
            for (int i = start; i >= 0; i--) {
                if (is_content(i) && std::regex_search(*rows[i].text, re)) {
                    cursor = i;
                    return true;
                }
            }
        }
        status = "Pattern not found";
        status_ttl = kStatusTtl;
        return false;
    }

    bool read_pattern(int fd, bool backward) {
        std::string pat;
        while (true) {
            const int k = read_key(fd);
            if (k == '\r' || k == '\n') { break; }
            if (k == KEY_ESC) { return false; }
            if (k == kBackspace || k == kDelete) {
                if (!pat.empty()) { pat.pop_back(); }
            } else if (k >= 32 && k < kBackspace) {
                pat.push_back(static_cast<char>(k));
            }
            draw_prompt(backward ? "?" : "/", pat);
        }
        last_pattern = pat;
        return !pat.empty();
    }

    void draw_prompt(const char* lead, const std::string& input) {
        const int first = top;
        int lastv = top + display_rows - 1;
        if (lastv >= total) { lastv = total - 1; }
        const int at_end = (cursor >= total - 1) ? 1 : 0;
        std::string fname;
        if (!views.empty()) {
            const int v = is_content(cursor) ? rows[cursor].view : 0;
            fname = views[v].name;
        }

        std::string msg;
        char buf[kBufSize];
        if (!input.empty()) {
            msg = std::string(lead) + " " + input;
        } else if (!status.empty()) {
            msg = status;
        } else if (cfg->long_prompt) {
            (void)snprintf(buf, sizeof(buf),
                     "%s lines %d-%d/%d (%d%%)%s",
                     fname.c_str(), first + 1, lastv + 1, total,
                     total > 1 ? cursor * 100 / (total - 1) : 100,
                     (at_end != 0) ? " (END)" : "");
            msg = buf;
        } else {
            (void)snprintf(buf, sizeof(buf), "lines %d-%d/%d (%d%%)%s",
                     first + 1, lastv + 1, total,
                     total > 1 ? cursor * 100 / (total - 1) : 100,
                     (at_end != 0) ? " (END)" : "");
            msg = buf;
        }
        printf("\033[7m%s\033[0m", msg.c_str());
        (void)fflush(stdout);
    }

    void render() {
        if (!cfg->no_init) {
            printf("\033[H\033[J");
        } else {
            printf("\033[J");
        }

        int avail = term_w - 1 - gutter();
        avail = std::max(avail, 1);

        int end = top + display_rows;
        end = std::min(end, total);

        for (int i = top; i < end; i++) {
            if (rows[i].separator) {
                int v = 0;
                for (int j = i + 1; j < total; j++) {
                    if (!rows[j].separator) { v = rows[j].view; break; }
                }
                if (!views[v].name.empty()) {
                    printf(":::::::::::::: %s ::::::::::::::\n", views[v].name.c_str());
                } else {
                    printf("\n");
                }
                continue;
            }
            if (i == cursor) {
                printf(">");
            } else {
                printf(" ");
            }
            if (cfg->line_numbers) {
                printf("%7d ", rows[i].local_line + 1);
            }
            std::string t = *rows[i].text;
            if (cfg->chop_long_lines && t.size() > static_cast<size_t>(avail)) {
                t = t.substr(0, static_cast<size_t>(avail));
            }
            printf("%s\n", t.c_str());
        }

        draw_prompt("", "");
        if (status_ttl > 0) {
            status_ttl--;
            if (status_ttl == 0) { status.clear(); }
        }
        (void)fflush(stdout);
    }
};

void pager_dump(const std::vector<PagerView>& views, const PagerOptions* cfg) {
    for (size_t vi = 0; vi < views.size(); vi++) {
        if (vi > 0 && views.size() > 1) {
            printf(":::::::::::::: %s ::::::::::::::\n", views[vi].name.c_str());
        }
        for (size_t li = 0; li < views[vi].lines.size(); li++) {
            if (cfg->line_numbers) {
                printf("%7d %s\n", static_cast<int>(li) + 1, views[vi].lines[li].c_str());
            } else {
                printf("%s\n", views[vi].lines[li].c_str());
            }
        }
    }
}

const PagerOptions kDefaultPagerOptions;

}  // namespace

void pager_run(const std::vector<PagerView>& views, const PagerOptions* cfg) {
    if (cfg == nullptr) { cfg = &kDefaultPagerOptions; }
    PagerState st(views, cfg);
    st.build_rows();
    if (st.total == 0) {
        return;
    }

    // Non-TTY: behave like cat (full dump), never enter interactive mode.
    if (isatty(STDOUT_FILENO) == 0) {
        pager_dump(views, cfg);
        return;
    }

    const int kbd_fd = open_keyboard_fd();
    const bool need_close_kbd = (kbd_fd >= 0 && kbd_fd != STDIN_FILENO);
    if (kbd_fd < 0 || pager_enable_raw_mode(kbd_fd) < 0) {
        if (need_close_kbd) { close(kbd_fd); }
        pager_dump(views, cfg);
        return;
    }

    (void)atexit(pager_restore_terminal);
    (void)signal(SIGINT, pager_signal_handler);
    (void)signal(SIGTERM, pager_signal_handler);
    (void)signal(SIGQUIT, pager_signal_handler);

    st.term_h = get_term_height();
    st.term_w = get_term_width();
    st.display_rows = st.term_h - 1;
    st.display_rows = std::max(st.display_rows, 1);

    // -F: quit immediately if everything fits on one screen.
    if (cfg->quit_if_one_screen && st.total <= st.display_rows) {
        pager_dump(views, cfg);
        pager_restore_terminal();
        if (need_close_kbd) { close(kbd_fd); }
        return;
    }

    // Initial position: +G / +N / startup line.
    if (cfg->start_line <= 0) {
        st.cursor = st.total - 1;
    } else if (cfg->start_line > st.total) {
        st.cursor = st.total - 1;
    } else {
        st.cursor = static_cast<int>(cfg->start_line) - 1;
    }
    st.top = st.cursor - (st.display_rows / 2);
    st.top = st.clamp_top();

    // Initial search (-p / +/pat): start from before the first row so a
    // match on line 1 is found; if no match, fall back to the top.
    if (!cfg->pattern.empty()) {
        st.last_pattern = cfg->pattern;
        st.last_dir = 1;
        const int saved = st.cursor;
        st.cursor = -1;
        if (!st.do_search(1)) {
            st.cursor = saved;
        }
    }

    while (true) {
        st.top = st.clamp_top();
        st.render();

        const int k = read_key(kbd_fd);
        if (k < 0) { break; }

        if (k == 'q') { break; }

        switch (k) {
            case 'j':
            case KEY_DOWN:
            case 'e':
            case '\r':
            case '\n':
            case 'y':
                if (st.cursor < st.total - 1) {
                    st.cursor++;
                    if (st.cursor >= st.top + st.display_rows) { st.top++; }
                } else if (cfg->quit_at_eof) {
                    goto done;
                }
                break;
            case 'k':
            case KEY_UP:
                if (st.cursor > 0) {
                    st.cursor--;
                    if (st.cursor < st.top) { st.top--; }
                }
                break;
            case ' ':
            case 'f':
            case KEY_PGDOWN:
                if (st.cursor < st.total - 1) {
                    st.cursor += st.display_rows;
                    st.top += st.display_rows;
                    st.cursor = std::min(st.cursor, st.total - 1);
                } else if (cfg->quit_at_eof) {
                    goto done;
                }
                break;
            case 'b':
            case KEY_PGUP:
                st.cursor -= st.display_rows;
                st.top -= st.display_rows;
                st.cursor = std::max(st.cursor, 0);
                break;
            case 'g':
            case '<':
                st.cursor = 0;
                st.top = 0;
                break;
            case 'G':
            case '>':
                st.cursor = st.total - 1;
                st.top = std::max(st.total - st.display_rows, 0);
                break;
            case '/':
                if (st.read_pattern(kbd_fd, false)) {
                    st.last_dir = 1;
                    if (st.do_search(1)) {
                        st.top = st.cursor - (st.display_rows / 2);
                    }
                }
                break;
            case '?':
                if (st.read_pattern(kbd_fd, true)) {
                    st.last_dir = -1;
                    if (st.do_search(-1)) {
                        st.top = st.cursor - (st.display_rows / 2);
                    }
                }
                break;
            case 'n':
                if (st.last_dir != 0) {
                    if (st.do_search(st.last_dir)) {
                        st.top = st.cursor - (st.display_rows / 2);
                    }
                }
                break;
            case 'N':
                if (st.last_dir != 0) {
                    if (st.do_search(-st.last_dir)) {
                        st.top = st.cursor - (st.display_rows / 2);
                    }
                }
                break;
            case KEY_ESC:
            default:
                break;
        }
    }

done:
    pager_restore_terminal();
    if (need_close_kbd) { close(kbd_fd); }
    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGTERM, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);
}
