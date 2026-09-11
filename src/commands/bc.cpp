#include "commands/bc.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for BigInt operators
struct BigInt;
BigInt operator+(const BigInt& a, const BigInt& b);
BigInt operator-(const BigInt& a, const BigInt& b);
BigInt operator-(const BigInt& a);
BigInt operator*(const BigInt& a, const BigInt& b);
BigInt operator/(const BigInt& a, const BigInt& b);
BigInt operator%(const BigInt& a, const BigInt& b);
BigInt powi_bigint(const BigInt& b, uint64_t e);
std::string bigint_to_string(const BigInt& n);

// BigInt: arbitrary-precision signed integer, base 10^9 (verified correct)
struct BigInt {
    static constexpr uint32_t BASE = 1000000000u;
    std::vector<uint32_t> d;
    bool neg = false;

    BigInt() = default;
    explicit BigInt(int64_t v) {
        if (v < 0) { neg = true; v = -v; }
        uint64_t n = (uint64_t)v;
        while (n > 0) { d.push_back((uint32_t)(n % BASE)); n /= BASE; }
    }
    explicit BigInt(uint64_t v) {
        while (v > 0) { d.push_back((uint32_t)(v % BASE)); v /= BASE; }
    }
    explicit BigInt(uint32_t v) : BigInt((uint64_t)v) {}
    BigInt(const char* s, int base = 10) : BigInt(std::string(s), base) {}
    BigInt(const std::string& s, int base = 10) {
        if (s.empty()) return;
        size_t start = 0;
        bool nneg = false;
        if (s[start] == '-') { nneg = true; start++; }
        else if (s[start] == '+') start++;
        std::string num = s.substr(start);
        size_t first = num.find_first_not_of('0');
        if (first == std::string::npos) { neg = false; return; }  // zero -> empty digits
        num = num.substr(first);
        BigInt result, multiplier((int64_t)1), b((int64_t)base);
        for (int i = (int)num.size() - 1; i >= 0; --i) {
            int digit = 0;
            char c = num[i];
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else continue;
            result = result + (multiplier * BigInt((int64_t)digit));
            multiplier = multiplier * b;
        }
        d = result.d;
        neg = nneg && !result.is_zero();
    }

    bool is_zero() const {
        if (d.empty()) return true;
        for (uint32_t v : d) if (v != 0) return false;
        return true;
    }
    BigInt abs() const { BigInt b = *this; b.neg = false; return b; }
    bool operator<(const BigInt& o) const {
        if (neg != o.neg) return neg;
        if (d.size() != o.d.size()) return neg ? d.size() > o.d.size() : d.size() < o.d.size();
        for (int i = (int)d.size() - 1; i >= 0; --i)
            if (d[i] != o.d[i]) return neg ? d[i] > o.d[i] : d[i] < o.d[i];
        return false;
    }
    bool operator==(const BigInt& o) const {
        if (neg != o.neg) return false;
        return d == o.d;
    }
    bool operator!=(const BigInt& o) const { return !(*this == o); }
    bool operator<=(const BigInt& o) const { return !(o < *this); }
    bool operator>=(const BigInt& o) const { return !(*this < o); }
    bool operator>(const BigInt& o) const { return o < *this; }
};

BigInt operator+(const BigInt& a, const BigInt& b) {
    if (a.neg == b.neg) {
        BigInt r; r.neg = a.neg;
        uint64_t carry = 0;
        size_t n = std::max(a.d.size(), b.d.size());
        for (size_t i = 0; i < n || carry; ++i) {
            uint64_t sum = carry;
            if (i < a.d.size()) sum += a.d[i];
            if (i < b.d.size()) sum += b.d[i];
            r.d.push_back((uint32_t)(sum % BigInt::BASE));
            carry = sum / BigInt::BASE;
        }
        return r;
    }
    if (a.neg && !b.neg) return b - a.abs();
    if (!a.neg && b.neg) return a - (-b);
    return a - b;
}

BigInt operator-(const BigInt& a, const BigInt& b) {
    if (!a.neg && !b.neg) {
        if (a < b) return -(b - a);
        BigInt r;
        int64_t borrow = 0;
        for (size_t i = 0; i < b.d.size() || borrow || i < a.d.size(); ++i) {
            int64_t av = (i < a.d.size()) ? a.d[i] : 0;
            int64_t bv = (i < b.d.size()) ? b.d[i] : 0;
            int64_t diff = av - borrow - bv;
            if (diff < 0) { diff += BigInt::BASE; borrow = 1; }
            else borrow = 0;
            r.d.push_back((uint32_t)diff);
        }
        while (!r.d.empty() && r.d.back() == 0) r.d.pop_back();
        if (r.is_zero()) r.neg = false;
        return r;
    }
    if (a.neg && b.neg) return (-b) - (-a);
    if (a.neg && !b.neg) return -(a.abs() + b);
    return a + (-b);
}

BigInt operator-(const BigInt& a) {
    BigInt r = a;
    if (!r.is_zero()) r.neg = !r.neg;
    return r;
}

BigInt operator*(const BigInt& a, const BigInt& b) {
    if (a.is_zero() || b.is_zero()) return BigInt((int64_t)0);
    BigInt r;
    r.d.assign(a.d.size() + b.d.size(), 0);
    for (size_t i = 0; i < a.d.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.d.size() || carry; ++j) {
            uint64_t cur = r.d[i + j] + carry + (uint64_t)a.d[i] * (j < b.d.size() ? b.d[j] : 0);
            r.d[i + j] = (uint32_t)(cur % BigInt::BASE);
            carry = cur / BigInt::BASE;
        }
    }
    r.neg = a.neg != b.neg;
    while (!r.d.empty() && r.d.back() == 0) r.d.pop_back();
    if (r.is_zero()) r.neg = false;
    return r;
}

