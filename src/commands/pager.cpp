#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <csignal>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <string>
#include <regex>

#include "commands/pager.hpp"

#define DEFAULT_TERM_HEIGHT 24
#define DEFAULT_TERM_WIDTH 80

// ── shared raw-mode / fd plumbing (legacy + less) ───────────────────────────

static struct termios orig_termios;
static int termios_saved = 0;
static int pager_kbd_fd = -1;

static void pager_restore_terminal(void) {
    if (termios_saved && pager_kbd_fd >= 0) {
        tcsetattr(pager_kbd_fd, TCSAFLUSH, &orig_termios);
        termios_saved = 0;
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
    termios_saved = 1;
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

static int get_term_height(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return ws.ws_row;
    }
    return DEFAULT_TERM_HEIGHT;
}

static int get_term_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return DEFAULT_TERM_WIDTH;
}

static int open_keyboard_fd(void) {
    if (isatty(STDIN_FILENO)) {
        return STDIN_FILENO;
    }
    int fd = open("/dev/tty", O_RDONLY);
    if (fd >= 0) {
        return fd;
    }
    return -1;
}

// Key codes above the ASCII range to distinguish escape sequences.
enum {
    KEY_UP = 1000,
    KEY_DOWN,
    KEY_PGUP,
    KEY_PGDOWN,
    KEY_ESC = 27
};

// Read one logical key, decoding the common xterm escape sequences.
static int read_key(int fd) {
    unsigned char c;
    if (read(fd, &c, 1) != 1) {
        return -1;
    }
    if (c == 27) {
        struct pollfd pfd{fd, POLLIN, 0};
        if (poll(&pfd, 1, 50) > 0) {
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
    return (int)c;
}

// ── legacy entry point (cat --less) — behaviour unchanged ──────────────────

void pager_run(const std::vector<std::string>& lines) {
    int total = (int)lines.size();
    if (total == 0) {
        return;
    }

    int kbd_fd = open_keyboard_fd();
    int need_close_kbd = (kbd_fd >= 0 && kbd_fd != STDIN_FILENO);

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

    int term_h = get_term_height();
    int display_rows = term_h - 1;
    if (display_rows < 1) { display_rows = 1; }

    int top = 0;
    int cursor = 0;

    while (1) {
        printf("\033[H\033[J");

        int end = top + display_rows;
        if (end > total) { end = total; }

        for (int i = top; i < end; i++) {
            if (i == cursor) {
                printf(">");
            } else {
                printf(" ");
            }
            printf("%s\n", lines[i].c_str());
        }

        int percent = (total > 1) ? (cursor * 100 / (total - 1)) : 0;
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
    int view;        // index into views (valid when !separator)
    int local_line;  // line within that view (valid when !separator)
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
    std::string status;        // transient message on the prompt line
    int status_ttl = 0;        // redraws before clearing status
    std::string last_pattern;  // for n / N
    int last_dir = 0;          // +1 forward, -1 backward

    explicit PagerState(const std::vector<PagerView>& v, const PagerOptions* c)
        : views(v), cfg(c) {}
    void build_rows() {
        rows.clear();
        for (size_t vi = 0; vi < views.size(); vi++) {
            if (vi > 0 && views.size() > 1) {
                rows.push_back({true, 0, 0, nullptr});
            }
            for (size_t li = 0; li < views[vi].lines.size(); li++) {
                rows.push_back({false, (int)vi, (int)li, &views[vi].lines[li]});
            }
        }
        total = (int)rows.size();
    }

    bool is_content(int i) const {
        return i >= 0 && i < total && !rows[i].separator;
    }

    int clamp_top() const {
        if (total <= display_rows) return 0;
        if (top > total - display_rows) return total - display_rows;
        if (top < 0) return 0;
        return top;
    }

    int gutter() const { return cfg->line_numbers ? 8 : 0; }

    bool do_search(int dir) {
        if (last_pattern.empty()) {
            status = "No previous regular expression";
            status_ttl = 2;
            return false;
        }
        std::regex re;
        try {
            bool apply_icase = cfg->force_case_insensitive ||
                               (cfg->smartcase && last_pattern.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos);
            re.assign(last_pattern,
                      apply_icase ? std::regex::icase : std::regex::ECMAScript);
        } catch (...) {
            status = "Invalid pattern";
            status_ttl = 2;
            return false;
        }
        int start = (dir > 0) ? cursor + 1 : cursor - 1;
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
        status_ttl = 2;
        return false;
    }

    bool read_pattern(int fd, bool backward) {
        std::string pat;
        while (1) {
            int k = read_key(fd);
            if (k == '\r' || k == '\n') break;
            if (k == KEY_ESC) { return false; }
            if (k == 127 || k == 8) {
                if (!pat.empty()) pat.pop_back();
            } else if (k >= 32 && k < 127) {
                pat.push_back((char)k);
            }
            draw_prompt(backward ? "?" : "/", pat);
        }
        last_pattern = pat;
        return !pat.empty();
    }

    void draw_prompt(const char* lead, const std::string& input) {
        int first = top;
        int lastv = top + display_rows - 1;
        if (lastv >= total) lastv = total - 1;
        int at_end = (cursor >= total - 1) ? 1 : 0;
        std::string fname;
        if (!views.empty()) {
            int v = is_content(cursor) ? rows[cursor].view : 0;
            fname = views[v].name;
        }

        std::string msg;
        if (!input.empty()) {
            msg = std::string(lead) + " " + input;
        } else if (!status.empty()) {
            msg = status;
        } else if (cfg->long_prompt) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "%s lines %d-%d/%d (%d%%)%s",
                     fname.c_str(), first + 1, lastv + 1, total,
                     total > 1 ? cursor * 100 / (total - 1) : 100,
                     at_end ? " (END)" : "");
            msg = buf;
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "lines %d-%d/%d (%d%%)%s",
                     first + 1, lastv + 1, total,
                     total > 1 ? cursor * 100 / (total - 1) : 100,
                     at_end ? " (END)" : "");
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
        if (avail < 1) avail = 1;

        int end = top + display_rows;
        if (end > total) end = total;

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
            if (cfg->chop_long_lines && (int)t.size() > avail) {
                t = t.substr(0, (size_t)avail);
            }
            printf("%s\n", t.c_str());
        }

        draw_prompt("", "");
        if (status_ttl > 0) {
            status_ttl--;
            if (status_ttl == 0) status.clear();
        }
        (void)fflush(stdout);
    }
};
};

