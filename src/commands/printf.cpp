#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include "commands/printf.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s FORMAT [ARGUMENT]...\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print ARGUMENT(s) according to FORMAT, as printf in C.\n");
    printf("\n");
    printf("  -v, --version  output version information and exit\n");
    printf("  -h, --help     display this help and exit\n");
}

static const char* unescape(const char* s, char* out) {
    while (*s) {
        if (*s == '\\' && *(s + 1)) {
            s++;
            switch (*s) {
                case 'a': *out++ = '\a'; break;
                case 'b': *out++ = '\b'; break;
                case 'e': *out++ = '\x1b'; break;
                case 'f': *out++ = '\f'; break;
                case 'n': *out++ = '\n'; break;
                case 'r': *out++ = '\r'; break;
                case 't': *out++ = '\t'; break;
                case 'v': *out++ = '\v'; break;
                case '\\': *out++ = '\\'; break;
                case '0': *out++ = '\0'; break;
                case '\'': *out++ = '\''; break;
                case '\"': *out++ = '\"'; break;
                case '?': *out++ = '?'; break;
                default: *out++ = '\\'; *out++ = *s; break;
            }
            s++;
        } else {
            *out++ = *s++;
        }
    }
    *out = '\0';
    return out;
}

static void emit_format(const char* fmt, const std::vector<std::string>& args, size_t& ai) {
    const char* p = fmt;
    char ebuf[16];
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                putchar('%');
                p++;
                continue;
            }
            if (*p == '\0') {
                putchar('%');
                break;
            }
            const char* start = p;
            int flags = 0;
            while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
                flags = 1;
                p++;
            }
            while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') {
                p++;
                while (*p >= '0' && *p <= '9') p++;
            }
            char conv = *p;
            p++;
            std::string spec("%");
            if (flags) spec += std::string(start, p - start);
            else spec += conv;

            if (conv == 'c') {
                if (ai < args.size()) {
                    printf(spec.c_str(), args[ai][0]);
                } else {
                    printf(spec.c_str(), 0);
                }
                ai++;
            } else if (conv == 's') {
                const char* s = (ai < args.size()) ? args[ai].c_str() : "";
                printf(spec.c_str(), s);
                ai++;
            } else if (conv == 'd' || conv == 'i' || conv == 'o' ||
                       conv == 'u' || conv == 'x' || conv == 'X') {
                long long v = (ai < args.size()) ? atoll(args[ai].c_str()) : 0;
                printf(spec.c_str(), v);
                ai++;
            } else if (conv == 'f' || conv == 'F' || conv == 'e' ||
                       conv == 'E' || conv == 'g' || conv == 'G') {
                double v = (ai < args.size()) ? atof(args[ai].c_str()) : 0.0;
                printf(spec.c_str(), v);
                ai++;
            } else if (conv == 'b') {
                const char* s = (ai < args.size()) ? args[ai].c_str() : "";
                unescape(s, ebuf);
                fputs(ebuf, stdout);
                ai++;
            } else if (conv == 'q') {
                const char* s = (ai < args.size()) ? args[ai].c_str() : "";
                printf("%s", s);
                ai++;
            } else {
                fputc('%', stdout);
                fputc(conv, stdout);
            }
        } else {
            fputc(*p, stdout);
            p++;
        }
    }
}

void printf_command(int argc, char** argv) {
    const char* prog = argv[0];
    if (argc < 2) {
        fprintf(stderr, "printf: missing operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return;
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help(prog);
        return;
    }
    if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("printf (modbox) 1.0\n");
        return;
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; i++) {
        args.push_back(argv[i]);
    }

    size_t ai = 0;
    char fmt_buf[4096];
    unescape(argv[1], fmt_buf);
    emit_format(fmt_buf, args, ai);

    while (ai < args.size()) {
        ai = 0;
        emit_format(fmt_buf, args, ai);
    }
}