BigInt operator/(const BigInt& a, const BigInt& b) {
    if (b.is_zero() || a.abs() < b.abs()) return BigInt((int64_t)0);
    BigInt quotient, remainder;
    BigInt bb = b.abs();
    for (int i = (int)a.d.size() - 1; i >= 0; --i) {
        remainder = remainder * BigInt(BigInt::BASE) + BigInt((uint64_t)a.d[i]);
        if (remainder >= bb) {
            uint64_t lo = 1, hi = BigInt::BASE - 1, digit = 0;
            while (lo <= hi) {
                uint64_t mid = lo + (hi - lo) / 2;
                if (BigInt(mid) * bb <= remainder) { digit = mid; lo = mid + 1; }
                else hi = mid - 1;
            }
            remainder = remainder - BigInt((int64_t)digit) * bb;
            quotient = quotient * BigInt(BigInt::BASE) + BigInt((int64_t)digit);
        } else {
            quotient = quotient * BigInt(BigInt::BASE);
        }
    }
    quotient.neg = a.neg != b.neg;
    if (quotient.is_zero()) quotient.neg = false;
    return quotient;
}

BigInt operator%(const BigInt& a, const BigInt& b) {
    if (b.is_zero()) return BigInt((int64_t)0);
    BigInt q = a / b;
    return a - q * b;
}

BigInt powi_bigint(const BigInt& b, uint64_t e) {
    if (e == 0) return BigInt((int64_t)1);
    BigInt result((int64_t)1), base = b;
    while (e > 0) {
        if (e & 1) result = result * base;
        base = base * base;
        e >>= 1;
    }
    return result;
}

std::string bigint_to_string(const BigInt& n) {
    if (n.is_zero()) return "0";
    std::string s;
    for (int i = (int)n.d.size() - 1; i >= 0; --i) {
        if (i == (int)n.d.size() - 1) s += std::to_string(n.d[i]);
        else {
            std::string chunk = std::to_string(n.d[i]);
            while (chunk.size() < 9) chunk = "0" + chunk;
            s += chunk;
        }
    }
    if (n.neg) s = "-" + s;
    return s;
}

// behavioral globals
static int g_scale = 0;
static int g_inbase = 10;
static int g_outbase = 10;
static bool g_warn = false;
static bool g_runtime_error = false;

// BigDecimal: value = coeff * 10^exp
struct BigDecimal {
    BigInt coeff;
    int exp = 0;
    BigDecimal() = default;
    explicit BigDecimal(const BigInt& c, int e = 0) : coeff(c), exp(e) {}
    explicit BigDecimal(int64_t v) : coeff(v), exp(0) {}
    explicit BigDecimal(uint64_t v) : coeff(v), exp(0) {}

    BigDecimal(const std::string& s, int base = 10) {
        if (s.empty()) return;
        size_t start = 0;
        bool neg = false;
        if (s[start] == '-') { neg = true; start++; }
        else if (s[start] == '+') start++;
        bool hex = (s.size() > start + 1 && s[start] == '0' &&
                    (s[start + 1] == 'x' || s[start + 1] == 'X'));
        std::string body = s.substr(start);
        if (hex) body = body.substr(2);
        int b = hex ? 16 : base;
        size_t dot = body.find('.');
        if (dot == std::string::npos) {
            coeff = BigInt(body, b);
        } else {
            std::string int_part = body.substr(0, dot);
            std::string frac_part = body.substr(dot + 1);
            if (int_part.empty()) int_part = "0";
            BigInt i_val(int_part, b);
            BigInt f_val(frac_part.empty() ? "0" : frac_part, b);
            BigInt shift((int64_t)1);
            for (size_t i = 0; i < frac_part.size(); ++i)
                shift = shift * BigInt((int64_t)10);
            coeff = i_val * shift + f_val;
            exp = -(int)frac_part.size();
        }
        if (neg && !coeff.is_zero()) coeff.neg = true;
    }

    bool is_zero() const { return coeff.is_zero(); }
    bool is_negative() const { return !coeff.is_zero() && coeff.neg; }
    BigDecimal abs() const { BigDecimal b = *this; b.coeff.neg = false; return b; }
};

BigDecimal operator-(const BigDecimal& a) { return BigDecimal(-a.coeff, a.exp); }

BigDecimal operator+(const BigDecimal& a, const BigDecimal& b) {
    if (a.exp == b.exp) return BigDecimal(a.coeff + b.coeff, a.exp);
    if (a.exp > b.exp) {
        int diff = a.exp - b.exp;
        return BigDecimal(a.coeff + b.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)diff), a.exp);
    }
    int diff = b.exp - a.exp;
    return BigDecimal(a.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)diff) + b.coeff, b.exp);
}

BigDecimal operator-(const BigDecimal& a, const BigDecimal& b) { return a + (-b); }

BigDecimal operator*(const BigDecimal& a, const BigDecimal& b) {
    return BigDecimal(a.coeff * b.coeff, a.exp + b.exp);
}

// Comparison -1/0/1 after aligning exponents (scale-aware)
static int bd_cmp(const BigDecimal& a, const BigDecimal& b) {
    if (a.exp == b.exp) {
        if (a.coeff < b.coeff) return -1;
        if (b.coeff < a.coeff) return 1;
        return 0;
    }
    BigDecimal x = a, y = b;
    if (x.exp < y.exp) {
        y.coeff = y.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)(y.exp - x.exp));
        y.exp = x.exp;
    } else {
        x.coeff = x.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)(x.exp - y.exp));
        x.exp = y.exp;
    }
    if (x.coeff < y.coeff) return -1;
    if (y.coeff < x.coeff) return 1;
    return 0;
}

