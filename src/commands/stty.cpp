#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <string>
#include <vector>

#include "commands/stty.hpp"
#include "commands/command_macros.hpp"

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif

static const char* g_prog = "stty";

static int g_fd = STDIN_FILENO;
static struct termios g_mode;

/* ── Baud rate tables ─────────────────────────────────────────────────────── */

struct BaudEntry {
    speed_t code;
    int num;
};

static const BaudEntry kBauds[] = {
#ifdef B0
    {B0, 0},
#endif
#ifdef B50
    {B50, 50},
#endif
#ifdef B75
    {B75, 75},
#endif
#ifdef B110
    {B110, 110},
#endif
#ifdef B134
    {B134, 134},
#endif
#ifdef B150
    {B150, 150},
#endif
#ifdef B200
    {B200, 200},
#endif
#ifdef B300
    {B300, 300},
#endif
#ifdef B600
    {B600, 600},
#endif
#ifdef B1200
    {B1200, 1200},
#endif
#ifdef B1800
    {B1800, 1800},
#endif
#ifdef B2400
    {B2400, 2400},
#endif
#ifdef B4800
    {B4800, 4800},
#endif
#ifdef B9600
    {B9600, 9600},
#endif
#ifdef B19200
    {B19200, 19200},
#endif
#ifdef B38400
    {B38400, 38400},
#endif
#ifdef B57600
    {B57600, 57600},
#endif
#ifdef B115200
    {B115200, 115200},
#endif
#ifdef B230400
    {B230400, 230400},
#endif
#ifdef B460800
    {B460800, 460800},
#endif
#ifdef B500000
    {B500000, 500000},
#endif
#ifdef B576000
    {B576000, 576000},
#endif
#ifdef B921600
    {B921600, 921600},
#endif
#ifdef B1000000
    {B1000000, 1000000},
#endif
#ifdef B1152000
    {B1152000, 1152000},
#endif
#ifdef B1500000
    {B1500000, 1500000},
#endif
#ifdef B2000000
    {B2000000, 2000000},
#endif
#ifdef B2500000
    {B2500000, 2500000},
#endif
#ifdef B3000000
    {B3000000, 3000000},
#endif
#ifdef B3500000
    {B3500000, 3500000},
#endif
#ifdef B4000000
    {B4000000, 4000000},
#endif
};

static speed_t encode_baud(int n) {
    for (const auto& b : kBauds) {
        if (b.num == n) return b.code;
    }
    return static_cast<speed_t>(-1);
}

static int decode_baud(speed_t code) {
    for (const auto& b : kBauds) {
        if (b.code == code) return b.num;
    }
    return -1;
}

/* ── Character (special control char) printing/parsing ───────────────────── */

static void sprint_char(char* buf, size_t buflen, unsigned char c) {
    if (c == static_cast<unsigned char>(_POSIX_VDISABLE)) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buflen, "<undef>");
    } else if (c == 127) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buflen, "^?");
    } else if (c < 32) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buflen, "^%c", (char)(c + 64));
    } else if (c == ' ') {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buflen, " ");
    } else if (isprint(c)) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buflen, "%c", (char)c);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(buf, buflen, "\\%03o", (int)c);
    }
}

static int parse_char(const char* s) {
    if (s == NULL) return -1;
    if (strcmp(s, "-") == 0 || strcmp(s, "undef") == 0 ||
        strcmp(s, "<undef>") == 0) {
        return _POSIX_VDISABLE;
    }
    if (s[0] == '^' && s[1] != '\0' && s[2] == '\0') {
        if (s[1] == '?') return 127;
        if (s[1] >= '@' && s[1] <= '_') return s[1] - 64;
        return -1;
    }
    if (s[0] == '\\') {
        int val = 0;
        const char* p = s + 1;
        int count = 0;
        while (*p >= '0' && *p <= '7' && count < 3) {
            val = val * 8 + (*p - '0');
            p++;
            count++;
        }
        if (count > 0) return val & 0xff;
        return -1;
    }
    if (s[0] != '\0' && s[1] == '\0') {
        return (unsigned char)s[0];
    }
    return -1;
}

/* ── Flag mode table ──────────────────────────────────────────────────────── */

enum Group { G_IFLAG = 0, G_OFLAG, G_CFLAG, G_LFLAG };

struct FlagMode {
    const char* name;
    int group;
    tcflag_t clr_mask;
    tcflag_t set_mask;
    bool sane_on;
};

