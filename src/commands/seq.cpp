#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "commands/seq.hpp"

static bool parse_number(const char* s, long double* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    char* endp = nullptr;
    errno = 0;
    long double v = strtold(s, &endp);
    if (endp == s || *endp != '\0') {
        return false;
    }
    *out = v;
    return true;
}

static int fractional_digits(const char* s) {
    const char* dot = strchr(s, '.');
    if (dot == nullptr) {
        return 0;
    }
    int n = 0;
    for (const char* p = dot + 1; *p != '\0' && isdigit((unsigned char)*p); p++) {
        n++;
    }
    return n;
}

static bool is_integer_literal(const char* s) {
    if (strpbrk(s, ".eExX") != nullptr) {
        return false;
    }
    if (strstr(s, "inf") != nullptr || strstr(s, "nan") != nullptr) {
        return false;
    }
    return true;
}

static bool looks_like_number(const char* s) {
    const char* p = s;
    if (*p == '+' || *p == '-') {
        p++;
    }
    return isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]));
}

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... LAST\n", prog);
    printf("  or:  %s [OPTION]... FIRST LAST\n", prog);
    printf("  or:  %s [OPTION]... FIRST INCREMENT LAST\n", prog);
    printf("Print numbers from FIRST to LAST, in steps of INCREMENT.\n");
    printf("\n");
    printf("  -f, --format=FORMAT      use printf style floating-point FORMAT\n");
    printf("  -s, --separator=STRING   use STRING to separate numbers (default: \\n)\n");
    printf("  -w, --equal-width        equalize width by padding with leading zeroes\n");
    printf("  -h, --help               display this help and exit\n");
    printf("\n");
    printf("If FIRST or INCREMENT is omitted, it defaults to 1.\n");
}

void seq_command(int argc, char** argv) {
    const char* prog = argv[0];
    const char* format = nullptr;
    std::string sep = "\n";
    bool equal_width = false;
    std::vector<const char*> operands;

    bool no_more_opts = false;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!no_more_opts && strcmp(a, "--") == 0) {
            no_more_opts = true;
            continue;
        }
        if (!no_more_opts && a[0] == '-' && a[1] != '\0' && !looks_like_number(a)) {
            if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
                print_help(prog);
                return;
            } else if (strcmp(a, "-w") == 0 || strcmp(a, "--equal-width") == 0) {
                equal_width = true;
            } else if (strcmp(a, "-f") == 0 || strcmp(a, "--format") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "seq: option requires an argument -- 'f'\n");
                    return;
                }
                format = argv[++i];
            } else if (strncmp(a, "--format=", 9) == 0) {
                format = a + 9;
            } else if (strncmp(a, "-f", 2) == 0) {
                format = a + 2;
            } else if (strcmp(a, "-s") == 0 || strcmp(a, "--separator") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "seq: option requires an argument -- 's'\n");
                    return;
                }
                sep = argv[++i];
            } else if (strncmp(a, "--separator=", 12) == 0) {
                sep = a + 12;
            } else if (strncmp(a, "-s", 2) == 0) {
                sep = a + 2;
            } else {
                fprintf(stderr, "seq: invalid option -- '%s'\n", a);
                fprintf(stderr, "Try '%s --help' for more information.\n", prog);
                return;
            }
        } else {
            operands.push_back(a);
        }
    }

    if (operands.empty() || operands.size() > 3) {
        fprintf(stderr, "seq: missing operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return;
    }

    const char* first_s = "1";
    const char* incr_s = "1";
    const char* last_s = nullptr;

    if (operands.size() == 1) {
        last_s = operands[0];
    } else if (operands.size() == 2) {
        first_s = operands[0];
        last_s = operands[1];
    } else {
        first_s = operands[0];
        incr_s = operands[1];
        last_s = operands[2];
    }

    long double first = 0, incr = 0, last = 0;
    if (!parse_number(first_s, &first)) {
        fprintf(stderr, "seq: invalid floating point argument: '%s'\n", first_s);
        return;
    }
    if (!parse_number(incr_s, &incr)) {
        fprintf(stderr, "seq: invalid floating point argument: '%s'\n", incr_s);
        return;
    }
    if (!parse_number(last_s, &last)) {
        fprintf(stderr, "seq: invalid floating point argument: '%s'\n", last_s);
        return;
    }

    if (incr == 0) {
        fprintf(stderr, "seq: invalid Zero increment value: '%s'\n", incr_s);
        return;
    }

    bool all_int = is_integer_literal(first_s) && is_integer_literal(incr_s) && is_integer_literal(last_s);

    int prec = fractional_digits(first_s);
    int d = fractional_digits(incr_s);
    if (d > prec) prec = d;
    d = fractional_digits(last_s);
    if (d > prec) prec = d;

    std::string fmt;
    bool user_format = (format != nullptr);
    if (user_format) {
        fmt = format;
    } else if (all_int) {
        fmt = "%.0Lf";
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%%.%dLf", prec);
        fmt = buf;
    }

    long double n_ld = floorl((last - first) / incr + 1e-9L);
    if (n_ld < 0) {
        return;
    }
    long long count = (long long)n_ld + 1;

    int width = 0;
    if (equal_width && !user_format) {
        for (long long i = 0; i < count; i++) {
            long double val = first + (long double)i * incr;
            char buf[128];
            snprintf(buf, sizeof(buf), fmt.c_str(), val);
            int len = (int)strlen(buf);
            if (len > width) width = len;
        }
    }

    for (long long i = 0; i < count; i++) {
        long double val = first + (long double)i * incr;
        if (i > 0) {
            fputs(sep.c_str(), stdout);
        }
        if (width > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), fmt.c_str(), val);
            int len = (int)strlen(buf);
            const char* p = buf;
            bool neg = (buf[0] == '-');
            if (neg) {
                fputc('-', stdout);
                p++;
                len--;
            }
            for (int k = len; k < width - (neg ? 1 : 0); k++) {
                fputc('0', stdout);
            }
            fputs(p, stdout);
        } else if (user_format) {
            printf(fmt.c_str(), (double)val);
        } else {
            printf(fmt.c_str(), val);
        }
    }
    if (count > 0) {
        fputc('\n', stdout);
    }
}