static bool bd_eq(const BigDecimal& a, const BigDecimal& b) { return bd_cmp(a, b) == 0; }

// Division with explicit scale (truncates toward zero)
BigDecimal bd_div(const BigDecimal& a, const BigDecimal& b, int scale) {
    if (b.is_zero()) {
        fprintf(stderr, "bc: runtime error: divide by zero\n");
        g_runtime_error = true;
        return BigDecimal(BigInt((int64_t)0), 0);
    }
    int e = a.exp - b.exp + scale;
    BigInt num, den;
    if (e >= 0) {
        num = a.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)e);
        den = b.coeff;
    } else {
        num = a.coeff;
        den = b.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)(-e));
    }
    BigInt q = num / den;
    return BigDecimal(q, -scale);
}

BigDecimal operator/(const BigDecimal& a, const BigDecimal& b) { return bd_div(a, b, 0); }

BigDecimal bd_mod(const BigDecimal& a, const BigDecimal& b) {
    if (b.is_zero()) {
        fprintf(stderr, "bc: runtime error: divide by zero\n");
        g_runtime_error = true;
        return BigDecimal(BigInt((int64_t)0), 0);
    }
    BigDecimal x = a, y = b;
    if (x.exp < y.exp) {
        y.coeff = y.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)(y.exp - x.exp));
        y.exp = x.exp;
    } else if (y.exp < x.exp) {
        x.coeff = x.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)(x.exp - y.exp));
        x.exp = y.exp;
    }
    return BigDecimal(x.coeff % y.coeff, x.exp);
}

BigDecimal powi_bigdecimal(const BigDecimal& b, uint64_t e) {
    if (e == 0) return BigDecimal((int64_t)1);
    BigDecimal result((int64_t)1);
    BigDecimal base = b;
    while (e > 0) {
        if (e & 1) result = result * base;
        base = base * base;
        e >>= 1;
    }
    return result;
}

// integer square root of a non-negative BigInt
static BigInt isqrt_bigint(const BigInt& n) {
    if (n.is_zero() || n.neg) return BigInt((int64_t)0);
    std::string s = bigint_to_string(n);
    size_t half = (s.size() + 1) / 2;
    BigInt x = powi_bigint(BigInt((int64_t)10), (uint64_t)half);
    while (true) {
        BigInt nx = (x + n / x) / BigInt((int64_t)2);
        if (!(nx < x)) break;
        x = nx;
    }
    while (x * x > n) x = x - BigInt((int64_t)1);
    while ((x + BigInt((int64_t)1)) * (x + BigInt((int64_t)1)) <= n)
        x = x + BigInt((int64_t)1);
    return x;
}

BigDecimal sqrt_bd(const BigDecimal& n, int scale) {
    if (n.is_zero()) return BigDecimal((int64_t)0);
    if (n.is_negative()) {
        fprintf(stderr, "bc: runtime error: square root of a negative number\n");
        g_runtime_error = true;
        return BigDecimal(BigInt((int64_t)0), 0);
    }
    int e2 = n.exp + 2 * scale;
    BigInt M;
    if (e2 >= 0) M = n.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)e2);
    else M = n.coeff / powi_bigint(BigInt((int64_t)10), (uint64_t)(-e2));
    BigInt r = isqrt_bigint(M);
    return BigDecimal(r, -scale);
}

// Format a non-negative BigInt in base 2..36
static std::string bigint_to_base(const BigInt& n, int base) {
    if (n.is_zero()) return "0";
    static const char* digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    BigInt x = n;
    std::string s;
    while (!x.is_zero()) {
        uint64_t carry = 0;
        for (size_t i = 0; i < x.d.size(); ++i) {
            uint64_t cur = carry * BigInt::BASE + x.d[i];
            x.d[i] = (uint32_t)(cur / (uint64_t)base);
            carry = cur % (uint64_t)base;
        }
        while (x.d.size() > 0 && x.d.back() == 0) x.d.pop_back();
        s.push_back(digits[carry]);
    }
    std::reverse(s.begin(), s.end());
    return s;
}

std::string bigdecimal_to_string(const BigDecimal& n) {
    if (n.coeff.is_zero()) return "0";
    int base = g_outbase;
    BigInt abs_coeff = n.coeff.abs();
    int e = n.exp;
    std::string sign = n.coeff.neg ? "-" : "";
    std::string intpart, fracpart;
    if (e >= 0) {
        BigInt result = abs_coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)e);
        intpart = (base == 10) ? bigint_to_string(result) : bigint_to_base(result, base);
    } else {
        int frac_digits = -e;
        BigInt den = powi_bigint(BigInt((int64_t)10), (uint64_t)frac_digits);
        BigInt ip = abs_coeff / den;
        BigInt fr = abs_coeff - ip * den;
        intpart = (base == 10) ? bigint_to_string(ip) : bigint_to_base(ip, base);
        if (base == 10) {
            std::string fs = bigint_to_string(fr);
            while ((int)fs.size() < frac_digits) fs = "0" + fs;
            fracpart = fs;
        } else {
            static const char* digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            for (int i = 0; i < frac_digits && !fr.is_zero(); ++i) {
                fr = fr * BigInt((int64_t)base);
                BigInt d = fr / den;
                fr = fr - d * den;
                int dv = 0;
                if (!d.d.empty()) dv = (int)d.d[0];
                if (dv >= 0 && dv < 36) fracpart.push_back(digits[dv]);
            }
        }
    }
    if (fracpart.empty()) return sign + intpart;
    return sign + intpart + "." + fracpart;
}