#define FM(g, name, clr, set, sane) \
    { name, g, (tcflag_t)(clr), (tcflag_t)(set), sane }

static const FlagMode kControlModes[] = {
    FM(G_CFLAG, "parenb", 0, PARENB, false),
    FM(G_CFLAG, "parodd", 0, PARODD, false),
#ifdef CMSPAR
    FM(G_CFLAG, "cmspar", 0, CMSPAR, false),
#endif
    FM(G_CFLAG, "cs5", CSIZE, CS5, false),
    FM(G_CFLAG, "cs6", CSIZE, CS6, false),
    FM(G_CFLAG, "cs7", CSIZE, CS7, false),
    FM(G_CFLAG, "cs8", CSIZE, CS8, true),
    FM(G_CFLAG, "cstopb", 0, CSTOPB, false),
    FM(G_CFLAG, "cread", 0, CREAD, true),
    FM(G_CFLAG, "clocal", 0, CLOCAL, false),
    FM(G_CFLAG, "hupcl", 0, HUPCL, false),
#ifdef CRTSCTS
    FM(G_CFLAG, "crtscts", 0, CRTSCTS, false),
#endif
};

static const FlagMode kInputModes[] = {
    FM(G_IFLAG, "ignbrk", 0, IGNBRK, false),
    FM(G_IFLAG, "brkint", 0, BRKINT, true),
    FM(G_IFLAG, "ignpar", 0, IGNPAR, false),
    FM(G_IFLAG, "parmrk", 0, PARMRK, false),
    FM(G_IFLAG, "inpck", 0, INPCK, false),
    FM(G_IFLAG, "istrip", 0, ISTRIP, false),
    FM(G_IFLAG, "inlcr", 0, INLCR, false),
    FM(G_IFLAG, "igncr", 0, IGNCR, false),
    FM(G_IFLAG, "icrnl", 0, ICRNL, true),
    FM(G_IFLAG, "ixon", 0, IXON, false),
    FM(G_IFLAG, "ixoff", 0, IXOFF, false),
    FM(G_IFLAG, "iuclc", 0, IUCLC, false),
    FM(G_IFLAG, "ixany", 0, IXANY, false),
    FM(G_IFLAG, "imaxbel", 0, IMAXBEL, true),
#ifdef IUTF8
    FM(G_IFLAG, "iutf8", 0, IUTF8, true),
#endif
};

static const FlagMode kOutputModes[] = {
    FM(G_OFLAG, "opost", 0, OPOST, true),
    FM(G_OFLAG, "olcuc", 0, OLCUC, false),
    FM(G_OFLAG, "ocrnl", 0, OCRNL, false),
    FM(G_OFLAG, "onlcr", 0, ONLCR, true),
    FM(G_OFLAG, "onocr", 0, ONOCR, false),
    FM(G_OFLAG, "onlret", 0, ONLRET, false),
    FM(G_OFLAG, "ofill", 0, OFILL, false),
    FM(G_OFLAG, "ofdel", 0, OFDEL, false),
    FM(G_OFLAG, "nl0", NLDLY, NL0, true),
    FM(G_OFLAG, "nl1", NLDLY, NL1, false),
    FM(G_OFLAG, "cr0", CRDLY, CR0, true),
    FM(G_OFLAG, "cr1", CRDLY, CR1, false),
    FM(G_OFLAG, "cr2", CRDLY, CR2, false),
    FM(G_OFLAG, "cr3", CRDLY, CR3, false),
    FM(G_OFLAG, "tab0", TABDLY, TAB0, true),
    FM(G_OFLAG, "tab1", TABDLY, TAB1, false),
    FM(G_OFLAG, "tab2", TABDLY, TAB2, false),
    FM(G_OFLAG, "tab3", TABDLY, TAB3, false),
    FM(G_OFLAG, "bs0", BSDLY, BS0, true),
    FM(G_OFLAG, "bs1", BSDLY, BS1, false),
    FM(G_OFLAG, "vt0", VTDLY, VT0, true),
    FM(G_OFLAG, "vt1", VTDLY, VT1, false),
    FM(G_OFLAG, "ff0", FFDLY, FF0, true),
    FM(G_OFLAG, "ff1", FFDLY, FF1, false),
};