// Plain (non-interactive) dump: used when stdout is not a TTY or raw mode
// can't be enabled. Honours -N and multi-file separators; everything else
// (search, -F, -X, -M, -S) is terminal-only and skipped.
void pager_dump(const std::vector<PagerView>& views, const PagerOptions* cfg) {
    for (size_t vi = 0; vi < views.size(); vi++) {
        if (vi > 0 && views.size() > 1) {
            printf(":::::::::::::: %s ::::::::::::::\n", views[vi].name.c_str());
        }
        for (size_t li = 0; li < views[vi].lines.size(); li++) {
            if (cfg->line_numbers) {
                printf("%7d %s\n", (int)li + 1, views[vi].lines[li].c_str());
            } else {
                printf("%s\n", views[vi].lines[li].c_str());
            }
        }
    }
}


namespace {
// Safe default so the pointer overload never dereferences null.
const PagerOptions kDefaultPagerOptions;
}  // namespace

void pager_run(const std::vector<PagerView>& views, const PagerOptions* cfg) {
    if (cfg == nullptr) cfg = &kDefaultPagerOptions;
    PagerState st(views, cfg);
    st.build_rows();
    if (st.total == 0) {
        return;
    }

    // Non-TTY: behave like cat (full dump), never enter interactive mode.
    if (!isatty(STDOUT_FILENO)) {
        pager_dump(views, cfg);
        return;
    }

    int kbd_fd = open_keyboard_fd();
    int need_close_kbd = (kbd_fd >= 0 && kbd_fd != STDIN_FILENO);
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
    if (st.display_rows < 1) { st.display_rows = 1; }

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
        st.cursor = (int)cfg->start_line - 1;
    }
    st.top = st.cursor - st.display_rows / 2;
    st.top = st.clamp_top();

    // Initial search (-p / +/pat): start from before the first row so a
    // match on line 1 is found; if no match, fall back to the top.
    if (!cfg->pattern.empty()) {
        st.last_pattern = cfg->pattern;
        st.last_dir = 1;
        int saved = st.cursor;
        st.cursor = -1;
        if (!st.do_search(1)) {
            st.cursor = saved;
        }
    }

    while (1) {
        st.top = st.clamp_top();
        st.render();

        int k = read_key(kbd_fd);
        if (k < 0) break;

        if (k == 'q') break;

        switch (k) {
            case 'j':
            case KEY_DOWN:
            case 'e':
            case '\r':
            case '\n':
            case 'y':
                if (st.cursor < st.total - 1) {
                    st.cursor++;
                    if (st.cursor >= st.top + st.display_rows) st.top++;
                } else if (cfg->quit_at_eof) {
                    goto done;
                }
                break;
            case 'k':
            case KEY_UP:
                if (st.cursor > 0) {
                    st.cursor--;
                    if (st.cursor < st.top) st.top--;
                }
                break;
            case ' ':
            case 'f':
            case KEY_PGDOWN:
                if (st.cursor < st.total - 1) {
                    st.cursor += st.display_rows;
                    st.top += st.display_rows;
                    if (st.cursor > st.total - 1) st.cursor = st.total - 1;
                } else if (cfg->quit_at_eof) {
                    goto done;
                }
                break;
            case 'b':
            case KEY_PGUP:
                st.cursor -= st.display_rows;
                st.top -= st.display_rows;
                if (st.cursor < 0) st.cursor = 0;
                break;
            case 'g':
            case '<':
                st.cursor = 0;
                st.top = 0;
                break;
            case 'G':
            case '>':
                st.cursor = st.total - 1;
                st.top = st.total - st.display_rows;
                break;
            case '/':
                if (st.read_pattern(kbd_fd, false)) {
                    st.last_dir = 1;
                    if (st.do_search(1)) {
                        st.top = st.cursor - st.display_rows / 2;
                    }
                }
                break;
            case '?':
                if (st.read_pattern(kbd_fd, true)) {
                    st.last_dir = -1;
                    if (st.do_search(-1)) {
                        st.top = st.cursor - st.display_rows / 2;
                    }
                }
                break;
            case 'n':
                if (st.last_dir != 0) {
                    if (st.do_search(st.last_dir)) st.top = st.cursor - st.display_rows / 2;
                }
                break;
            case 'N':
                if (st.last_dir != 0) {
                    if (st.do_search(-st.last_dir)) st.top = st.cursor - st.display_rows / 2;
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
