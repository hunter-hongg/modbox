#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <argtable3.h>
#include "commands/umask.hpp"
#include "commands/command_macros.hpp"

static mode_t parse_octal(const char *str) {
    mode_t result = 0;
    const char *p = str;
    while (*p) {
        if (*p >= '0' && *p <= '7') {
            result = (result << 3) | (*p - '0');
            p++;
        } else break;
    }
    return result;
}

static bool is_octal_string(const char *str) {
    while (*str) {
        if (*str < '0' || *str > '7') return false;
        str++;
    }
    return true;
}

static void print_symbolic(mode_t mask) {
    mode_t inv = (~mask) & 0777;
    printf("u=%s%s%s,g=%s%s%s,o=%s%s%s",
           (inv & S_IRUSR) ? "r" : "",
           (inv & S_IWUSR) ? "w" : "",
           (inv & S_IXUSR) ? "x" : "",
           (inv & S_IRGRP) ? "r" : "",
           (inv & S_IWGRP) ? "w" : "",
           (inv & S_IXGRP) ? "x" : "",
           (inv & S_IROTH) ? "r" : "",
           (inv & S_IWOTH) ? "w" : "",
           (inv & S_IXOTH) ? "x" : "");
}

void umask_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* S_opt = arg_lit0("S", "symbolic", "use symbolic form");
    struct arg_lit* p_opt = arg_lit0("p", "print", "output in reusable form");
    struct arg_str* mask = arg_str0(NULL, NULL, "<MODE>", "file mode creation mask");

    struct arg_end* end = arg_end(20);

    void* argtable[] = {help_opt, version_opt, S_opt, p_opt, mask, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: umask [-p] [-S] [mode]\n");
        printf("Set or read file creation permission mask.\n");
        printf("\n");
        printf("  -p        output in a form that may be reused as input\n");
        printf("  -S        make the mode symbolic rather than octal\n");
        printf("  --help    display this help and exit\n");
        printf("  --version output version information and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (version_opt->count > 0) {
        printf("umask (modbox) 1.0\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool print_symbolic_mode = S_opt->count > 0;
    bool print_reusable = p_opt->count > 0;
    bool setting_mask = mask->count > 0 && strlen(mask->sval[0]) > 0;

    if (!setting_mask) {
        mode_t current = umask(0);
        umask(current);

        if (print_reusable) {
            printf("umask %04o\n", current & 0777);
        } else if (print_symbolic_mode) {
            printf("umask ");
            print_symbolic(current);
            printf("\n");
        } else {
            printf("%04o\n", current & 0777);
        }
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    const char *mask_str = mask->sval[0];
    mode_t new_mask;

    if (is_octal_string(mask_str)) {
        new_mask = parse_octal(mask_str);
    } else {
        fprintf(stderr, "umask: invalid mask: %s\n", mask_str);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (print_reusable) {
        printf("umask %04o\n", new_mask & 0777);
    } else if (print_symbolic_mode) {
        printf("umask ");
        print_symbolic(new_mask);
        printf("\n");
    }

    umask(new_mask & 0777);

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("umask", umask_command, "Set or read file creation permission mask");