// ---- Abstract syntax ---------------------------------------------------------
struct Env;
struct ExprNode;

struct Stmt {
    enum Type { EXPR, FUNC_DEF, IF, WHILE, FOR, RETURN, BREAK, HALT, PRINT, BLOCK };
    Type type = EXPR;
    std::unique_ptr<ExprNode> expr;
    std::unique_ptr<ExprNode> cond;
    std::unique_ptr<ExprNode> init;   // FOR
    std::unique_ptr<ExprNode> step;   // FOR
    std::vector<std::unique_ptr<Stmt>> loop_body;
    std::vector<std::unique_ptr<Stmt>> then_body;
    std::vector<std::unique_ptr<Stmt>> else_body;
    std::vector<std::unique_ptr<Stmt>> body;   // BLOCK / FUNC_DEF
    std::unique_ptr<ExprNode> ret_val;
};

struct ExprNode {
    virtual ~ExprNode() = default;
    virtual BigDecimal eval(Env& env) = 0;
};

struct NumExpr : ExprNode {
    std::string raw;
    BigDecimal eval(Env& env) override;
};

struct VarExpr : ExprNode {
    std::string name;
    bool is_array = false;
    std::unique_ptr<ExprNode> idx;
    BigDecimal eval(Env& env) override;
};

struct AssignExpr : ExprNode {
    std::string name;
    bool is_array = false;
    std::unique_ptr<ExprNode> idx;
    std::unique_ptr<ExprNode> rhs;
    BigDecimal eval(Env& env) override;
};

struct IncDecExpr : ExprNode {
    std::string name;
    int delta = 1;
    bool postfix = true;
    BigDecimal eval(Env& env) override;
};

struct BinOpExpr : ExprNode {
    char op;
    bool cmp_incl = false;
    std::unique_ptr<ExprNode> lhs, rhs;
    BigDecimal eval(Env& env) override;
};

struct UnOpExpr : ExprNode {
    char op;
    std::unique_ptr<ExprNode> inner;
    BigDecimal eval(Env& env) override;
};

struct FuncCallExpr : ExprNode {
    std::string name;
    std::vector<std::unique_ptr<ExprNode>> args;
    BigDecimal eval(Env& env) override;
};

struct Env {
    std::unordered_map<std::string, BigDecimal> vars;
    std::unordered_map<std::string, std::vector<BigDecimal>> arrays;
    struct FuncDef {
        std::vector<std::string> params;
        std::vector<std::unique_ptr<Stmt>> body;
    };
    std::unordered_map<std::string, FuncDef> funcs;
    bool should_return = false;
    bool should_break = false;
    BigDecimal return_value;
};

// ---- helpers ---------------------------------------------------------------
static int64_t bd_to_int64(const BigDecimal& v) {
    BigDecimal x = v;
    if (x.exp < 0) {
        BigInt den = powi_bigint(BigInt((int64_t)10), (uint64_t)(-x.exp));
        x.coeff = x.coeff / den;
        x.exp = 0;
    } else if (x.exp > 0) {
        x.coeff = x.coeff * powi_bigint(BigInt((int64_t)10), (uint64_t)x.exp);
        x.exp = 0;
    }
    int64_t r = 0;
    for (int i = (int)x.coeff.d.size() - 1; i >= 0; --i) {
        int64_t chunk = x.coeff.d[i];
        if (r > (INT64_MAX - chunk) / 1000000000LL) { r = INT64_MAX; break; }
        r = r * 1000000000 + chunk;
    }
    return x.coeff.neg ? -r : r;
}

static void run_statements(Env& env, const std::vector<std::unique_ptr<Stmt>>& stmts);

// ---- expression evaluation --------------------------------------------------
BigDecimal NumExpr::eval(Env&) { return BigDecimal(raw, g_inbase); }

BigDecimal VarExpr::eval(Env& env) {
    if (name == "scale") return BigDecimal((int64_t)g_scale);
    if (name == "ibase") return BigDecimal((int64_t)g_inbase);
    if (name == "obase") return BigDecimal((int64_t)g_outbase);
    if (is_array) {
        auto it = env.arrays.find(name);
        if (it != env.arrays.end() && idx) {
            int64_t i = bd_to_int64(idx->eval(env));
            if (i >= 0 && i < (int64_t)it->second.size()) return it->second[i];
        }
        return BigDecimal((int64_t)0);
    }
    auto it = env.vars.find(name);
    if (it != env.vars.end()) return it->second;
    return BigDecimal((int64_t)0);
}

BigDecimal AssignExpr::eval(Env& env) {
    BigDecimal val = rhs->eval(env);
    if (!is_array && name == "scale") {
        g_scale = (int)bd_to_int64(val);
        if (g_scale < 0) g_scale = 0;
        env.vars[name] = val;
        return val;
    }
    if (!is_array && name == "ibase") {
        int64_t b = bd_to_int64(val);
        if (b >= 2 && b <= 16) g_inbase = (int)b;
        env.vars[name] = val;
        return val;
    }
    if (!is_array && name == "obase") {
        int64_t b = bd_to_int64(val);
        if (b >= 2 && b <= 36) g_outbase = (int)b;
        env.vars[name] = val;
        return val;
    }
    if (is_array) {
        int64_t i = bd_to_int64(idx->eval(env));
        auto& arr = env.arrays[name];
        if (i < 0) i = 0;
        if (i >= (int64_t)arr.size()) arr.resize(i + 1);
        arr[i] = val;
        return val;
    }
    env.vars[name] = val;
    return val;
}