static const FlagMode kLocalModes[] = {
    FM(G_LFLAG, "isig", 0, ISIG, true),
    FM(G_LFLAG, "icanon", 0, ICANON, true),
#ifdef XCASE
    FM(G_LFLAG, "xcase", 0, XCASE, false),
#endif
    FM(G_LFLAG, "echo", 0, ECHO, true),
    FM(G_LFLAG, "echoe", 0, ECHOE, true),
    FM(G_LFLAG, "echok", 0, ECHOK, true),
    FM(G_LFLAG, "echonl", 0, ECHONL, false),
    FM(G_LFLAG, "echoprt", 0, ECHOPRT, false),
    FM(G_LFLAG, "echoctl", 0, ECHOCTL, true),
    FM(G_LFLAG, "echoke", 0, ECHOKE, true),
    FM(G_LFLAG, "flusho", 0, FLUSHO, false),
    FM(G_LFLAG, "noflsh", 0, NOFLSH, false),
#ifdef TOSTOP
    FM(G_LFLAG, "tostop", 0, TOSTOP, false),
#endif
    FM(G_LFLAG, "iexten", 0, IEXTEN, true),
#ifdef EXTPROC
    FM(G_LFLAG, "extproc", 0, EXTPROC, false),
#endif
};

#undef FM

/* ── Special character table ──────────────────────────────────────────────── */

struct CharMode {
    const char* name;
    int idx;
};

static std::vector<CharMode> build_charmodes(void) {
    std::vector<CharMode> v;
#define ADD(nm, idx) v.push_back({nm, idx})
#ifdef VINTR
    ADD("intr", VINTR);
#endif
#ifdef VQUIT
    ADD("quit", VQUIT);
#endif
#ifdef VERASE
    ADD("erase", VERASE);
#endif
#ifdef VKILL
    ADD("kill", VKILL);
#endif
#ifdef VEOF
    ADD("eof", VEOF);
#endif
#ifdef VEOL
    ADD("eol", VEOL);
#endif
#ifdef VEOL2
    ADD("eol2", VEOL2);
#endif
#ifdef VSWTC
    ADD("swtch", VSWTC);
#elif defined(VSWTCH)
    ADD("swtch", VSWTCH);
#endif
#ifdef VSTART
    ADD("start", VSTART);
#endif
#ifdef VSTOP
    ADD("stop", VSTOP);
#endif
#ifdef VSUSP
    ADD("susp", VSUSP);
#endif
#ifdef VREPRINT
    ADD("rprnt", VREPRINT);
#elif defined(VRPRNT)
    ADD("rprnt", VRPRNT);
#endif
#ifdef VWERASE
    ADD("werase", VWERASE);
#endif
#ifdef VLNEXT
    ADD("lnext", VLNEXT);
#endif
#ifdef VDISCARD
    ADD("discard", VDISCARD);
#elif defined(VFLUSHO)
    ADD("discard", VFLUSHO);
#endif
#undef ADD
    return v;
}

/* ── Flag helpers ─────────────────────────────────────────────────────────── */

static tcflag_t* group_ptr(int group) {
    switch (group) {
        case G_IFLAG: return &g_mode.c_iflag;
        case G_OFLAG: return &g_mode.c_oflag;
        case G_CFLAG: return &g_mode.c_cflag;
        case G_LFLAG: return &g_mode.c_lflag;
        default: return NULL;
    }
}

static tcflag_t* group_ptr_of(const struct termios* t, int group) {
    switch (group) {
        case G_IFLAG: return (tcflag_t*)&t->c_iflag;
        case G_OFLAG: return (tcflag_t*)&t->c_oflag;
        case G_CFLAG: return (tcflag_t*)&t->c_cflag;
        case G_LFLAG: return (tcflag_t*)&t->c_lflag;
        default: return NULL;
    }
}

static bool mode_on(const struct termios* t, const FlagMode* fm) {
    tcflag_t f = *group_ptr_of(t, fm->group);
    if (fm->clr_mask == 0) {
        return (f & fm->set_mask) != 0;
    }
    return (f & fm->clr_mask) == fm->set_mask;
}

static void apply_mode(const FlagMode* fm, bool on) {
    tcflag_t* f = group_ptr(fm->group);
    if (fm->clr_mask != 0) {
        *f &= ~fm->clr_mask;
        if (on) *f |= fm->set_mask;
    } else {
        if (on) *f |= fm->set_mask;
        else *f &= ~fm->set_mask;
    }
}

