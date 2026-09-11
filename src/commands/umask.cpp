#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <argtable3.h>
#include <sys/types.h>
#include "commands/umask.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static mode_t parse_octal(const char *str) {
    mode_t result = 0;
    const char *p = str;
    while ((*p) != 0) {
        if (*p >= '0' && *p <= '7') {
            result = (result << 3) | (*p - '0');
            p++;
        } else { { break;
}
}
    }
    return result;
}

static bool is_octal_string(const char *str) {
    while ((*str) != 0) {
        if (*str < '0' || *str > '7') { return false;
}
        str++;
    }
    return true;
}

static void print_symbolic(mode_t mask) {
    mode_t const inv = (~mask) & 0777;
    printf("u=%s%s%s,g=%s%s%s,o=%s%s%s",
           ((inv & S_IRUSR) != 0u) ? "r" : "",
           ((inv & S_IWUSR) != 0u) ? "w" : "",
           ((inv & S_IXUSR) != 0u) ? "x" : "",
           ((inv & S_IRGRP) != 0u) ? "r" : "",
           ((inv & S_IWGRP) != 0u) ? "w" : "",
           ((inv & S_IXGRP) != 0u) ? "x" : "",
           ((inv & S_IROTH) != 0u) ? "r" : "",
           ((inv & S_IWOTH) != 0u) ? "w" : "",
           ((inv & S_IXOTH) != 0u) ? "x" : "");
}

int umask_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* S_opt = arg_lit0("S", "symbolic", "use symbolic form");
    struct arg_lit* p_opt = arg_lit0("p", "print", "output in reusable form");
    struct arg_str* mask = arg_str0(NULL, NULL, "<MODE>", "file mode creation mask");

    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, S_opt, p_opt, mask, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: umask [-p] [-S] [mode]\n");
        printf("Set or read file creation permission mask.\n");
        printf("\n");
        printf("  -p        output in a form that may be reused as input\n");
        printf("  -S        make the mode symbolic rather than octal\n");
        printf("  --help    display this help and exit\n");
        printf("  --version output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("umask");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    bool const print_symbolic_mode = S_opt->count > 0;
    bool const print_reusable = p_opt->count > 0;
    bool const setting_mask = mask->count > 0 && strlen(mask->sval[0]) > 0;

    if (!setting_mask) {
        mode_t const current = umask(0);
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
        return 0;
    }

    const char *mask_str = mask->sval[0];
    mode_t new_mask;

    if (is_octal_string(mask_str)) {
        new_mask = parse_octal(mask_str);
    } else {
        (void)fprintf(stderr, "umask: invalid mask: %s\n", mask_str);
        return 0;
    }

    if (print_reusable) {
        printf("umask %04o\n", new_mask & 0777);
    } else if (print_symbolic_mode) {
        printf("umask ");
        print_symbolic(new_mask);
        printf("\n");
    }

    umask(new_mask & 0777);

    return 0;
}

REGISTER_COMMAND("umask", umask_command, "Set or read file creation permission mask");