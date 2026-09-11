#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>

#include "commands/expr.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// GNU coreutils expr uses arbitrary-precision-capable integer arithmetic.
// We emulate it with a 128-bit signed integer so that it does not error on
// the large values that GNU expr happily handles.
using Int = __int128_t;

struct Value {
    std::string s;   // canonical string representation
    bool is_int;     // s can be parsed as a (possibly negative) integer
    Int i;           // integer value when is_int is true
};

const char* g_prog = "expr";

std::string int128_to_string(Int n) {
    if (n == 0) { return "0";
}
    std::string s;
    unsigned __int128 mag =
        (n < 0) ? static_cast<unsigned __int128>(0) - static_cast<unsigned __int128>(n) : static_cast<unsigned __int128>(n);
    while (mag > 0) {
        s += static_cast<char>('0' + static_cast<int>(mag % 10));
        mag /= 10;
    }
    if (n < 0) { s += '-';
}
    std::reverse(s.begin(), s.end());
    return s;
}

// An integer literal is an optional leading '-' followed by one or more
// digits.  A leading '+' is NOT accepted (GNU expr treats "+5" as a string).
bool looks_like_int(const std::string& s) {
    if (s.empty()) { return false;
}
    size_t i = 0;
    if (s[0] == '-') {
        i = 1;
        if (s.size() == 1) { return false;
}
    }
    for (size_t j = i; j < s.size(); j++) {
        if (s[j] < '0' || s[j] > '9') { return false;
}
    }
    return true;
}

Value make_value(const std::string& s) {
    Value v;
    v.s = s;
    if (looks_like_int(s)) {
        bool const neg = (s[0] == '-');
        size_t const i = neg ? 1 : 0;
        unsigned __int128 mag = 0;
        for (size_t j = i; j < s.size(); j++) {
            mag = mag * 10 + static_cast<unsigned __int128>(s[j] - '0');
        }
        v.is_int = true;
        v.i = neg ? -static_cast<Int>(mag) : static_cast<Int>(mag);
    } else {
        v.is_int = false;
        v.i = 0;
    }
    return v;
}

struct Ctx {
    char** args;
    int n;
    int pos;
    std::string last;  // last consumed token (for diagnostics)
    bool error;
};

void err_non_integer(Ctx& c) {
    (void)fprintf(stderr, "%s: non-integer argument\n", g_prog);
    c.error = true;
}

void err_div_zero(Ctx& c) {
    (void)fprintf(stderr, "%s: division by zero\n", g_prog);
    c.error = true;
}

void err_bad_regex(Ctx& c) {
    (void)fprintf(stderr, "%s: Invalid regular expression\n", g_prog);
    c.error = true;
}

void err_missing(Ctx& c) {
    (void)fprintf(stderr, "%s: syntax error: missing argument after '%s'\n", g_prog, c.last.c_str());
    c.error = true;
}

void err_expect_paren(Ctx& c) {
    (void)fprintf(stderr, "%s: syntax error: expecting ')' after '%s'\n", g_prog, c.last.c_str());
    c.error = true;
}

void err_unexpected(Ctx& c, const char* tok) {
    (void)fprintf(stderr, "%s: syntax error: unexpected argument '%s'\n", g_prog, tok);
    c.error = true;
}

bool non_zero(const Value& v) {
    return !v.s.empty() && (!v.is_int || v.i != 0);
}

Value or_op(const Value& a, const Value& b) {
    return non_zero(a) ? a : b;
}

Value and_op(const Value& a, const Value& b) {
    if (non_zero(a) && non_zero(b)) { return a;
}
    return make_value("0");
}

Value cmp_op(const Value& a, const std::string& op, const Value& b) {
    bool r;
    if (a.is_int && b.is_int) {
        Int const x = a.i;
        Int const y = b.i;
        if (op == "<") { r = x < y;
        } else if (op == "<=") { r = x <= y;
        } else if (op == "=") { r = x == y;
        } else if (op == "!=") { r = x != y;
        } else if (op == ">=") { r = x >= y;
        } else { r = x > y;
}
    } else {
        int const c = strcmp(a.s.c_str(), b.s.c_str());
        if (op == "<") { r = c < 0;
        } else if (op == "<=") { r = c <= 0;
        } else if (op == "=") { r = c == 0;
        } else if (op == "!=") { r = c != 0;
        } else if (op == ">=") { r = c >= 0;
        } else { r = c > 0;
}
    }
    return make_value(r ? "1" : "0");
}

Value arith_op(Ctx& c, const Value& a, const std::string& op, const Value& b) {
    if (!a.is_int || !b.is_int) {
        err_non_integer(c);
        return make_value("0");
    }
    Int const x = a.i;
    Int const y = b.i;
    Int r = 0;
    if (op == "+") { { r = x + y;
    } } else if (op == "-") { { r = x - y;
    } } else if (op == "*") { { r = x * y;
    } } else if (op == "/") {
        if (y == 0) {
            err_div_zero(c);
            return make_value("0");
        }
        r = x / y;
    } else {  // "%"
        if (y == 0) {
            err_div_zero(c);
            return make_value("0");
        }
        r = x % y;
    }
    return make_value(int128_to_string(r));
}