static const FlagMode* find_mode(const char* name) {
    for (const auto& fm : kControlModes) if (strcmp(fm.name, name) == 0) return &fm;
    for (const auto& fm : kInputModes) if (strcmp(fm.name, name) == 0) return &fm;
    for (const auto& fm : kOutputModes) if (strcmp(fm.name, name) == 0) return &fm;
    for (const auto& fm : kLocalModes) if (strcmp(fm.name, name) == 0) return &fm;
    return NULL;
}

/* ── Presets: sane / raw ──────────────────────────────────────────────────── */

static void apply_raw(struct termios* t) {
#ifdef HAVE_CFMAKERAW
    cfmakeraw(t);
#else
    t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->c_cflag &= ~(CSIZE | PARENB);
    t->c_cflag |= CS8;
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
#endif
}

static void apply_sane(struct termios* t) {
    static const char* sane_list[] = {
        "cread", "-cstopb", "cs8", "-parenb", "-parodd",
#ifdef CMSPAR
        "-cmspar",
#endif
#ifdef CRTSCTS
        "-crtscts",
#endif
        "-hupcl", "-clocal",
        "-ignbrk", "brkint", "-ignpar", "-parmrk", "-inpck", "-istrip",
        "-inlcr", "-igncr", "icrnl", "-ixon", "-ixoff", "-iuclc", "-ixany",
        "-imaxbel",
#ifdef IUTF8
        "-iutf8",
#endif
        "-opost", "-olcuc", "-ocrnl", "onlcr", "-onocr", "-onlret", "-ofill",
        "-ofdel", "nl0", "cr0", "tab0", "bs0", "vt0", "ff0",
        "isig", "icanon", "iexten", "echo", "echoe", "echok", "-echonl",
        "-echoprt", "echoctl", "echoke", "-flusho", "-noflsh",
#ifdef EXTPROC
        "-extproc",
#endif
        NULL
    };
    for (const char** p = sane_list; *p != NULL; p++) {
        const char* name = *p;
        bool on = true;
        if (name[0] == '-' && name[1] != '\0') {
            on = false;
            name++;
        }
        const FlagMode* fm = find_mode(name);
        if (fm == NULL) continue;
        tcflag_t* f = group_ptr_of(t, fm->group);
        if (fm->clr_mask != 0) {
            *f &= ~fm->clr_mask;
            if (on) *f |= fm->set_mask;
        } else {
            if (on) *f |= fm->set_mask;
            else *f &= ~fm->set_mask;
        }
    }

    static const struct { const char* name; int val; } sane_cc[] = {
#ifdef VINTR
        {"intr", 3},
#endif
#ifdef VQUIT
        {"quit", 28},
#endif
#ifdef VERASE
        {"erase", 127},
#endif
#ifdef VKILL
        {"kill", 21},
#endif
#ifdef VEOF
        {"eof", 4},
#endif
#ifdef VEOL
        {"eol", _POSIX_VDISABLE},
#endif
#ifdef VEOL2
        {"eol2", _POSIX_VDISABLE},
#endif
#ifdef VSWTC
        {"swtch", _POSIX_VDISABLE},
#elif defined(VSWTCH)
        {"swtch", _POSIX_VDISABLE},
#endif
#ifdef VSTART
        {"start", 17},
#endif
#ifdef VSTOP
        {"stop", 19},
#endif
#ifdef VSUSP
        {"susp", 26},
#endif
#ifdef VREPRINT
        {"rprnt", 18},
#elif defined(VRPRNT)
        {"rprnt", 18},
#endif
#ifdef VWERASE
        {"werase", 23},
#endif
#ifdef VLNEXT
        {"lnext", 22},
#endif
#ifdef VDISCARD
        {"discard", 15},
#elif defined(VFLUSHO)
        {"discard", 15},
#endif
    };
    std::vector<CharMode> cms = build_charmodes();
    for (const auto& c : sane_cc) {
        std::string n = c.name;
        for (const auto& m : cms) {
            if (n == m.name) {
                t->c_cc[m.idx] = (cc_t)c.val;
                break;
            }
        }
    }

    if (t->c_lflag & ICANON) {
#ifdef VMIN
        t->c_cc[VMIN] = 1;
#endif
#ifdef VTIME
        t->c_cc[VTIME] = 0;
#endif
    }
}

/* ── Winsize helpers ──────────────────────────────────────────────────────── */

static int get_winsize(struct winsize* ws) {
    return ioctl(g_fd, TIOCGWINSZ, ws);
}