BigDecimal IncDecExpr::eval(Env& env) {
    BigDecimal cur;
    auto it = env.vars.find(name);
    if (it != env.vars.end()) cur = it->second;
    BigDecimal d((int64_t)delta);
    BigDecimal nv = cur + d;
    env.vars[name] = nv;
    return postfix ? cur : nv;
}

BigDecimal BinOpExpr::eval(Env& env) {
    BigDecimal lhs_val = lhs->eval(env);
    BigDecimal rhs_val = rhs->eval(env);
    switch (op) {
        case '+': return lhs_val + rhs_val;
        case '-': return lhs_val - rhs_val;
        case '*': return lhs_val * rhs_val;
        case '/': return bd_div(lhs_val, rhs_val, g_scale);
        case '%': return bd_mod(lhs_val, rhs_val);
        case '^': {
            if (rhs_val.is_negative()) {
                BigDecimal p = powi_bigdecimal(lhs_val, (uint64_t)(-bd_to_int64(rhs_val)));
                return bd_div(BigDecimal((int64_t)1), p, g_scale);
            }
            int64_t e = bd_to_int64(rhs_val);
            if (e < 0) e = 0;
            return powi_bigdecimal(lhs_val, (uint64_t)e);
        }
        case '>': {
            int c = bd_cmp(lhs_val, rhs_val);
            return BigDecimal((int64_t)(cmp_incl ? (c >= 0) : (c > 0)));
        }
        case '<': {
            int c = bd_cmp(lhs_val, rhs_val);
            return BigDecimal((int64_t)(cmp_incl ? (c <= 0) : (c < 0)));
        }
        case '=': return BigDecimal((int64_t)(bd_eq(lhs_val, rhs_val)));
        case '!': return BigDecimal((int64_t)(!bd_eq(lhs_val, rhs_val)));
    }
    return BigDecimal((int64_t)0);
}

BigDecimal UnOpExpr::eval(Env& env) {
    BigDecimal inner_val = inner->eval(env);
    if (op == '-') return -inner_val;
    if (op == '!') {
        bool nonzero = !bd_eq(inner_val, BigDecimal((int64_t)0));
        return BigDecimal((int64_t)(nonzero ? 0 : 1));
    }
    return inner_val;
}

BigDecimal FuncCallExpr::eval(Env& env) {
    auto it = env.funcs.find(name);
    if (it != env.funcs.end()) {
        auto& params = it->second.params;
        auto& body = it->second.body;
        if (params.size() != args.size()) {
            fprintf(stderr, "bc: runtime error: function %s expects %zu args, got %zu\n",
                    name.c_str(), params.size(), args.size());
            g_runtime_error = true;
            return BigDecimal((int64_t)0);
        }
        std::unordered_map<std::string, BigDecimal> saved_vars = env.vars;
        for (size_t i = 0; i < params.size(); ++i)
            env.vars[params[i]] = args[i]->eval(env);
        bool saved_return = env.should_return;
        bool saved_break = env.should_break;
        BigDecimal saved_value = env.return_value;
        env.should_return = false;
        env.should_break = false;
        env.return_value = BigDecimal((int64_t)0);
        run_statements(env, body);
        BigDecimal retv = env.should_return ? env.return_value : BigDecimal((int64_t)0);
        env.should_return = saved_return;
        env.should_break = saved_break;
        env.return_value = saved_value;
        env.vars = saved_vars;
        return retv;
    }
    if (name == "sqrt" && args.size() == 1)
        return sqrt_bd(args[0]->eval(env), g_scale);
    fprintf(stderr, "bc: runtime error: function not defined: %s\n", name.c_str());
    g_runtime_error = true;
    return BigDecimal((int64_t)0);
}

// ---- Tokenizer / Parser ----------------------------------------------------
class Parser {
public:
    Parser(const std::string& input, Env& env) : env_(env) { tokenize(input); }

    bool had_errors() const { return errors_; }
    std::string peek() const { return (pos_ >= tokens_.size()) ? "" : tokens_[pos_]; }
    std::string advance() { return (pos_ >= tokens_.size()) ? "" : tokens_[pos_++]; }
    bool at_end() const { return pos_ >= tokens_.size(); }

    void expect(const std::string& t) {
        if (peek() != t) {
            errors_ = true;
            fprintf(stderr, "bc: syntax error: expected '%s' but found '%s'\n",
                    t.c_str(), peek().c_str());
        }
        advance();
    }
    void error(const std::string& msg) {
        errors_ = true;
        fprintf(stderr, "bc: syntax error: %s\n", msg.c_str());
    }

    std::vector<std::unique_ptr<Stmt>> parse_program();
    void tokenize(const std::string& input);

private:
    std::vector<std::string> tokens_;
    size_t pos_ = 0;
    Env& env_;
    bool errors_ = false;

    std::vector<std::unique_ptr<Stmt>> parse_statements();
    std::unique_ptr<Stmt> parse_statement();
    std::vector<std::unique_ptr<Stmt>> parse_body();
    std::unique_ptr<ExprNode> parse_expression();
    std::unique_ptr<ExprNode> parse_assign();
    std::unique_ptr<ExprNode> parse_or();
    std::unique_ptr<ExprNode> parse_and();
    std::unique_ptr<ExprNode> parse_equality();
    std::unique_ptr<ExprNode> parse_comparison();
    std::unique_ptr<ExprNode> parse_additive();
    std::unique_ptr<ExprNode> parse_multiplicative();
    std::unique_ptr<ExprNode> parse_power();
    std::unique_ptr<ExprNode> parse_unary();
    std::unique_ptr<ExprNode> parse_primary();
    bool peek_at_is_ident_number(const std::string& tok);
};

