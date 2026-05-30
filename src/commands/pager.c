#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <glib.h>

#include "commands/pager.h"

#define DEFAULT_TERM_HEIGHT 24

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
    // NOLINTNEXTLINE(misc-include-cleaner)
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return ws.ws_row;
    }
    return DEFAULT_TERM_HEIGHT;
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

void pager_run(GPtrArray* lines) {
    int total = (int)lines->len;
    if (total == 0) {
        return;
    }

    int kbd_fd = open_keyboard_fd();
    int need_close_kbd = (kbd_fd >= 0 && kbd_fd != STDIN_FILENO);

    if (kbd_fd < 0) {
        for (int i = 0; i < total; i++) {
            printf("%s\n", (char*)g_ptr_array_index(lines, i));
        }
        return;
    }

    if (pager_enable_raw_mode(kbd_fd) < 0) {
        if (need_close_kbd) { close(kbd_fd); }
        for (int i = 0; i < total; i++) {
            printf("%s\n", (char*)g_ptr_array_index(lines, i));
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
            printf("%s\n", (char*)g_ptr_array_index(lines, i));
        }

        int percent = (total > 1) ? (cursor * 100 / (total - 1)) : 0;
        printf("\033[7m--modbox-- line %d of %d (%d%%) -- j:down k:up q:quit\033[0m",
               cursor + 1, total, percent);
        // NOLINTNEXTLINE(bugprone-unused-return-value)
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