static int set_winsize(struct winsize* ws) {
    return ioctl(g_fd, TIOCSWINSZ, ws);
}

/* ── Printing ─────────────────────────────────────────────────────────────── */

static void print_speed_line(bool with_size) {
    int ospeed = decode_baud(cfgetospeed(&g_mode));
    if (ospeed < 0) ospeed = 0;
    printf("speed %d baud;", ospeed);
    if (with_size) {
        struct winsize ws;
        if (get_winsize(&ws) == 0) {
            printf(" rows %d; columns %d;", (int)ws.ws_row, (int)ws.ws_col);
        }
    }
    printf(" line = %d;\n", (int)g_mode.c_line);
}

static void print_group(const FlagMode* modes, size_t n, bool diff,
                        const struct termios* base) {
    bool any = false;
    for (size_t i = 0; i < n; i++) {
        const FlagMode* fm = &modes[i];
        bool on = mode_on(&g_mode, fm);
        if (diff) {
            bool base_on = mode_on(base, fm);
            if (on == base_on) continue;
        }
        if (any) putchar(' ');
        printf("%s%s", on ? "" : "-", fm->name);
        any = true;
    }
    if (any) printf("\n");
}

static void print_chars(bool diff, const struct termios* base) {
    std::vector<CharMode> cms = build_charmodes();
    char buf[32];
    bool any = false;
    for (const auto& c : cms) {
        unsigned char cur = g_mode.c_cc[c.idx];
        if (diff) {
            unsigned char b = base->c_cc[c.idx];
            if (cur == b) continue;
        }
        sprint_char(buf, sizeof(buf), cur);
        if (any) printf("; ");
        printf("%s = %s", c.name, buf);
        any = true;
    }

    if (!(g_mode.c_lflag & ICANON) || !diff) {
#ifdef VMIN
        if (diff) {
            if (g_mode.c_cc[VMIN] != base->c_cc[VMIN]) {
                if (any) printf("; ");
                printf("min = %d", (int)g_mode.c_cc[VMIN]);
                any = true;
            }
        } else {
            if (any) printf("; ");
            printf("min = %d", (int)g_mode.c_cc[VMIN]);
            any = true;
        }
#endif
#ifdef VTIME
        if (diff) {
            if (g_mode.c_cc[VTIME] != base->c_cc[VTIME]) {
                if (any) printf("; ");
                printf("time = %d", (int)g_mode.c_cc[VTIME]);
                any = true;
            }
        } else {
            if (any) printf("; ");
            printf("time = %d", (int)g_mode.c_cc[VTIME]);
            any = true;
        }
#endif
    }

    if (any) printf(";\n");
}

static void print_all(void) {
    print_speed_line(true);
    print_chars(false, NULL);
    print_group(kControlModes,
                sizeof(kControlModes) / sizeof(kControlModes[0]), false, NULL);
    print_group(kInputModes,
                sizeof(kInputModes) / sizeof(kInputModes[0]), false, NULL);
    print_group(kOutputModes,
                sizeof(kOutputModes) / sizeof(kOutputModes[0]), false, NULL);
    print_group(kLocalModes,
                sizeof(kLocalModes) / sizeof(kLocalModes[0]), false, NULL);
}

static void print_default(void) {
    struct termios sane = g_mode;
    apply_sane(&sane);

    print_speed_line(false);
    print_chars(true, &sane);
    print_group(kControlModes,
                sizeof(kControlModes) / sizeof(kControlModes[0]), true, &sane);
    print_group(kInputModes,
                sizeof(kInputModes) / sizeof(kInputModes[0]), true, &sane);
    print_group(kOutputModes,
                sizeof(kOutputModes) / sizeof(kOutputModes[0]), true, &sane);
    print_group(kLocalModes,
                sizeof(kLocalModes) / sizeof(kLocalModes[0]), true, &sane);
}

static void print_g(void) {
    printf("%x:%x:%lx:%lx:%lx:%lx",
           (unsigned int)cfgetispeed(&g_mode),
           (unsigned int)cfgetospeed(&g_mode),
           (unsigned long)g_mode.c_cflag,
           (unsigned long)g_mode.c_iflag,
           (unsigned long)g_mode.c_oflag,
           (unsigned long)g_mode.c_lflag);
    for (size_t i = 0; i < NCCS; i++) {
        printf(":%x", (unsigned int)g_mode.c_cc[i]);
    }
    printf("\n");
}