Value substr_op(const Value& s, const Value& pos, const Value& len) {
    if (!pos.is_int || !len.is_int) { return make_value("");
}
    Int const p = pos.i;
    Int const l = len.i;
    if (p < 1 || l < 0) { return make_value("");
}
    const std::string& str = s.s;
    if (p > static_cast<Int>(str.size())) { return make_value("");
}
    size_t const start = static_cast<size_t>(p - 1);
    size_t const avail = str.size() - start;
    size_t const count = (l < static_cast<Int>(avail)) ? static_cast<size_t>(l) : avail;
    return make_value(str.substr(start, count));
}

Value index_op(const Value& s, const Value& chars) {
    const std::string& str = s.s;
    const std::string& ch = chars.s;
    for (size_t i = 0; i < str.size(); i++) {
        if (ch.find(str[i]) != std::string::npos) {
            return make_value(std::to_string(static_cast<long long>(i + 1)));
        }
    }
    return make_value("0");
}

Value length_op(const Value& s) {
    return make_value(std::to_string(static_cast<long long>(s.s.size())));
}

Value colon_op(Ctx& c, const Value& a, const Value& b) {
    std::string pat = b.s;
    if (pat.empty() || pat[0] != '^') { pat = "^" + pat;
}
    std::regex re;
    try {
        re.assign(pat, std::regex::basic);
    } catch (const std::regex_error&) {
        err_bad_regex(c);
        return make_value("0");
    }
    std::smatch m;
    if (!std::regex_search(a.s, m, re)) {
        return make_value("0");
    }
    if (m.size() > 1 && m[1].matched) {
        return make_value(std::string(m[1].first, m[1].second));
    }
    long long const len = static_cast<long long>(m[0].length());
    return make_value(std::to_string(len));
}

// Forward declarations of the recursive-descent parser.
static Value parse_or(Ctx& c);
static Value parse_and(Ctx& c);
static Value parse_cmp(Ctx& c);
static Value parse_add(Ctx& c);
static Value parse_mul(Ctx& c);
static Value parse_colon(Ctx& c);
static Value parse_primary(Ctx& c);

bool is_cmp(const char* t) {
    return strcmp(t, "<") == 0 || strcmp(t, "<=") == 0 || strcmp(t, "=") == 0 ||
           strcmp(t, "!=") == 0 || strcmp(t, ">=") == 0 || strcmp(t, ">") == 0;
}

Value parse_or(Ctx& c) {
    if (c.error) { return make_value("0");
}
    Value v = parse_and(c);
    while (!c.error && c.pos < c.n && strcmp(c.args[c.pos], "|") == 0) {
        c.last = c.args[c.pos];
        c.pos++;
        Value const rhs = parse_and(c);
        v = or_op(v, rhs);
    }
    return v;
}

Value parse_and(Ctx& c) {
    if (c.error) { return make_value("0");
}
    Value v = parse_cmp(c);
    while (!c.error && c.pos < c.n && strcmp(c.args[c.pos], "&") == 0) {
        c.last = c.args[c.pos];
        c.pos++;
        Value const rhs = parse_cmp(c);
        v = and_op(v, rhs);
    }
    return v;
}

Value parse_cmp(Ctx& c) {
    if (c.error) { return make_value("0");
}
    Value v = parse_add(c);
    while (!c.error && c.pos < c.n && is_cmp(c.args[c.pos])) {
        c.last = c.args[c.pos];
        std::string const op = c.args[c.pos];
        c.pos++;
        Value const rhs = parse_add(c);
        v = cmp_op(v, op, rhs);
    }
    return v;
}

Value parse_add(Ctx& c) {
    if (c.error) { return make_value("0");
}
    Value v = parse_mul(c);
    while (!c.error && c.pos < c.n &&
           (strcmp(c.args[c.pos], "+") == 0 || strcmp(c.args[c.pos], "-") == 0)) {
        c.last = c.args[c.pos];
        std::string const op = c.args[c.pos];
        c.pos++;
        Value const rhs = parse_mul(c);
        v = arith_op(c, v, op, rhs);
    }
    return v;
}

Value parse_mul(Ctx& c) {
    if (c.error) { return make_value("0");
}
    Value v = parse_colon(c);
    while (!c.error && c.pos < c.n &&
           (strcmp(c.args[c.pos], "*") == 0 || strcmp(c.args[c.pos], "/") == 0 ||
            strcmp(c.args[c.pos], "%") == 0)) {
        c.last = c.args[c.pos];
        std::string const op = c.args[c.pos];
        c.pos++;
        Value const rhs = parse_colon(c);
        v = arith_op(c, v, op, rhs);
    }
    return v;
}

