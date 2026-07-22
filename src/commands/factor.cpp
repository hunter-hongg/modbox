#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "commands/factor.hpp"
#include "commands/command_macros.hpp"

using u64 = uint64_t;
using u128 = __uint128_t;

static u64 mulmod(u64 a, u64 b, u64 m) {
    return (u64)((u128)a * b % m);
}

static u64 powmod(u64 a, u64 e, u64 m) {
    u64 r = 1 % m;
    a %= m;
    while (e > 0) {
        if ((e & 1U) != 0U) r = mulmod(r, a, m);
        a = mulmod(a, a, m);
        e >>= 1;
    }
    return r;
}

static u64 gcd_u64(u64 a, u64 b) {
    while (b != 0) {
        u64 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static bool is_prime(u64 n) {
    if (n < 2) return false;
    static const u64 witnesses[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    for (u64 p : witnesses) {
        if (n == p) return true;
        if (n % p == 0) return false;
    }
    u64 d = n - 1;
    int s = 0;
    while ((d & 1U) == 0U) {
        d >>= 1;
        s++;
    }
    for (u64 a : witnesses) {
        u64 x = powmod(a, d, n);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (int r = 0; r < s - 1; r++) {
            x = mulmod(x, x, n);
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

static u64 pollard_rho(u64 n) {
    if ((n & 1U) == 0U) return 2;
    for (u64 c = 1;; c++) {
        u64 x = 2, y = 2, d = 1;
        while (d == 1) {
            x = (mulmod(x, x, n) + c) % n;
            y = (mulmod(y, y, n) + c) % n;
            y = (mulmod(y, y, n) + c) % n;
            u64 diff = x > y ? x - y : y - x;
            d = gcd_u64(diff, n);
        }
        if (d != n) return d;
    }
}

static void factorize(u64 n, std::vector<u64>& out) {
    if (n < 2) return;
    for (u64 p = 2; p < 1000 && p * p <= n; p++) {
        while (n % p == 0) {
            out.push_back(p);
            n /= p;
        }
    }
    if (n < 2) return;
    if (is_prime(n)) {
        out.push_back(n);
        return;
    }
    u64 d = pollard_rho(n);
    factorize(d, out);
    factorize(n / d, out);
}

static bool parse_u64(const char* s, u64* out) {
    if (s == nullptr || *s == '\0') return false;
    const char* p = s;
    if (*p == '+') p++;
    if (*p == '\0') return false;
    for (const char* q = p; *q != '\0'; q++) {
        if (isdigit((unsigned char)*q) == 0) return false;
    }
    errno = 0;
    char* endp = nullptr;
    unsigned long long v = strtoull(p, &endp, 10);
    if (errno == ERANGE || endp == p || *endp != '\0') return false;
    *out = (u64)v;
    return true;
}

static void print_factors(const char* s, bool exponents) {
    u64 n = 0;
    if (!parse_u64(s, &n)) {
        fprintf(stderr, "factor: '%s' is not a valid positive integer\n", s);
        return;
    }
    printf("%llu:", (unsigned long long)n);
    if (n < 2) {
        printf("\n");
        return;
    }
    std::vector<u64> f;
    factorize(n, f);
    std::sort(f.begin(), f.end());
    if (exponents) {
        size_t i = 0;
        while (i < f.size()) {
            size_t j = i;
            while (j < f.size() && f[j] == f[i]) j++;
            size_t e = j - i;
            if (e == 1) {
                printf(" %llu", (unsigned long long)f[i]);
            } else {
                printf(" %llu^%zu", (unsigned long long)f[i], e);
            }
            i = j;
        }
    } else {
        for (u64 p : f) {
            printf(" %llu", (unsigned long long)p);
        }
    }
    printf("\n");
}

static void print_help(const char* prog) {
    printf("Usage: %s [NUMBER]...\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print the prime factors of each specified integer NUMBER.  If none\n");
    printf("are specified on the command line, read them from standard input.\n");
    printf("\n");
    printf("  -h, --exponents   print repeated factors in form p^e unless e==1\n");
    printf("      --help        display this help and exit\n");
    printf("      --version     output version information and exit\n");
}

void factor_command(int argc, char** argv) {
    const char* prog = argv[0];
    bool exponents = false;
    std::vector<const char*> operands;

    bool no_more_opts = false;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!no_more_opts && strcmp(a, "--") == 0) {
            no_more_opts = true;
            continue;
        }
        if (!no_more_opts && a[0] == '-' && a[1] != '\0' && isdigit((unsigned char)a[1]) == 0) {
            if (strcmp(a, "--help") == 0) {
                print_help(prog);
                return;
            }
            if (strcmp(a, "--version") == 0) {
                printf("factor (modbox) 1.0\n");
                return;
            }
            if (strcmp(a, "-h") == 0 || strcmp(a, "--exponents") == 0) {
                exponents = true;
                continue;
            }
            fprintf(stderr, "factor: invalid option: '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", prog);
            return;
        }
        operands.push_back(a);
    }

    if (!operands.empty()) {
        for (const char* s : operands) {
            print_factors(s, exponents);
        }
        return;
    }

    std::string tok;
    int c = 0;
    while ((c = getchar()) != EOF) {
        if (isspace(c) != 0) {
            if (!tok.empty()) {
                print_factors(tok.c_str(), exponents);
                tok.clear();
            }
        } else {
            tok.push_back((char)c);
        }
    }
    if (!tok.empty()) {
        print_factors(tok.c_str(), exponents);
    }
}

REGISTER_COMMAND("factor", factor_command, "Print prime factors");