static int apply_g(const char* s) {
    std::vector<std::string> toks;
    const char* p = s;
    while (*p) {
        const char* colon = strchr(p, ':');
        if (colon == NULL) {
            toks.emplace_back(p);
            break;
        }
        toks.emplace_back(p, (size_t)(colon - p));
        p = colon + 1;
    }
    if (toks.size() < 6) return -1;

    auto hx = [](const std::string& t, long def) -> long {
        if (t.empty()) return def;
        return strtol(t.c_str(), NULL, 16);
    };

    long isp = hx(toks[0], -1);
    long osp = hx(toks[1], -1);
    g_mode.c_cflag = (tcflag_t)hx(toks[2], g_mode.c_cflag);
    g_mode.c_iflag = (tcflag_t)hx(toks[3], g_mode.c_iflag);
    g_mode.c_oflag = (tcflag_t)hx(toks[4], g_mode.c_oflag);
    g_mode.c_lflag = (tcflag_t)hx(toks[5], g_mode.c_lflag);

    size_t n = toks.size() - 6;
    for (size_t i = 0; i < NCCS && i < n; i++) {
        g_mode.c_cc[i] = (cc_t)hx(toks[6 + i], g_mode.c_cc[i]);
    }

    if (isp >= 0) cfsetispeed(&g_mode, (speed_t)isp);
    if (osp >= 0) cfsetospeed(&g_mode, (speed_t)osp);
    return 0;
}

/* ── Mode application from argument tokens ────────────────────────────────── */

static int apply_tokens(char** tokens, int count) {
    std::vector<CharMode> cms = build_charmodes();
    bool saw_g = false;

    for (int i = 0; i < count; i++) {
        const char* arg = tokens[i];

        if (strchr(arg, ':') != NULL) {
            if (apply_g(arg) == 0) {
                saw_g = true;
                continue;
            }
        }
        if (saw_g) {
            return -1;
        }

        if (strcmp(arg, "sane") == 0) {
            apply_sane(&g_mode);
            continue;
        }
        if (strcmp(arg, "raw") == 0) {
            apply_raw(&g_mode);
            continue;
        }
        if (strcmp(arg, "cooked") == 0) {
            apply_sane(&g_mode);
            continue;
        }
        if (strcmp(arg, "-raw") == 0) {
            apply_sane(&g_mode);
            continue;
        }

        if (strcmp(arg, "size") == 0) {
            struct winsize ws;
            if (get_winsize(&ws) == 0) {
                printf("%d %d\n", (int)ws.ws_row, (int)ws.ws_col);
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "%s: standard input: %s\n", g_prog, strerror(errno));
            }
            continue;
        }

        const FlagMode* fm = find_mode(arg);
        if (fm != NULL) {
            apply_mode(fm, true);
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            const FlagMode* neg = find_mode(arg + 1);
            if (neg != NULL) {
                apply_mode(neg, false);
                continue;
            }
        }

        if (strcmp(arg, "rows") == 0 && i + 1 < count) {
            struct winsize ws;
            if (get_winsize(&ws) != 0) ws.ws_row = 0, ws.ws_col = 0;
            ws.ws_row = (unsigned short)atoi(tokens[++i]);
            set_winsize(&ws);
            continue;
        }
        if (strcmp(arg, "cols") == 0 && i + 1 < count) {
            struct winsize ws;
            if (get_winsize(&ws) != 0) ws.ws_row = 0, ws.ws_col = 0;
            ws.ws_col = (unsigned short)atoi(tokens[++i]);
            set_winsize(&ws);
            continue;
        }
        if (strcmp(arg, "columns") == 0 && i + 1 < count) {
            struct winsize ws;
            if (get_winsize(&ws) != 0) ws.ws_row = 0, ws.ws_col = 0;
            ws.ws_col = (unsigned short)atoi(tokens[++i]);
            set_winsize(&ws);
            continue;
        }
        if ((strcmp(arg, "speed") == 0 || strcmp(arg, "ispeed") == 0 ||
             strcmp(arg, "ospeed") == 0) &&
            i + 1 < count) {
            int n = atoi(tokens[i + 1]);
            speed_t code = encode_baud(n);
            if (code == (speed_t)-1) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "%s: invalid speed\n", g_prog);
                return -1;
            }
            if (strcmp(arg, "ispeed") == 0 || strcmp(arg, "speed") == 0)
                cfsetispeed(&g_mode, code);
            if (strcmp(arg, "ospeed") == 0 || strcmp(arg, "speed") == 0)
                cfsetospeed(&g_mode, code);
            i++;
            continue;
        }
        if (strcmp(arg, "min") == 0 && i + 1 < count) {
#ifdef VMIN
            g_mode.c_cc[VMIN] = (cc_t)atoi(tokens[++i]);
#endif
            continue;
        }
        if (strcmp(arg, "time") == 0 && i + 1 < count) {
#ifdef VTIME
            g_mode.c_cc[VTIME] = (cc_t)atoi(tokens[++i]);
#endif
            continue;
        }

        bool is_char = false;
        for (const auto& c : cms) {
            if (strcmp(arg, c.name) == 0) {
                if (i + 1 < count) {
                    int v = parse_char(tokens[i + 1]);
                    if (v < 0) {
                        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                        (void)fprintf(stderr, "%s: invalid character '%s'\n", g_prog,
                                tokens[i + 1]);
                        return -1;
                    }
                    g_mode.c_cc[c.idx] = (cc_t)v;
                    i++;
                    is_char = true;
                }
                break;
            }
        }
        if (is_char) continue;

        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "%s: invalid argument '%s'\n", g_prog, arg);
        return -1;
    }
    return 0;
}