Value parse_colon(Ctx& c) {
    if (c.error) { return make_value("0");
}
    Value v = parse_primary(c);
    while (!c.error && c.pos < c.n && strcmp(c.args[c.pos], ":") == 0) {
        c.last = c.args[c.pos];
        c.pos++;
        Value const rhs = parse_primary(c);
        v = colon_op(c, v, rhs);
    }
    return v;
}

Value parse_primary(Ctx& c) {
    if (c.error) { return make_value("0");
}
    if (c.pos >= c.n) {
        err_missing(c);
        return make_value("0");
    }
    std::string const tok = c.args[c.pos];
    c.last = tok;

    if (tok == "(") {
        c.pos++;
        Value v = parse_or(c);
        if (!c.error) {
            if (c.pos >= c.n) {
                err_expect_paren(c);
            } else if (strcmp(c.args[c.pos], ")") != 0) {
                err_unexpected(c, c.args[c.pos]);
            } else {
                c.pos++;
            }
        }
        return v;
    }

    if (tok == "match") {
        c.pos++;
        Value const a = parse_primary(c);
        Value const b = parse_primary(c);
        return colon_op(c, a, b);
    }
    if (tok == "substr") {
        c.pos++;
        Value const a = parse_primary(c);
        Value const b = parse_primary(c);
        Value const d = parse_primary(c);
        return substr_op(a, b, d);
    }
    if (tok == "index") {
        c.pos++;
        Value const a = parse_primary(c);
        Value const b = parse_primary(c);
        return index_op(a, b);
    }
    if (tok == "length") {
        c.pos++;
        Value const a = parse_primary(c);
        return length_op(a);
    }

    c.pos++;
    return make_value(tok);
}

void print_help(const char* prog) {
    printf("Usage: %s EXPRESSION\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("      Print the value of EXPRESSION to standard output.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
    printf("\n");
    printf("Expressions (lowest to highest precedence):\n");
    printf("  ARG1 | ARG2       ARG1 if it is neither null nor 0, else ARG2\n");
    printf("  ARG1 & ARG2       ARG1 if neither argument is null or 0, else 0\n");
    printf("  ARG1 < ARG2       ARG1 is less than ARG2\n");
    printf("  ARG1 <= ARG2      ARG1 is less than or equal to ARG2\n");
    printf("  ARG1 = ARG2       ARG1 is equal to ARG2\n");
    printf("  ARG1 != ARG2      ARG1 is unequal to ARG2\n");
    printf("  ARG1 >= ARG2      ARG1 is greater than or equal to ARG2\n");
    printf("  ARG1 > ARG2       ARG1 is greater than ARG2\n");
    printf("  ARG1 + ARG2       arithmetic sum of ARG1 and ARG2\n");
    printf("  ARG1 - ARG2       arithmetic difference of ARG1 and ARG2\n");
    printf("  ARG1 * ARG2       arithmetic product of ARG1 and ARG2\n");
    printf("  ARG1 / ARG2       arithmetic quotient of ARG1 and ARG2\n");
    printf("  ARG1 %% ARG2       arithmetic remainder of ARG1 divided by ARG2\n");
    printf("  STRING : REGEXP   anchored pattern match of REGEXP in STRING\n");
    printf("\n");
    printf("  match STRING REGEXP        same as STRING : REGEXP\n");
    printf("  substr STRING POS LENGTH   substring of STRING, POS counted from 1\n");
    printf("  index STRING CHARS         index in STRING where any CHARS is found, or 0\n");
    printf("  length STRING              length of STRING\n");
    printf("\n");
    printf("Exit status is 0 if EXPRESSION is neither null nor 0, 1 if EXPRESSION is\n");
    printf("null or 0, 2 if EXPRESSION is invalid.\n");
}

}  // namespace

int expr_command(int argc, char** argv) {
    g_prog = argv[0];

    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        print_version("expr");
        return 0;
    }

    int start = 1;
    if (argc >= 2 && strcmp(argv[1], "--") == 0) {
        start = 2;
    }

    if (start >= argc) {
        (void)fprintf(stderr, "%s: missing operand\n", g_prog);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", g_prog);
        exit(2);
    }

    Ctx c;
    c.args = argv;
    c.n = argc;
    c.pos = start;
    c.last = "";
    c.error = false;

    Value const result = parse_or(c);

    if (c.error) {
        exit(2);
    }
    if (c.pos < c.n) {
        err_unexpected(c, c.args[c.pos]);
        exit(2);
    }

    printf("%s\n", result.s.c_str());
    if (result.s.empty() || (result.is_int && result.i == 0)) {
        exit(1);
    }
    exit(0);
}

REGISTER_COMMAND("expr", expr_command, "Evaluate expression");