void Parser::tokenize(const std::string& input) {
    size_t i = 0;
    while (i < input.size()) {
        if (std::isspace((unsigned char)input[i])) { ++i; continue; }
        if (input[i] == '#') { while (i < input.size() && input[i] != '\n') ++i; continue; }
        if (i + 1 < input.size() && input[i] == '/' && input[i + 1] == '/') {
            while (i < input.size() && input[i] != '\n') ++i;
            continue;
        }
        if (std::isdigit((unsigned char)input[i]) ||
            (input[i] == '.' && i + 1 < input.size() &&
             std::isdigit((unsigned char)input[i + 1]))) {
            size_t start = i;
            if (i + 1 < input.size() && input[i] == '0' &&
                (input[i + 1] == 'x' || input[i + 1] == 'X')) {
                i += 2;
                while (i < input.size() &&
                       (std::isxdigit((unsigned char)input[i]) || input[i] == '.'))
                    ++i;
            } else {
                while (i < input.size() &&
                       (std::isdigit((unsigned char)input[i]) || input[i] == '.'))
                    ++i;
                // allow trailing hex letters (e.g. 1F, FC) for ibase > 10
                while (i < input.size() && std::isxdigit((unsigned char)input[i])) ++i;
            }
            tokens_.push_back(input.substr(start, i - start));
            continue;
        }
        if (i + 1 < input.size()) {
            std::string two = input.substr(i, 2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "++" ||
                two == "--" || two == "&&" || two == "||") {
                tokens_.push_back(two);
                i += 2;
                continue;
            }
        }
        char c = input[i];
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '^' || c == '>' || c == '<' || c == '=' ||
            c == '(' || c == ')' || c == ';' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == ',' || c == '!' || c == '&' || c == '|') {
            tokens_.push_back(std::string(1, c));
            ++i;
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < input.size() &&
                   (std::isalnum((unsigned char)input[i]) || input[i] == '_'))
                ++i;
            tokens_.push_back(input.substr(start, i - start));
            continue;
        }
        ++i;
    }
}

std::vector<std::unique_ptr<Stmt>> Parser::parse_program() { return parse_statements(); }

std::vector<std::unique_ptr<Stmt>> Parser::parse_statements() {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!at_end() && peek() != "}") {
        auto stmt = parse_statement();
        if (stmt) stmts.push_back(std::move(stmt));
        if (errors_) break;
    }
    return stmts;
}

// A single statement body, or a { ... } block
std::vector<std::unique_ptr<Stmt>> Parser::parse_body() {
    if (peek() == "{") {
        advance();
        auto body = parse_statements();
        expect("}");
        return body;
    }
    std::vector<std::unique_ptr<Stmt>> v;
    auto s = parse_statement();
    if (s) v.push_back(std::move(s));
    return v;
}

std::unique_ptr<Stmt> Parser::parse_statement() {
    std::string tok = peek();

    if (tok == "if") {
        advance(); expect("(");
        auto cond = parse_expression(); expect(")");
        auto then_body = parse_body();
        std::vector<std::unique_ptr<Stmt>> else_body;
        if (peek() == "else") { advance(); else_body = parse_body(); }
        auto stmt = std::make_unique<Stmt>();
        stmt->type = Stmt::IF;
        stmt->cond = std::move(cond);
        stmt->then_body = std::move(then_body);
        stmt->else_body = std::move(else_body);
        return stmt;
    }
    if (tok == "{") {
        advance();
        auto body = parse_statements();
        expect("}");
        auto stmt = std::make_unique<Stmt>();
        stmt->type = Stmt::BLOCK;
        stmt->body = std::move(body);
        return stmt;
    }
    if (tok == "while") {
        advance(); expect("(");
        auto cond = parse_expression(); expect(")");
        auto body = parse_body();
        auto stmt = std::make_unique<Stmt>();
        stmt->type = Stmt::WHILE;
        stmt->cond = std::move(cond);
        stmt->loop_body = std::move(body);
        return stmt;
    }
    if (tok == "for") {
        advance(); expect("(");
        auto init = parse_expression(); expect(";");
        auto cond = parse_expression(); expect(";");
        auto step = parse_expression(); expect(")");
        auto body = parse_body();
        auto stmt = std::make_unique<Stmt>();
        stmt->type = Stmt::FOR;
        stmt->init = std::move(init);
        stmt->cond = std::move(cond);
        stmt->step = std::move(step);
        stmt->loop_body = std::move(body);
        return stmt;
    }
    if (tok == "return") {
        advance();
        auto expr = parse_expression();
        if (peek() == ";") advance();
        auto stmt = std::make_unique<Stmt>();
        stmt->type = Stmt::RETURN;
        stmt->ret_val = std::move(expr);
        return stmt;
    }
    if (tok == "break") { advance(); if (peek() == ";") advance(); return std::make_unique<Stmt>(Stmt::BREAK); }
    if (tok == "halt") { advance(); return std::make_unique<Stmt>(Stmt::HALT); }
    if (tok == "print") {
        advance();
        auto expr = parse_expression();
        if (peek() == ";") advance();
        auto stmt = std::make_unique<Stmt>();
        stmt->type = Stmt::PRINT;
        stmt->expr = std::move(expr);
        return stmt;
    }
    if (tok == "define") {
        advance();
        std::string name = advance();
        expect("(");
        std::vector<std::string> params;
        if (peek() != ")") {
            params.push_back(advance());
            while (peek() == ",") { advance(); params.push_back(advance()); }
        }
        expect(")");
        auto body = parse_body();
        Env::FuncDef def;
        def.params = params;
        def.body = std::move(body);
        env_.funcs[name] = std::move(def);
        return nullptr;
    }
    if (tok == ";" ) { advance(); return nullptr; }

    auto expr = parse_expression();
    if (peek() == ";") advance();
    auto stmt = std::make_unique<Stmt>();
    stmt->type = Stmt::EXPR;
    stmt->expr = std::move(expr);
    return stmt;
}