/* ── Help / version ───────────────────────────────────────────────────────── */

static void print_help(void) {
    printf("Usage: %s [-F DEVICE | --file=DEVICE] [SETTING]...\n", g_prog);
    printf("Print or change terminal characteristics.\n");
    printf("\n");
    printf("  -a, --all           print all current settings in human-readable form\n");
    printf("  -g, --save          print all current settings in a stty-readable form\n");
    printf("  -F, --file=DEVICE   open and use the specified DEVICE instead of stdin\n");
    printf("      --help          display this help and exit\n");
    printf("      --version       output version information and exit\n");
    printf("\n");
    printf("Optional - before SETTING indicates negation.  An * marks non-POSIX\n");
    printf("settings.  Available settings are listed in the stty documentation.\n");
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

void stty_command(int argc, char** argv) {
    g_prog = argv[0];

    bool do_all = false;
    bool do_save = false;
    bool have_file = false;
    std::string file;
    std::vector<std::string> modes;
    bool stop_opts = false;

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (!stop_opts) {
            if (a == "--") { stop_opts = true; continue; }
            if (a == "--help" || a == "-h") { print_help(); return; }
            if (a == "--version") { printf("stty (modbox) 1.0\n"); return; }
            if (a == "-a" || a == "--all") { do_all = true; continue; }
            if (a == "-g" || a == "--save") { do_save = true; continue; }
            if (a == "-F") {
                if (i + 1 >= argc) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "%s: option requires an argument -- 'F'\n",
                            g_prog);
                    return;
                }
                file = argv[++i];
                have_file = true;
                continue;
            }
            if (a.starts_with("--file=")) {
                file = a.substr(sizeof("--file=") - 1);
                have_file = true;
                continue;
            }
            if (a.starts_with("-F") && a.size() > 2) {
                file = a.substr(2);
                have_file = true;
                continue;
            }
        }
        modes.push_back(a);
    }

    if (have_file) {
        g_fd = open(file.c_str(), O_RDWR);
        if (g_fd < 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "%s: %s: %s\n", g_prog, file.c_str(), strerror(errno));
            return;
        }
    } else {
        g_fd = STDIN_FILENO;
    }

    if (tcgetattr(g_fd, &g_mode) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "%s: standard input: %s\n", g_prog, strerror(errno));
        if (have_file) { close(g_fd); }
        return;
    }

    int rc = 0;
    if (!modes.empty()) {
        std::vector<char*> mtoks;
        for (auto& m : modes) { mtoks.push_back(const_cast<char*>(m.c_str())); }
        rc = apply_tokens(mtoks.data(), static_cast<int>(mtoks.size()));
        if (rc == 0) {
            if (tcsetattr(g_fd, TCSANOW, &g_mode) != 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "%s: %s\n", g_prog, strerror(errno));
                rc = 1;
            }
        }
    }

    if (rc == 0) {
        if (do_save) {
            print_g();
        } else if (do_all) {
            print_all();
        } else if (modes.empty()) {
            print_default();
        }
    }

    if (have_file) { close(g_fd); }
}

REGISTER_COMMAND("stty", stty_command, "Print or change terminal characteristics");