std::unique_ptr<ExprNode> Parser::parse_expression() { return parse_assign(); }

std::unique_ptr<ExprNode> Parser::parse_assign() {
    auto lhs = parse_or();
    if (peek() == "=") {
        advance();
        auto rhs = parse_assign();
        auto* var = dynamic_cast<VarExpr*>((ExprNode*)lhs.get());
        if (!var) { error("bad assignment left-hand side"); return rhs; }
        auto ae = std::make_unique<AssignExpr>();
        ae->name = var->name;
        ae->is_array = var->is_array;
        ae->idx = std::move(var->idx);
        ae->rhs = std::move(rhs);
        return ae;
    }
    return lhs;
}

std::unique_ptr<ExprNode> Parser::parse_or() {
    auto expr = parse_and();
    while (peek() == "||") {
        advance();
        auto rhs = parse_and();
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = ':';
        bin->lhs = std::move(expr); bin->rhs = std::move(rhs);
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parse_and() {
    auto expr = parse_equality();
    while (peek() == "&&") {
        advance();
        auto rhs = parse_equality();
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = '&';
        bin->lhs = std::move(expr); bin->rhs = std::move(rhs);
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parse_equality() {
    auto expr = parse_comparison();
    while (peek() == "==" || peek() == "!=") {
        std::string op = advance();
        auto rhs = parse_comparison();
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = op == "==" ? '=' : '!';
        bin->lhs = std::move(expr); bin->rhs = std::move(rhs);
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parse_comparison() {
    auto expr = parse_additive();
    while (peek() == "<" || peek() == ">" || peek() == "<=" || peek() == ">=") {
        std::string op = advance();
        auto rhs = parse_additive();
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = op[0];
        bin->cmp_incl = op.size() == 2;
        bin->lhs = std::move(expr); bin->rhs = std::move(rhs);
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parse_additive() {
    auto expr = parse_multiplicative();
    while (peek() == "+" || peek() == "-") {
        char op = advance()[0];
        auto rhs = parse_multiplicative();
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = op; bin->lhs = std::move(expr); bin->rhs = std::move(rhs);
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parse_multiplicative() {
    auto expr = parse_unary();
    while (peek() == "*" || peek() == "/" || peek() == "%") {
        char op = advance()[0];
        auto rhs = parse_unary();
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = op; bin->lhs = std::move(expr); bin->rhs = std::move(rhs);
        expr = std::move(bin);
    }
    return expr;
}

// power binds tighter than unary minus and is right-associative
std::unique_ptr<ExprNode> Parser::parse_unary() {
    if (peek() == "-" || peek() == "!") {
        char op = advance()[0];
        auto inner = parse_unary();
        auto expr = std::make_unique<UnOpExpr>();
        expr->op = op; expr->inner = std::move(inner);
        return expr;
    }
    return parse_power();
}

std::unique_ptr<ExprNode> Parser::parse_power() {
    auto base = parse_primary();
    if (peek() == "^") {
        advance();
        auto rhs = parse_unary();   // right-assoc; allows 2^-2
        auto bin = std::make_unique<BinOpExpr>();
        bin->op = '^'; bin->lhs = std::move(base); bin->rhs = std::move(rhs);
        base = std::move(bin);
    }
    return base;
}

std::unique_ptr<ExprNode> Parser::parse_primary() {
    std::string tok = peek();
    if (tok.empty()) { error("unexpected end of input"); return std::make_unique<NumExpr>(); }
    if (tok[0] == '.' || std::isdigit((unsigned char)tok[0])) {
        advance();
        auto expr = std::make_unique<NumExpr>();
        expr->raw = tok;
        return expr;
    }
    if (tok == "(") {
        advance();
        auto expr = parse_expression();
        expect(")");
        return expr;
    }
    if (std::isalpha((unsigned char)tok[0]) || tok == "_") {
        // a run of hex letters (length>1) with no following '(' is a number (ibase)
        bool allhex = tok.size() > 1;
        if (allhex)
            for (char ch : tok)
                if (!std::isxdigit((unsigned char)ch)) { allhex = false; break; }
        if (allhex && peek_at_is_ident_number(tok)) {
            advance();
            auto expr = std::make_unique<NumExpr>();
            expr->raw = tok;
            return expr;
        }
        advance();
        std::string name = tok;
        if (peek() == "[") {
            advance();
            auto idx = parse_expression();
            expect("]");
            auto expr = std::make_unique<VarExpr>();
            expr->name = name; expr->is_array = true; expr->idx = std::move(idx);
            if (peek() == "++" || peek() == "--") {
                int delta = (advance()[1] == '+') ? 1 : -1;
                auto ie = std::make_unique<IncDecExpr>();
                ie->name = name; ie->delta = delta; ie->postfix = true;
                return ie;
            }
            return expr;
        }
        if (peek() == "(") {
            advance();
            std::vector<std::unique_ptr<ExprNode>> args;
            if (peek() != ")") {
                args.push_back(parse_expression());
                while (peek() == ",") { advance(); args.push_back(parse_expression()); }
            }
            expect(")");
            auto expr = std::make_unique<FuncCallExpr>();
            expr->name = name; expr->args = std::move(args);
            return expr;
        }
        auto expr = std::make_unique<VarExpr>();
        expr->name = name;
        if (peek() == "++" || peek() == "--") {
            int delta = (advance()[1] == '+') ? 1 : -1;
            auto ie = std::make_unique<IncDecExpr>();
            ie->name = name; ie->delta = delta; ie->postfix = true;
            return ie;
        }
        return expr;
    }
    error("unexpected token: " + tok);
    return std::make_unique<NumExpr>();
}

// helper: is `tok` a hex-letter number token and the next token is not '(' ?
bool Parser::peek_at_is_ident_number(const std::string& tok) {
    // the caller already verified allhex; ensure the following token isn't '('
    size_t k = pos_ + 1;
    if (k < tokens_.size() && tokens_[k] == "(") return false;
    return true;
}

// ---- Interpreter -----------------------------------------------------------
static void run_statements(Env& env, const std::vector<std::unique_ptr<Stmt>>& stmts) {
    for (auto& stmt : stmts) {
        if (env.should_break || env.should_return) break;
        if (!stmt) continue;
        switch (stmt->type) {
            case Stmt::EXPR: {
                // GNU bc: a standalone expression statement prints its value,
                // but an assignment statement does not.
                if (dynamic_cast<AssignExpr*>((ExprNode*)stmt->expr.get()) != nullptr ||
                    dynamic_cast<IncDecExpr*>((ExprNode*)stmt->expr.get()) != nullptr) {
                    stmt->expr->eval(env);
                    break;
                }
                BigDecimal val = stmt->expr->eval(env);
                printf("%s\n", bigdecimal_to_string(val).c_str());
                break;
            }
            case Stmt::PRINT: {
                BigDecimal val = stmt->expr->eval(env);
                printf("%s\n", bigdecimal_to_string(val).c_str());
                break;
            }
            case Stmt::IF: {
                bool cond = !bd_eq(stmt->cond->eval(env), BigDecimal((int64_t)0));
                if (cond) run_statements(env, stmt->then_body);
                else run_statements(env, stmt->else_body);
                break;
            }
            case Stmt::WHILE: {
                while (!env.should_break && !env.should_return &&
                       !bd_eq(stmt->cond->eval(env), BigDecimal((int64_t)0))) {
                    run_statements(env, stmt->loop_body);
                    if (env.should_break) { env.should_break = false; break; }
                    if (env.should_return) break;
                }
                break;
            }
            case Stmt::FOR: {
                if (stmt->init) stmt->init->eval(env);
                while (!env.should_break && !env.should_return &&
                       !bd_eq(stmt->cond->eval(env), BigDecimal((int64_t)0))) {
                    run_statements(env, stmt->loop_body);
                    if (env.should_break) { env.should_break = false; break; }
                    if (env.should_return) break;
                    if (stmt->step) stmt->step->eval(env);
                }
                break;
            }
            case Stmt::RETURN:
                env.return_value =
                    stmt->ret_val ? stmt->ret_val->eval(env) : BigDecimal((int64_t)0);
                env.should_return = true;
                break;
            case Stmt::BREAK:
                env.should_break = true;
                break;
            case Stmt::HALT:
                exit(0);
            case Stmt::FUNC_DEF:
                break;
            case Stmt::BLOCK:
                run_statements(env, stmt->body);
                break;
        }
        if (env.should_break || env.should_return) break;
    }
}

// ---- entry point -----------------------------------------------------------
int bc_command(int argc, char** argv) {
    bool quiet = false;
    bool library = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-q" || a == "--quiet") quiet = true;
        else if (a == "-l" || a == "--mathlib") library = true;
        else if (a == "-s" || a == "--standard") { }
        else if (a == "-w" || a == "--warn") g_warn = true;
        else if (a == "-i" || a == "--interactive") { }
        else if (a == "--help") {
            printf("bc - arbitrary precision calculator language\n");
            printf("Usage: bc [options] [file...]\n");
            printf("\nOptions:\n");
            printf("  -q, --quiet          suppress welcome message\n");
            printf("  -l, --mathlib        set scale to 20\n");
            printf("  -s, --standard       ignore insignificant whitespace\n");
            printf("  -w, --warn           warn about POSIX incompatibilities\n");
            printf("  -i, --interactive    force interactive mode\n");
            return 0;
        }
        else if (a == "--version") { print_version("bc"); return 0; }
        else files.push_back(a);
    }

    if (library) g_scale = 20;
    if (!quiet) printf("bc 1.07.1 (modbox)\n");

    std::string input;
    for (auto& f : files) {
        FILE* fp = fopen(f.c_str(), "r");
        if (!fp) { fprintf(stderr, "bc: cannot open %s\n", f.c_str()); return 1; }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) input.append(buf, n);
        fclose(fp);
        input.push_back('\n');
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin)) input += buf;

    if (input.empty()) return 1;

    Env env;
    Parser parser(input, env);
    std::vector<std::unique_ptr<Stmt>> stmts = parser.parse_program();
    if (parser.had_errors()) return 2;

    run_statements(env, stmts);
    return g_runtime_error ? 1 : 0;
}

REGISTER_COMMAND("bc", bc_command, "Arbitrary precision calculator");