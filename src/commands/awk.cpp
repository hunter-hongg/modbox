#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "commands/awk.hpp"
#include "commands/command_macros.hpp"

// ---------------------------------------------------------------------------
// Value
// ---------------------------------------------------------------------------

struct Value {
    bool numeric = true;
    bool regex = false;
    double num = 0;
    std::string str;

    Value() = default;
    Value(double n) : numeric(true), num(n) {}
    Value(const std::string& s) : numeric(false), str(s) {}
    Value(const char* s) : numeric(false), str(s) {}
};

static const std::string& awk_convfmt();
static const std::string& awk_ofmt();

static bool is_numstr(const std::string& s) {
    static const std::regex re("^[+-]?([0-9]+(\\.[0-9]*)?|\\.[0-9]+)([eE][+-]?[0-9]+)?$");
    return std::regex_match(s, re);
}

static double val_to_num(const Value& v) {
    if (v.numeric) return v.num;
    if (v.str.empty()) return 0;
    return std::strtod(v.str.c_str(), nullptr);
}

static std::string val_to_str(const Value& v, const std::string& fmt) {
    if (!v.numeric) return v.str;
    char buf[512];
    std::snprintf(buf, sizeof(buf), fmt.c_str(), v.num);
    return std::string(buf);
}

static bool val_is_numeric(const Value& v) {
    if (v.numeric) return true;
    return is_numstr(v.str);
}

static bool val_truthy(const Value& v, const std::string& record) {
    if (v.regex) {
        try {
            if (v.str.empty()) return true;
            std::regex re(v.str);
            return std::regex_search(record, re);
        } catch (...) {
            return false;
        }
    }
    if (v.numeric) return v.num != 0;
    return !v.str.empty();
}

static int compare_values(const Value& a, const Value& b) {
    bool an = val_is_numeric(a), bn = val_is_numeric(b);
    if (an && bn) {
        double x = val_to_num(a), y = val_to_num(b);
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    }
    std::string xs = val_to_str(a, awk_convfmt());
    std::string ys = val_to_str(b, awk_convfmt());
    if (xs < ys) return -1;
    if (xs > ys) return 1;
    return 0;
}

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------

enum TokType {
    T_EOF,
    T_NUM,
    T_STR,
    T_REGEX,
    T_IDENT,
    T_IN,
    T_EQ, T_NE, T_LE, T_GE, T_MATCH, T_NMATCH,
    T_AND, T_OR,
    T_ADD_A, T_SUB_A, T_MUL_A, T_DIV_A, T_MOD_A, T_POW_A,
    T_INCR, T_DECR,
    T_PLUS = '+', T_MINUS = '-', T_MUL = '*', T_DIV = '/',
    T_MOD = '%', T_POW = '^', T_ASSIGN = '=', T_LT = '<', T_GT = '>',
    T_NOT = '!', T_LP = '(', T_RP = ')', T_LB = '{', T_RB = '}',
    T_LS = '[', T_RS = ']', T_COMMA = ',', T_SEMI = ';',
    T_DOLLAR = '$', T_COLON = ':', T_QUE = '?', T_TILDE = '~'
};

struct Token {
    int type;
    std::string text;
    double num = 0;
};

static bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_';
}
static bool is_ident_char(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

class Lexer {
public:
    explicit Lexer(const std::string& src) : s_(src) { scan(); }

    const Token& peek() { return toks_[pos_]; }
    const Token& peek2() { return toks_[pos_ + 1]; }
    Token next() { return toks_[pos_++]; }
    bool at_end() { return toks_[pos_].type == T_EOF; }

private:
    std::string s_;
    size_t i_ = 0;
    std::vector<Token> toks_;
    size_t pos_ = 0;

    bool operand_follows() {
        if (toks_.empty()) return true;
        int t = toks_.back().type;
        switch (t) {
            case T_NUM: case T_STR: case T_REGEX: case T_IDENT:
            case T_RP: case T_RS: case T_INCR: case T_DECR:
                return false;
            default:
                return true;
        }
    }

    void skip_ws() {
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
                i_++;
            } else if (c == '#') {
                while (i_ < s_.size() && s_[i_] != '\n') i_++;
            } else {
                break;
            }
        }
    }

    void scan() {
        while (true) {
            skip_ws();
            if (i_ >= s_.size()) { emit(T_EOF, ""); break; }

            char c = s_[i_];

            if (std::isdigit((unsigned char)c) ||
                (c == '.' && i_ + 1 < s_.size() && std::isdigit((unsigned char)s_[i_ + 1]))) {
                scan_number();
                continue;
            }
            if (c == '"') { scan_string(); continue; }
            if (c == '\'') { scan_string(); continue; }

            if (c == '/' && operand_follows()) { scan_regex(); continue; }

            if (is_ident_start(c)) { scan_ident(); continue; }

            scan_punct();
        }
    }

    void emit(int t, const std::string& txt, double n = 0) {
        Token tk;
        tk.type = t;
        tk.text = txt;
        tk.num = n;
        toks_.push_back(tk);
    }

    void scan_number() {
        size_t start = i_;
        while (i_ < s_.size() && (std::isdigit((unsigned char)s_[i_]) || s_[i_] == '.' ||
               s_[i_] == 'e' || s_[i_] == 'E' || s_[i_] == '+' || s_[i_] == '-')) {
            if ((s_[i_] == '+' || s_[i_] == '-') &&
                !(i_ > 0 && (s_[i_-1] == 'e' || s_[i_-1] == 'E'))) break;
            i_++;
        }
        std::string txt = s_.substr(start, i_ - start);
        emit(T_NUM, txt, std::strtod(txt.c_str(), nullptr));
    }

    void scan_string() {
        char quote = s_[i_++];
        std::string out;
        while (i_ < s_.size() && s_[i_] != quote) {
            char c = s_[i_++];
            if (c == '\\' && i_ < s_.size()) {
                char e = s_[i_++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'a': out += '\a'; break;
                    case 'v': out += '\v'; break;
                    case '0': out += '\0'; break;
                    default: out += e; break;
                }
            } else {
                out += c;
            }
        }
        if (i_ < s_.size()) i_++;
        emit(T_STR, out);
    }

    void scan_regex() {
        i_++; // consume /
        std::string out;
        while (i_ < s_.size() && s_[i_] != '/') {
            char c = s_[i_++];
            if (c == '\\' && i_ < s_.size()) {
                out += c;
                out += s_[i_++];
            } else {
                out += c;
            }
        }
        if (i_ < s_.size()) i_++;
        emit(T_REGEX, out);
    }

    void scan_ident() {
        size_t start = i_;
        while (i_ < s_.size() && is_ident_char(s_[i_])) i_++;
        std::string txt = s_.substr(start, i_ - start);
        if (txt == "in") emit(T_IN, txt);
        else emit(T_IDENT, txt);
    }

    void scan_punct() {
        char c = s_[i_];
        char n = (i_ + 1 < s_.size()) ? s_[i_ + 1] : 0;
        auto two = [&](int t, const std::string& str) {
            i_ += 2;
            emit(t, str);
        };
        auto one = [&](int t) { i_++; emit(t, std::string(1, c)); };

        switch (c) {
            case '=': if (n == '=') two(T_EQ, "=="); else one(T_ASSIGN); break;
            case '!': if (n == '=') two(T_NE, "!="); else one(T_NOT); break;
            case '<': if (n == '=') two(T_LE, "<="); else one(T_LT); break;
            case '>': if (n == '=') two(T_GE, ">="); else one(T_GT); break;
            case '~': one(T_TILDE); break;
            case '&': if (n == '&') two(T_AND, "&&"); else one(T_TILDE == 0 ? T_AND : T_TILDE); break;
            case '|': if (n == '|') two(T_OR, "||"); else one(T_OR); break;
            case '+': if (n == '=') two(T_ADD_A, "+="); else if (n == '+') two(T_INCR, "++"); else one(T_PLUS); break;
            case '-': if (n == '=') two(T_SUB_A, "-="); else if (n == '-') two(T_DECR, "--"); else one(T_MINUS); break;
            case '*': if (n == '=') two(T_MUL_A, "*="); else one(T_MUL); break;
            case '/': one(T_DIV); break;
            case '%': if (n == '=') two(T_MOD_A, "%="); else one(T_MOD); break;
            case '^': if (n == '=') two(T_POW_A, "^="); else one(T_POW); break;
            case '(': one(T_LP); break;
            case ')': one(T_RP); break;
            case '{': one(T_LB); break;
            case '}': one(T_RB); break;
            case '[': one(T_LS); break;
            case ']': one(T_RS); break;
            case ',': one(T_COMMA); break;
            case ';': one(T_SEMI); break;
            case '$': one(T_DOLLAR); break;
            case ':': one(T_COLON); break;
            case '?': one(T_QUE); break;
            default:
                i_++;
                emit(T_EOF, std::string(1, c));
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------

enum NodeKind {
    E_NUM, E_STR, E_REGEX,
    E_FIELD, E_VAR, E_ASSIGN, E_BINARY, E_UNARY, E_COND, E_CALL, E_IN,
    S_BLOCK, S_EXPR, S_PRINT, S_PRINTF, S_IF, S_WHILE, S_FOR, S_FOR_IN,
    S_NEXT, S_EXIT, S_BREAK, S_CONTINUE, S_DELETE, S_GETLINE, S_RETURN
};

enum Op {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_MATCH, OP_NMATCH, OP_AND, OP_OR, OP_NOT, OP_CONCAT,
    OP_ASSIGN, OP_ADD_A, OP_SUB_A, OP_MUL_A, OP_DIV_A, OP_MOD_A, OP_POW_A,
    OP_PREINC, OP_PREDEC, OP_POSTINC, OP_POSTDEC, OP_NEG
};

struct Node {
    int kind;
    int op = 0;
    double num = 0;
    std::string str;
    std::string str2;
    int redir = 0;            // 0 none, 1 >, 2 >>
    Node* a = nullptr;
    Node* b = nullptr;
    Node* c = nullptr;
    Node* d = nullptr;        // redirect target
    std::vector<Node*> kids;  // statements / call args / indices
    std::vector<std::string> argnames;

    Node(int k) : kind(k) {}
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

class Parser {
public:
    explicit Parser(const std::string& src) : lex_(src) {}

    std::vector<Node*> begin_blocks;
    std::vector<Node*> end_blocks;
    std::vector<Node*> rules;        // action blocks (EXPR pattern implied)
    std::vector<Node*> rule_patterns; // E_REGEX/E_BINARY or nullptr

    void parse() {
        while (!lex_.at_end()) {
            const Token& t = lex_.peek();
            if (t.type == T_SEMI) { lex_.next(); continue; }
            if (t.type == T_IDENT && t.text == "function") {
                parse_function();
                continue;
            }
            if (t.type == T_IDENT && t.text == "BEGIN") {
                lex_.next();
                begin_blocks.push_back(parse_block());
                continue;
            }
            if (t.type == T_IDENT && t.text == "END") {
                lex_.next();
                end_blocks.push_back(parse_block());
                continue;
            }
            if (t.type == T_LB) {
                Node* act = parse_block();
                rules.push_back(act);
                rule_patterns.push_back(nullptr);
                continue;
            }
            Node* pat = parse_pattern();
            Node* act = nullptr;
            if (lex_.peek().type == T_LB) act = parse_block();
        rules.push_back(act);
        rule_patterns.push_back(pat);
    }
    }

    std::unordered_map<std::string, Node*>& functions() { return funcs_; }

private:
    Lexer lex_;
    std::unordered_map<std::string, Node*> funcs_;
    std::vector<std::unique_ptr<Node>> pool_;
    bool in_print_ = false;

    Node* mk(int k) {
        Node* n = new Node(k);
        pool_.push_back(std::unique_ptr<Node>(n));
        return n;
    }

    bool is_primary_start() {
        int t = lex_.peek().type;
        return t == T_NUM || t == T_STR || t == T_REGEX || t == T_IDENT ||
               t == T_LP || t == T_DOLLAR;
    }

    Node* parse_pattern() {
        if (lex_.at_end() || lex_.peek().type == T_LB) return nullptr;
        Node* e = parse_expr();
        if (lex_.peek().type == T_COMMA) {
            lex_.next();
            Node* e2 = parse_expr();
            Node* rng = mk(E_CALL);
            rng->str = "@range";
            rng->a = e;
            rng->b = e2;
            return rng;
        }
        return e;
    }

    Node* parse_block() {
        expect(T_LB);
        Node* blk = mk(S_BLOCK);
        while (lex_.peek().type != T_RB && !lex_.at_end()) {
            if (lex_.peek().type == T_SEMI) { lex_.next(); continue; }
            blk->kids.push_back(parse_statement());
            if (lex_.peek().type == T_SEMI) lex_.next();
        }
        expect(T_RB);
        return blk;
    }

    Node* parse_statement() {
        const Token& t = lex_.peek();
        if (t.type == T_LB) return parse_block();
        if (t.type == T_IDENT) {
            if (t.text == "print") return parse_print(false);
            if (t.text == "printf") return parse_print(true);
            if (t.text == "if") {
                lex_.next();
                expect(T_LP);
                Node* cond = parse_expr();
                expect(T_RP);
                Node* thenb = parse_statement();
                Node* elseb = nullptr;
                if (lex_.peek().type == T_IDENT && lex_.peek().text == "else") {
                    lex_.next();
                    elseb = parse_statement();
                }
                Node* n = mk(S_IF);
                n->a = cond; n->b = thenb; n->c = elseb;
                return n;
            }
            if (t.text == "while") {
                lex_.next();
                expect(T_LP);
                Node* cond = parse_expr();
                expect(T_RP);
                Node* body = parse_statement();
                Node* n = mk(S_WHILE);
                n->a = cond; n->b = body;
                return n;
            }
            if (t.text == "for") return parse_for();
            if (t.text == "break") { lex_.next(); return mk(S_BREAK); }
            if (t.text == "continue") { lex_.next(); return mk(S_CONTINUE); }
            if (t.text == "next") { lex_.next(); return mk(S_NEXT); }
            if (t.text == "exit") {
                lex_.next();
                Node* n = mk(S_EXIT);
                if (lex_.peek().type != T_SEMI && lex_.peek().type != T_RB &&
                    lex_.peek().type != T_EOF && !is_primary_start_end()) {
                    n->a = parse_expr();
                }
                return n;
            }
            if (t.text == "delete") return parse_delete();
            if (t.text == "return") {
                lex_.next();
                Node* n = mk(S_RETURN);
                if (lex_.peek().type != T_SEMI && lex_.peek().type != T_RB &&
                    lex_.peek().type != T_EOF && !is_primary_start_end()) {
                    n->a = parse_expr();
                }
                return n;
            }
            if (t.text == "getline") return parse_getline();
        }
        Node* n = mk(S_EXPR);
        n->a = parse_expr();
        return n;
    }

    bool is_primary_start_end() {
        return lex_.at_end() || lex_.peek().type == T_LB;
    }

    Node* parse_print(bool is_printf) {
        lex_.next();
        Node* n = mk(is_printf ? S_PRINTF : S_PRINT);
        if (lex_.peek().type == T_GT || lex_.peek().type == T_RB ||
            lex_.peek().type == T_SEMI || lex_.peek().type == T_EOF) {
            // no args
        } else {
            in_print_ = true;
            n->kids.push_back(parse_expr());
            while (lex_.peek().type == T_COMMA) {
                lex_.next();
                n->kids.push_back(parse_expr());
            }
            in_print_ = false;
        }
        if (lex_.peek().type == T_GT) {
            lex_.next();
            if (lex_.peek().type == T_GT) { n->redir = 2; lex_.next(); }
            else n->redir = 1;
            n->d = parse_expr();
        }
        return n;
    }

    Node* parse_for() {
        lex_.next();
        expect(T_LP);
        if (lex_.peek().type == T_IDENT && lex_.peek2().type == T_IN) {
            std::string var = lex_.next().text;
            lex_.next(); // in
            std::string arr = lex_.next().text;
            expect(T_RP);
            Node* body = parse_statement();
            Node* n = mk(S_FOR_IN);
            n->str = var;
            n->str2 = arr;
            n->a = body;
            return n;
        }
        Node *init = nullptr, *cond = nullptr, *incr = nullptr;
        if (lex_.peek().type != T_SEMI) init = parse_expr();
        expect(T_SEMI);
        if (lex_.peek().type != T_SEMI) cond = parse_expr();
        expect(T_SEMI);
        if (lex_.peek().type != T_RP) incr = parse_expr();
        expect(T_RP);
        Node* body = parse_statement();
        Node* n = mk(S_FOR);
        n->a = init; n->b = cond; n->c = incr; n->kids.push_back(body);
        return n;
    }

    Node* parse_delete() {
        lex_.next();
        std::string name = expect(T_IDENT).text;
        Node* n = mk(S_DELETE);
        n->str = name;
        if (lex_.peek().type == T_LS) {
            lex_.next();
            n->kids.push_back(parse_expr());
            while (lex_.peek().type == T_COMMA) { lex_.next(); n->kids.push_back(parse_expr()); }
            expect(T_RS);
        }
        return n;
    }

    Node* parse_getline() {
        lex_.next();
        Node* n = mk(S_GETLINE);
        if (lex_.peek().type == T_IDENT) {
            n->str = lex_.next().text;
        }
        if (lex_.peek().type == T_LT) {
            lex_.next();
            n->d = parse_expr();
        }
        return n;
    }

    Node* parse_expr() { return parse_assign(); }

    Node* parse_assign() {
        Node* left = parse_cond();
        int t = lex_.peek().type;
        if (t == T_ASSIGN || t == T_ADD_A || t == T_SUB_A || t == T_MUL_A ||
            t == T_DIV_A || t == T_MOD_A || t == T_POW_A) {
            lex_.next();
            Node* right = parse_assign();
            Node* n = mk(E_ASSIGN);
            n->a = left; n->b = right;
            switch (t) {
                case T_ASSIGN: n->op = OP_ASSIGN; break;
                case T_ADD_A: n->op = OP_ADD_A; break;
                case T_SUB_A: n->op = OP_SUB_A; break;
                case T_MUL_A: n->op = OP_MUL_A; break;
                case T_DIV_A: n->op = OP_DIV_A; break;
                case T_MOD_A: n->op = OP_MOD_A; break;
                case T_POW_A: n->op = OP_POW_A; break;
            }
            return n;
        }
        return left;
    }

    Node* parse_cond() {
        Node* cond = parse_or();
        if (lex_.peek().type == T_QUE) {
            lex_.next();
            Node* thenb = parse_or();
            expect(T_COLON);
            Node* elseb = parse_or();
            Node* n = mk(E_COND);
            n->a = cond; n->b = thenb; n->c = elseb;
            return n;
        }
        return cond;
    }

    Node* parse_or() {
        Node* left = parse_and();
        while (lex_.peek().type == T_OR) {
            lex_.next();
            Node* right = parse_and();
            Node* n = mk(E_BINARY);
            n->op = OP_OR; n->a = left; n->b = right;
            left = n;
        }
        return left;
    }

    Node* parse_and() {
        Node* left = parse_match();
        while (lex_.peek().type == T_AND) {
            lex_.next();
            Node* right = parse_match();
            Node* n = mk(E_BINARY);
            n->op = OP_AND; n->a = left; n->b = right;
            left = n;
        }
        return left;
    }

    Node* parse_match() {
        Node* left = parse_compare();
        int t = lex_.peek().type;
        if (t == T_TILDE) {
            lex_.next();
            Node* right = parse_compare();
            Node* n = mk(E_BINARY);
            n->op = OP_MATCH;
            n->a = left; n->b = right;
            return n;
        }
        if (t == T_NOT && lex_.peek2().type == T_TILDE) {
            lex_.next();
            lex_.next();
            Node* right = parse_compare();
            Node* n = mk(E_BINARY);
            n->op = OP_NMATCH;
            n->a = left; n->b = right;
            return n;
        }
        if (t == T_IN) {
            lex_.next();
            std::string arr = expect(T_IDENT).text;
            Node* n = mk(E_IN);
            n->a = left; n->str = arr;
            return n;
        }
        return left;
    }

    Node* parse_compare() {
        Node* left = parse_concat();
        if (in_print_ && lex_.peek().type == T_GT) return left;
        int t = lex_.peek().type;
        if (t == T_EQ || t == T_NE || t == T_LT || t == T_LE || t == T_GT || t == T_GE) {
            lex_.next();
            Node* right = parse_concat();
            Node* n = mk(E_BINARY);
            switch (t) {
                case T_EQ: n->op = OP_EQ; break;
                case T_NE: n->op = OP_NE; break;
                case T_LT: n->op = OP_LT; break;
                case T_LE: n->op = OP_LE; break;
                case T_GT: n->op = OP_GT; break;
                case T_GE: n->op = OP_GE; break;
            }
            n->a = left; n->b = right;
            return n;
        }
        return left;
    }

    Node* parse_concat() {
        Node* left = parse_add();
        while (is_primary_start()) {
            Node* right = parse_add();
            Node* n = mk(E_BINARY);
            n->op = OP_CONCAT; n->a = left; n->b = right;
            left = n;
        }
        return left;
    }

    Node* parse_add() {
        Node* left = parse_mul();
        while (lex_.peek().type == T_PLUS || lex_.peek().type == T_MINUS) {
            int t = lex_.next().type;
            Node* right = parse_mul();
            Node* n = mk(E_BINARY);
            n->op = (t == T_PLUS) ? OP_ADD : OP_SUB;
            n->a = left; n->b = right;
            left = n;
        }
        return left;
    }

    Node* parse_mul() {
        Node* left = parse_unary();
        while (lex_.peek().type == T_MUL || lex_.peek().type == T_DIV ||
               lex_.peek().type == T_MOD) {
            int t = lex_.next().type;
            Node* right = parse_unary();
            Node* n = mk(E_BINARY);
            n->op = (t == T_MUL) ? OP_MUL : (t == T_DIV) ? OP_DIV : OP_MOD;
            n->a = left; n->b = right;
            left = n;
        }
        return left;
    }

    Node* parse_pow() {
        Node* base = parse_postfix();
        if (lex_.peek().type == T_POW) {
            lex_.next();
            Node* exp = parse_unary();
            Node* n = mk(E_BINARY);
            n->op = OP_POW; n->a = base; n->b = exp;
            return n;
        }
        return base;
    }

    Node* parse_unary() {
        int t = lex_.peek().type;
        if (t == T_MINUS) {
            lex_.next();
            Node* n = mk(E_UNARY);
            n->op = OP_NEG; n->a = parse_unary();
            return n;
        }
        if (t == T_PLUS) { lex_.next(); return parse_unary(); }
        if (t == T_NOT) {
            lex_.next();
            Node* n = mk(E_UNARY);
            n->op = OP_NOT; n->a = parse_unary();
            return n;
        }
        if (t == T_DOLLAR) {
            lex_.next();
            Node* n = mk(E_FIELD);
            n->a = parse_unary();
            return n;
        }
        if (t == T_INCR || t == T_DECR) {
            lex_.next();
            Node* n = mk(E_ASSIGN);
            n->op = (t == T_INCR) ? OP_PREINC : OP_PREDEC;
            n->a = parse_unary();
            return n;
        }
        return parse_pow();
    }

    Node* parse_postfix() {
        Node* n = parse_primary();
        while (true) {
            int t = lex_.peek().type;
            if (t == T_LS) {
                lex_.next();
                n->kids.push_back(parse_expr());
                while (lex_.peek().type == T_COMMA) {
                    lex_.next();
                    n->kids.push_back(parse_expr());
                }
                expect(T_RS);
            } else if (t == T_INCR || t == T_DECR) {
                lex_.next();
                Node* inc = mk(E_ASSIGN);
                inc->op = (t == T_INCR) ? OP_POSTINC : OP_POSTDEC;
                inc->a = n;
                n = inc;
            } else if (t == T_LP) {
                lex_.next();
                Node* call = mk(E_CALL);
                call->str = n->str;
                if (lex_.peek().type != T_RP) {
                    call->kids.push_back(parse_expr());
                    while (lex_.peek().type == T_COMMA) {
                        lex_.next();
                        call->kids.push_back(parse_expr());
                    }
                }
                expect(T_RP);
                n = call;
            } else {
                break;
            }
        }
        return n;
    }

    Node* parse_primary() {
        const Token& t = lex_.peek();
        if (t.type == T_NUM) {
            lex_.next();
            Node* n = mk(E_NUM);
            n->num = t.num;
            return n;
        }
        if (t.type == T_STR) {
            lex_.next();
            Node* n = mk(E_STR);
            n->str = t.text;
            return n;
        }
        if (t.type == T_REGEX) {
            lex_.next();
            Node* n = mk(E_REGEX);
            n->str = t.text;
            return n;
        }
        if (t.type == T_LP) {
            lex_.next();
            Node* e = parse_expr();
            expect(T_RP);
            return e;
        }
        if (t.type == T_IDENT) {
            if (t.text == "getline") return parse_getline();
            lex_.next();
            Node* n = mk(E_VAR);
            n->str = t.text;
            return n;
        }
        // error fallback
        lex_.next();
        return mk(E_NUM);
    }

    Node* parse_function() {
        lex_.next(); // function
        std::string name = expect(T_IDENT).text;
        expect(T_LP);
        Node* fn = mk(S_BLOCK);
        fn->kind = S_BLOCK;
        if (lex_.peek().type != T_RP) {
            fn->argnames.push_back(expect(T_IDENT).text);
            while (lex_.peek().type == T_COMMA) {
                lex_.next();
                fn->argnames.push_back(expect(T_IDENT).text);
            }
        }
        expect(T_RP);
        Node* body = parse_block();
        fn->kids = body->kids;
        funcs_[name] = fn;
        return fn;
    }

    Token expect(int type) {
        Token t = lex_.next();
        if (t.type != type) {
            // best-effort; continue
        }
        return t;
    }
};

// ---------------------------------------------------------------------------
// Interpreter
// ---------------------------------------------------------------------------

struct Flow {
    enum { NORMAL, BREAK, CONTINUE, NEXT, EXIT, RETURN } kind = NORMAL;
    Value val;
};

class Awk {
public:
    void run(Parser& p, const std::vector<std::string>& files,
             const std::vector<std::pair<std::string, std::string>>& assigns) {
        for (const auto& kv : assigns) set_var(kv.first, Value(kv.second));

        parser_ = &p;
        setup_argv(files);

        for (Node* b : p.begin_blocks) exec_stmt(b);

        rng_active_.assign(p.rules.size(), false);
        int rule_idx = 0;
        for (Node* blk : p.rules) { (void)blk; rng_active_[rule_idx++] = false; }

        bool exited = false;
        if (arg_files_.empty()) {
            if (open_stream("-")) exited = process_current();
        } else {
            for (const std::string& f : arg_files_) {
                if (f.empty()) continue;
                if (!open_stream(f)) continue;
                bool e = process_current();
                if (e) { exited = true; break; }
            }
        }

        for (Node* e : p.end_blocks) exec_stmt(e);
        close_streams();
    }

private:
    Parser* parser_ = nullptr;
    std::unordered_map<std::string, Value> vars_;
    std::unordered_map<std::string, std::unordered_map<std::string, Value>> arrays_;
    std::vector<std::unordered_map<std::string, Value>> scopes_;

    std::string g_record;
    std::vector<std::string> g_fields;
    long nr_ = 0, fnr_ = 0;
    std::string filename_;
    std::string fs_ = " ";
    std::string ofs_ = " ";
    std::string ors_ = "\n";
    std::string rs_ = "\n";
    std::string ofmt_ = "%.6g";
    std::string convfmt_ = "%.6g";
    std::string subsep_ = "\034";
    long rstart_ = 0, rlength_ = 0;

    std::vector<std::string> arg_files_;
    std::vector<char> rng_active_;
    long argc_ = 0;
    std::unordered_map<std::string, std::unique_ptr<std::ifstream>> getline_files_;

    std::istream* cur_in_ = nullptr;
    std::ifstream cur_file_;
    std::string cur_name_;
    std::unordered_map<std::string, std::ofstream> out_files_;

    // ----- builtin var accessors -----
    const std::string& awk_convfmt() { return convfmt_; }
    const std::string& awk_ofmt() { return ofmt_; }

    bool is_special(const std::string& n) {
        static const char* sp[] = {"NF","NR","FNR","FS","OFS","ORS","RS","FILENAME",
            "OFMT","CONVFMT","SUBSEP","RSTART","RLENGTH","ARGC","ARGV",nullptr};
        for (int i = 0; sp[i]; i++) if (n == sp[i]) return true;
        return false;
    }

    Value get_var(const std::string& n) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto f = it->find(n);
            if (f != it->end()) return f->second;
        }
        if (n == "NF") return Value((double)g_fields.size());
        if (n == "NR") return Value((double)nr_);
        if (n == "FNR") return Value((double)fnr_);
        if (n == "FS") return Value(fs_);
        if (n == "OFS") return Value(ofs_);
        if (n == "ORS") return Value(ors_);
        if (n == "RS") return Value(rs_);
        if (n == "FILENAME") return Value(filename_);
        if (n == "OFMT") return Value(ofmt_);
        if (n == "CONVFMT") return Value(convfmt_);
        if (n == "SUBSEP") return Value(subsep_);
        if (n == "RSTART") return Value((double)rstart_);
        if (n == "RLENGTH") return Value((double)rlength_);
        if (n == "ARGC") return Value((double)argc_);
        auto f = vars_.find(n);
        if (f != vars_.end()) return f->second;
        return Value(0.0);
    }

    void set_var(const std::string& n, const Value& v) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto f = it->find(n);
            if (f != it->end()) { f->second = v; return; }
        }
        if (n == "FS") { fs_ = val_to_str(v, convfmt_); split_fields(g_record, fs_, g_fields); return; }
        if (n == "OFS") { ofs_ = val_to_str(v, convfmt_); return; }
        if (n == "ORS") { ors_ = val_to_str(v, convfmt_); return; }
        if (n == "RS") { rs_ = val_to_str(v, convfmt_); return; }
        if (n == "OFMT") { ofmt_ = val_to_str(v, convfmt_); return; }
        if (n == "CONVFMT") { convfmt_ = val_to_str(v, convfmt_); return; }
        if (n == "SUBSEP") { subsep_ = val_to_str(v, convfmt_); return; }
        if (n == "NF") { set_nf((long)val_to_num(v)); return; }
        if (n == "NR" || n == "FNR" || n == "FILENAME" || n == "RSTART" ||
            n == "RLENGTH" || n == "ARGC") return;
        vars_[n] = v;
    }

    std::unordered_map<std::string, Value>& get_array(const std::string& n) {
        return arrays_[n];
    }

    std::string array_key(Node* idxnode) {
        std::string k;
        for (size_t i = 0; i < idxnode->kids.size(); i++) {
            if (i) k += subsep_;
            k += val_to_str(eval(idxnode->kids[i]), convfmt_);
        }
        return k;
    }

    Value get_array_elem(const std::string& name, Node* idxnode) {
        auto& arr = get_array(name);
        auto f = arr.find(array_key(idxnode));
        if (f != arr.end()) return f->second;
        return Value(0.0);
    }

    void set_array_elem(const std::string& name, Node* idxnode, const Value& v) {
        get_array(name)[array_key(idxnode)] = v;
    }

    // ----- fields -----
    void split_fields(const std::string& rec, const std::string& fs,
                      std::vector<std::string>& out) {
        out.clear();
        if (fs.empty()) {
            for (char c : rec) out.push_back(std::string(1, c));
            return;
        }
        if (fs == " ") {
            size_t i = 0, n = rec.size();
            while (i < n && std::isspace((unsigned char)rec[i])) i++;
            while (i < n) {
                size_t j = i;
                while (j < n && !std::isspace((unsigned char)rec[j])) j++;
                out.push_back(rec.substr(i, j - i));
                while (j < n && std::isspace((unsigned char)rec[j])) j++;
                i = j;
            }
            return;
        }
        if (fs.size() == 1) {
            size_t i = 0;
            while (true) {
                size_t j = rec.find(fs[0], i);
                if (j == std::string::npos) { out.push_back(rec.substr(i)); break; }
                out.push_back(rec.substr(i, j - i));
                i = j + 1;
            }
            return;
        }
        try {
            std::regex re(fs);
            std::sregex_iterator it(rec.begin(), rec.end(), re);
            std::sregex_iterator end;
            size_t pos = 0;
            for (; it != end; ++it) {
                out.push_back(rec.substr(pos, it->position() - pos));
                pos = it->position() + it->length();
            }
            out.push_back(rec.substr(pos));
        } catch (...) {
            out.clear();
            out.push_back(rec);
        }
    }

    void rebuild_record() {
        std::string r;
        for (size_t i = 0; i < g_fields.size(); i++) {
            if (i) r += ofs_;
            r += g_fields[i];
        }
        g_record = r;
    }

    void set_record(const std::string& rec) {
        g_record = rec;
        split_fields(g_record, fs_, g_fields);
    }

    Value get_field(long n) {
        if (n <= 0) return Value(g_record);
        if ((size_t)n > g_fields.size()) return Value("");
        return Value(g_fields[n - 1]);
    }

    void set_field(long n, const Value& v) {
        if (n <= 0) {
            set_record(val_to_str(v, ofmt_));
            return;
        }
        while ((long)g_fields.size() < n) g_fields.push_back("");
        g_fields[n - 1] = val_to_str(v, ofmt_);
        rebuild_record();
    }

    void set_nf(long k) {
        if (k < 0) return;
        if ((long)g_fields.size() < k) {
            while ((long)g_fields.size() < k) g_fields.push_back("");
        } else {
            g_fields.resize(k);
        }
        rebuild_record();
    }

    // ----- lvalue helpers -----
    std::string lvalue_str(Node* n) {
        if (n->kind == E_FIELD) {
            return val_to_str(get_field((long)val_to_num(eval(n->a))), convfmt_);
        }
        if (n->kind == E_VAR) {
            if (n->kids.empty()) return val_to_str(get_var(n->str), convfmt_);
            return val_to_str(get_array_elem(n->str, n), convfmt_);
        }
        return val_to_str(eval(n), convfmt_);
    }

    void assign_lvalue(Node* n, const Value& v) {
        if (n->kind == E_FIELD) {
            set_field((long)val_to_num(eval(n->a)), v);
            return;
        }
        if (n->kind == E_VAR) {
            if (n->kids.empty()) set_var(n->str, v);
            else set_array_elem(n->str, n, v);
            return;
        }
        (void)v;
    }

    // ----- evaluation -----
    Value eval(Node* n) {
        if (!n) return Value(0.0);
        switch (n->kind) {
            case E_NUM: return Value(n->num);
            case E_STR: return Value(n->str);
            case E_REGEX: {
                Value v; v.numeric = false; v.regex = true; v.str = n->str;
                return v;
            }
            case E_FIELD: {
                Value idx = eval(n->a);
                return get_field((long)val_to_num(idx));
            }
            case E_VAR: {
                if (n->kids.empty()) return get_var(n->str);
                return get_array_elem(n->str, n);
            }
            case E_IN: {
                auto& arr = get_array(n->str);
                std::string k = val_to_str(eval(n->a), convfmt_);
                return Value(arr.find(k) != arr.end() ? 1.0 : 0.0);
            }
            case E_BINARY: return eval_binary(n);
            case E_UNARY: return eval_unary(n);
            case E_COND: return val_truthy(eval(n->a), g_record) ? eval(n->b) : eval(n->c);
            case E_ASSIGN: return eval_assign(n);
            case E_CALL: return eval_call(n);
            case S_GETLINE: return Value(do_getline(n) ? 1.0 : 0.0);
            default: return Value(0.0);
        }
    }

    Value eval_binary(Node* n) {
        Value l = eval(n->a);
        Value r = eval(n->b);
        switch (n->op) {
            case OP_ADD: return Value(val_to_num(l) + val_to_num(r));
            case OP_SUB: return Value(val_to_num(l) - val_to_num(r));
            case OP_MUL: return Value(val_to_num(l) * val_to_num(r));
            case OP_DIV: return Value(val_to_num(l) / val_to_num(r));
            case OP_MOD: return Value(std::fmod(val_to_num(l), val_to_num(r)));
            case OP_POW: return Value(std::pow(val_to_num(l), val_to_num(r)));
            case OP_CONCAT:
                return Value(val_to_str(l, convfmt_) + val_to_str(r, convfmt_));
            case OP_EQ: return Value(compare_values(l, r) == 0 ? 1.0 : 0.0);
            case OP_NE: return Value(compare_values(l, r) != 0 ? 1.0 : 0.0);
            case OP_LT: return Value(compare_values(l, r) < 0 ? 1.0 : 0.0);
            case OP_LE: return Value(compare_values(l, r) <= 0 ? 1.0 : 0.0);
            case OP_GT: return Value(compare_values(l, r) > 0 ? 1.0 : 0.0);
            case OP_GE: return Value(compare_values(l, r) >= 0 ? 1.0 : 0.0);
            case OP_AND: return Value((val_truthy(l, g_record) && val_truthy(r, g_record)) ? 1.0 : 0.0);
            case OP_OR: return Value((val_truthy(l, g_record) || val_truthy(r, g_record)) ? 1.0 : 0.0);
            case OP_MATCH: return Value(regex_search(val_to_str(l, convfmt_), val_to_str(r, convfmt_)) ? 1.0 : 0.0);
            case OP_NMATCH: return Value(regex_search(val_to_str(l, convfmt_), val_to_str(r, convfmt_)) ? 0.0 : 1.0);
            default: return Value(0.0);
        }
    }

    Value eval_unary(Node* n) {
        Value v = eval(n->a);
        switch (n->op) {
            case OP_NEG: return Value(-val_to_num(v));
            case OP_NOT: return Value(val_truthy(v, g_record) ? 0.0 : 1.0);
            default: return v;
        }
    }

    Value eval_assign(Node* n) {
        if (n->op == OP_PREINC || n->op == OP_PREDEC ||
            n->op == OP_POSTINC || n->op == OP_POSTDEC) {
            double cur = val_to_num(eval(n->a));
            double nv = (n->op == OP_PREINC || n->op == OP_POSTINC) ? cur + 1 : cur - 1;
            assign_lvalue(n->a, Value(nv));
            return Value((n->op == OP_PREINC || n->op == OP_PREDEC) ? nv : cur);
        }
        Value rhs = eval(n->b);
        if (n->op == OP_ADD_A || n->op == OP_SUB_A || n->op == OP_MUL_A ||
            n->op == OP_DIV_A || n->op == OP_MOD_A || n->op == OP_POW_A) {
            double cur = val_to_num(eval(n->a));
            double nv = cur;
            switch (n->op) {
                case OP_ADD_A: nv = cur + val_to_num(rhs); break;
                case OP_SUB_A: nv = cur - val_to_num(rhs); break;
                case OP_MUL_A: nv = cur * val_to_num(rhs); break;
                case OP_DIV_A: nv = cur / val_to_num(rhs); break;
                case OP_MOD_A: nv = std::fmod(cur, val_to_num(rhs)); break;
                case OP_POW_A: nv = std::pow(cur, val_to_num(rhs)); break;
            }
            assign_lvalue(n->a, Value(nv));
            return Value(nv);
        }
        assign_lvalue(n->a, rhs);
        return rhs;
    }

    bool regex_search(const std::string& s, const std::string& pat) {
        try {
            std::regex re(pat.empty() ? ".*" : pat);
            return std::regex_search(s, re);
        } catch (...) {
            return false;
        }
    }

    Value eval_call(Node* n) {
        const std::string& name = n->str;
        // builtins
        if (name == "@range") {
            // not a real call; handled by pattern matching externally
            return Value(0.0);
        }
        if (name == "length") {
            if (n->kids.empty()) return Value((double)g_record.size());
            Value a = eval(n->kids[0]);
            if (!a.numeric && n->kids[0]->kind == E_VAR && !n->kids[0]->kids.empty()) {
                return Value((double)get_array(n->kids[0]->str).size());
            }
            return Value((double)val_to_str(a, convfmt_).size());
        }
        if (name == "substr") {
            std::string s = val_to_str(eval(n->kids[0]), convfmt_);
            long m = (long)val_to_num(eval(n->kids[1]));
            if (m < 1) m = 1;
            if (m > (long)s.size()) return Value("");
            long len = (n->kids.size() > 2) ? (long)val_to_num(eval(n->kids[2])) : (long)s.size();
            if (len < 0) len = 0;
            if (m + len > (long)s.size()) len = (long)s.size() - m + 1;
            return Value(s.substr(m - 1, len));
        }
        if (name == "index") {
            std::string s = val_to_str(eval(n->kids[0]), convfmt_);
            std::string t = val_to_str(eval(n->kids[1]), convfmt_);
            size_t p = s.find(t);
            return Value(p == std::string::npos ? 0.0 : (double)(p + 1));
        }
        if (name == "split") {
            std::string s = val_to_str(eval(n->kids[0]), convfmt_);
            std::string sep = (n->kids.size() > 2) ? val_to_str(eval(n->kids[2]), convfmt_) : fs_;
            std::vector<std::string> parts;
            split_fields(s, sep, parts);
            auto& arr = get_array(n->kids[1]->str);
            arr.clear();
            for (size_t i = 0; i < parts.size(); i++)
                arr[std::to_string(i + 1)] = Value(parts[i]);
            return Value((double)parts.size());
        }
        if (name == "tolower") {
            std::string s = val_to_str(eval(n->kids[0]), convfmt_);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value(s);
        }
        if (name == "toupper") {
            std::string s = val_to_str(eval(n->kids[0]), convfmt_);
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value(s);
        }
        if (name == "match") {
            std::string s = val_to_str(eval(n->kids[0]), convfmt_);
            std::string pat = val_to_str(eval(n->kids[1]), convfmt_);
            try {
                std::regex re(pat);
                std::smatch m;
                if (std::regex_search(s, m, re)) {
                    rstart_ = m.position() + 1;
                    rlength_ = m.length();
                    return Value((double)rstart_);
                }
            } catch (...) {}
            rstart_ = 0; rlength_ = -1;
            return Value(0.0);
        }
        if (name == "int") return Value(std::floor(val_to_num(eval(n->kids[0]))));
        if (name == "sqrt") return Value(std::sqrt(val_to_num(eval(n->kids[0]))));
        if (name == "exp") return Value(std::exp(val_to_num(eval(n->kids[0]))));
        if (name == "log") return Value(std::log(val_to_num(eval(n->kids[0]))));
        if (name == "sin") return Value(std::sin(val_to_num(eval(n->kids[0]))));
        if (name == "cos") return Value(std::cos(val_to_num(eval(n->kids[0]))));
        if (name == "atan2") return Value(std::atan2(val_to_num(eval(n->kids[0])), val_to_num(eval(n->kids[1]))));
        if (name == "rand") return Value(std::rand() / (double)RAND_MAX);
        if (name == "srand") {
            double seed = n->kids.empty() ? (double)std::time(nullptr) : val_to_num(eval(n->kids[0]));
            std::srand((unsigned int)seed);
            return Value(seed);
        }
        if (name == "sprintf") {
            return Value(format_printf(val_to_str(eval(n->kids[0]), convfmt_), n->kids, 1));
        }
        if (name == "sub" || name == "gsub") {
            return Value(do_sub(name == "gsub", n));
        }
        if (name == "system") {
            int rc = std::system(val_to_str(eval(n->kids[0]), convfmt_).c_str());
            return Value((double)rc);
        }
        if (name == "close") {
            return Value(0.0);
        }

        // user function
        auto f = parser_->functions().find(name);
        if (f != parser_->functions().end()) {
            return call_user(f->second, n);
        }
        return Value(0.0);
    }

    Value call_user(Node* fn, Node* call) {
        scopes_.push_back(std::unordered_map<std::string, Value>());
        size_t nargs = call->kids.size();
        for (size_t i = 0; i < fn->argnames.size(); i++) {
            if (i < nargs) scopes_.back()[fn->argnames[i]] = eval(call->kids[i]);
            else scopes_.back()[fn->argnames[i]] = Value(0.0);
        }
        Flow flow;
        for (Node* st : fn->kids) {
            flow = exec_stmt(st);
            if (flow.kind == Flow::RETURN) break;
            if (flow.kind == Flow::EXIT || flow.kind == Flow::NEXT) break;
        }
        Value rv = (flow.kind == Flow::RETURN) ? flow.val : Value(0.0);
        scopes_.pop_back();
        return rv;
    }

    long do_sub(bool global, Node* n) {
        std::string pat = val_to_str(eval(n->kids[0]), convfmt_);
        std::string repl = val_to_str(eval(n->kids[1]), convfmt_);
        Node* target = (n->kids.size() > 2) ? n->kids[2] : nullptr;
        std::string text = target ? lvalue_str(target) : g_record;
        std::string result = text;
        long count = 0;
        try {
            std::regex re(pat);
            if (global) {
                std::string out;
                size_t pos = 0;
                auto begin = std::sregex_iterator(text.begin(), text.end(), re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it) {
                    out += text.substr(pos, it->position() - pos);
                    out += expand_repl(repl, *it);
                    pos = it->position() + it->length();
                    count++;
                }
                out += text.substr(pos);
                result = out;
            } else {
                std::smatch m;
                if (std::regex_search(text, m, re)) {
                    result = text.substr(0, m.position()) + expand_repl(repl, m) +
                             text.substr(m.position() + m.length());
                    count = 1;
                }
            }
        } catch (...) {}
        if (target) assign_lvalue(target, Value(result));
        else set_record(result);
        return count;
    }

    std::string expand_repl(const std::string& repl, const std::smatch& m) {
        std::string out;
        for (size_t i = 0; i < repl.size(); i++) {
            char c = repl[i];
            if (c == '&') { out += m.str(); }
            else if (c == '\\' && i + 1 < repl.size()) {
                char e = repl[++i];
                if (e >= '1' && e <= '9') {
                    int idx = e - '0';
                    if (idx < (int)m.size()) out += m[idx].str();
                } else if (e == '&') out += '&';
                else if (e == '\\') out += '\\';
                else { out += '\\'; out += e; }
            } else out += c;
        }
        return out;
    }

    std::string format_printf(const std::string& fmt, std::vector<Node*>& kids, size_t start) {
        std::string out;
        size_t ai = start;
        for (size_t i = 0; i < fmt.size();) {
            if (fmt[i] == '%') {
                size_t j = i + 1;
                std::string spec = "%";
                while (j < fmt.size() && std::strchr("-+ #0", fmt[j])) { spec += fmt[j]; j++; }
                if (j < fmt.size() && fmt[j] == '*') {
                    spec += std::to_string((long)val_to_num(eval(kids[ai++])));
                    j++;
                } else {
                    while (j < fmt.size() && std::isdigit((unsigned char)fmt[j])) { spec += fmt[j]; j++; }
                }
                if (j < fmt.size() && fmt[j] == '.') {
                    spec += '.'; j++;
                    if (j < fmt.size() && fmt[j] == '*') {
                        spec += std::to_string((long)val_to_num(eval(kids[ai++])));
                        j++;
                    } else {
                        while (j < fmt.size() && std::isdigit((unsigned char)fmt[j])) { spec += fmt[j]; j++; }
                    }
                }
                while (j < fmt.size() && std::strchr("hlL", fmt[j])) { spec += fmt[j]; j++; }
                char conv = (j < fmt.size()) ? fmt[j] : '%';
                j++;
                spec += conv;
                out += apply_conv(spec, conv, kids, ai);
                i = j;
            } else {
                out += fmt[i];
                i++;
            }
        }
        return out;
    }

    std::string apply_conv(const std::string& spec, char conv,
                           std::vector<Node*>& kids, size_t& ai) {
        char buf[1024];
        if (conv == '%') return "%";
        if (conv == 's') {
            std::string s = (ai < kids.size()) ? val_to_str(eval(kids[ai++]), convfmt_) : std::string("");
            std::snprintf(buf, sizeof(buf), spec.c_str(), s.c_str());
            return buf;
        }
        if (conv == 'c') {
            int ch = (ai < kids.size()) ? (int)val_to_num(eval(kids[ai++])) : 0;
            std::snprintf(buf, sizeof(buf), spec.c_str(), ch);
            return buf;
        }
        double d = (ai < kids.size()) ? val_to_num(eval(kids[ai++])) : 0.0;
        if (conv == 'd' || conv == 'i') {
            std::snprintf(buf, sizeof(buf), spec.c_str(), (long long)d);
            return buf;
        }
        if (conv == 'u' || conv == 'o' || conv == 'x' || conv == 'X') {
            std::snprintf(buf, sizeof(buf), spec.c_str(), (unsigned long long)(long long)d);
            return buf;
        }
        std::snprintf(buf, sizeof(buf), spec.c_str(), d);
        return buf;
    }

    // ----- statements -----
    Flow exec_stmt(Node* n) {
        Flow f;
        if (!n) return f;
        switch (n->kind) {
            case S_BLOCK: {
                for (Node* st : n->kids) {
                    f = exec_stmt(st);
                    if (f.kind != Flow::NORMAL) return f;
                }
                return f;
            }
            case S_EXPR: eval(n->a); return f;
            case S_IF: {
                if (val_truthy(eval(n->a), g_record)) return exec_stmt(n->b);
                if (n->c) return exec_stmt(n->c);
                return f;
            }
            case S_WHILE: {
                while (val_truthy(eval(n->a), g_record)) {
                    f = exec_stmt(n->b);
                    if (f.kind == Flow::BREAK) { f.kind = Flow::NORMAL; break; }
                    if (f.kind == Flow::CONTINUE) { f.kind = Flow::NORMAL; continue; }
                    if (f.kind == Flow::NEXT || f.kind == Flow::EXIT ||
                        f.kind == Flow::RETURN) return f;
                }
                return f;
            }
            case S_FOR: {
                if (n->a) eval(n->a);
                while (!n->b || val_truthy(eval(n->b), g_record)) {
                    f = exec_stmt(n->kids[0]);
                    if (f.kind == Flow::BREAK) { f.kind = Flow::NORMAL; break; }
                    if (f.kind == Flow::CONTINUE) { f.kind = Flow::NORMAL; continue; }
                    if (f.kind == Flow::NEXT || f.kind == Flow::EXIT ||
                        f.kind == Flow::RETURN) return f;
                    if (n->c) eval(n->c);
                }
                return f;
            }
            case S_FOR_IN: {
                auto& arr = get_array(n->str2);
                for (auto& kv : arr) {
                    set_var(n->str, Value(kv.first));
                    f = exec_stmt(n->a);
                    if (f.kind == Flow::BREAK) { f.kind = Flow::NORMAL; break; }
                    if (f.kind == Flow::CONTINUE) { f.kind = Flow::NORMAL; continue; }
                    if (f.kind == Flow::NEXT || f.kind == Flow::EXIT ||
                        f.kind == Flow::RETURN) return f;
                }
                return f;
            }
            case S_PRINT:
            case S_PRINTF: {
                do_print(n);
                return f;
            }
            case S_NEXT: f.kind = Flow::NEXT; return f;
            case S_EXIT: f.kind = Flow::EXIT; if (n->a) f.val = eval(n->a); return f;
            case S_BREAK: f.kind = Flow::BREAK; return f;
            case S_CONTINUE: f.kind = Flow::CONTINUE; return f;
            case S_RETURN: f.kind = Flow::RETURN; if (n->a) f.val = eval(n->a); return f;
            case S_DELETE: {
                auto& arr = get_array(n->str);
                if (!n->kids.empty()) arr.erase(array_key(n));
                else arr.clear();
                return f;
            }
            case S_GETLINE: {
                f.val = Value((double)do_getline(n) ? 1.0 : 0.0);
                return f;
            }
            default: return f;
        }
    }

    void do_print(Node* n) {
        std::string out;
        if (n->kind == S_PRINTF) {
            if (!n->kids.empty()) {
                std::string fmt = val_to_str(eval(n->kids[0]), convfmt_);
                out = format_printf(fmt, n->kids, 1);
            }
        } else {
            if (n->kids.empty()) {
                out = g_record;
            } else {
                bool first = true;
                for (Node* arg : n->kids) {
                    if (!first) out += ofs_;
                    out += val_to_str(eval(arg), ofmt_);
                    first = false;
                }
            }
            out += ors_;
        }

        if (n->redir) {
            std::string fname = val_to_str(eval(n->d), convfmt_);
            auto& os = out_files_[fname];
            if (!os.is_open()) {
                os.open(fname, (n->redir == 2) ? std::ios::app : std::ios::trunc);
            }
            os << out;
            os.flush();
        } else {
            std::cout << out;
        }
    }

    bool do_getline(Node* n) {
        std::string rec;
        if (n->d) {
            std::string fname = val_to_str(eval(n->d), convfmt_);
            auto it = getline_files_.find(fname);
            std::ifstream* f = nullptr;
            if (it != getline_files_.end()) f = it->second.get();
            else {
                auto p = std::make_unique<std::ifstream>(fname);
                if (!*p) return false;
                f = p.get();
                getline_files_[fname] = std::move(p);
            }
            if (!std::getline(*f, rec)) return false;
        } else {
            if (!cur_in_ || !read_record(*cur_in_, rec)) return false;
        }
        if (n->str.empty()) set_record(rec);
        else set_var(n->str, Value(rec));
        return true;
    }

    // ----- input -----
    void setup_argv(const std::vector<std::string>& files) {
        arrays_["ARGV"].clear();
        arg_files_.clear();
        arrays_["ARGV"]["0"] = Value("awk");
        int idx = 1;
        for (const auto& f : files) {
            arrays_["ARGV"][std::to_string(idx)] = Value(f);
            arg_files_.push_back(f);
            idx++;
        }
        set_var("ARGC", Value((double)idx));
        argc_ = idx;
    }

    bool open_stream(const std::string& name) {
        if (cur_file_.is_open()) cur_file_.close();
        if (name == "-") {
            cur_in_ = &std::cin;
            cur_name_ = "-";
            filename_ = "-";
            fnr_ = 0;
            return true;
        }
        cur_file_.open(name);
        if (!cur_file_) return false;
        cur_in_ = &cur_file_;
        cur_name_ = name;
        filename_ = name;
        fnr_ = 0;
        return true;
    }

    void close_streams() {
        if (cur_file_.is_open()) cur_file_.close();
        for (auto& p : out_files_) if (p.second.is_open()) p.second.close();
    }

    bool read_record(std::istream& in, std::string& out) {
        out.clear();
        if (rs_ == "") {
            std::string line;
            bool started = false;
            bool got = false;
            while (std::getline(in, line)) {
                if (line.empty()) {
                    if (started) return got;
                    continue;
                }
                started = true;
                if (!out.empty()) out += "\n";
                out += line;
                got = true;
            }
            return got;
        }
        if (rs_ == "\n") {
            return (bool)std::getline(in, out);
        }
        int c;
        std::string acc;
        size_t matchpos = 0;
        while ((c = in.get()) != EOF) {
            char ch = (char)c;
            acc += ch;
            if (ch == rs_[matchpos]) {
                matchpos++;
                if (matchpos == rs_.size()) {
                    acc.resize(acc.size() - rs_.size());
                    out = acc;
                    return true;
                }
            } else {
                matchpos = (ch == rs_[0]) ? 1 : 0;
            }
        }
        if (!acc.empty()) { out = acc; return true; }
        return false;
    }

    bool process_current() {
        std::string rec;
        while (read_record(*cur_in_, rec)) {
            set_record(rec);
            nr_++;
            fnr_++;
            for (size_t ri = 0; ri < parser_->rules.size(); ri++) {
                if (!rule_matches(ri)) continue;
                Node* act = parser_->rules[ri];
                if (!act) {
                    do_default_print();
                    continue;
                }
                Flow f = exec_stmt(act);
                if (f.kind == Flow::NEXT) break;
                if (f.kind == Flow::EXIT) return true;
            }
        }
        return false;
    }

    void do_default_print() {
        std::cout << g_record << ors_;
    }

    bool rule_matches(size_t ri) {
        Node* pat = parser_->rule_patterns[ri];
        if (!pat) return true; // matches all
        if (pat->kind == E_CALL && pat->str == "@range") {
            bool start = val_truthy(eval(pat->a), g_record);
            bool end = val_truthy(eval(pat->b), g_record);
            char& active = rng_active_[ri];
            if (!active) {
                if (start) {
                    active = true;
                    if (end) active = false;
                    return true;
                }
                return false;
            } else {
                if (end) { active = false; return true; }
                return true;
            }
        }
        return val_truthy(eval(pat), g_record);
    }
};

static const std::string& awk_convfmt() {
    static std::string s = "%.6g";
    return s;
}
static const std::string& awk_ofmt() {
    static std::string s = "%.6g";
    return s;
}

// ---------------------------------------------------------------------------
// Command
// ---------------------------------------------------------------------------

static std::string read_prog(const char* path);
static void print_help();

void awk_command(int argc, char** argv) {
    std::string program;
    std::vector<std::string> files;
    std::vector<std::pair<std::string, std::string>> assigns;
    std::string fs;
    bool have_fs = false;
    bool program_set = false;

    auto add_assign = [&](const std::string& av) {
        size_t eq = av.find('=');
        if (eq == std::string::npos) return;
        assigns.push_back({av.substr(0, eq), av.substr(eq + 1)});
    };

    int i = 1;
    bool end_opts = false;
    for (; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--") { end_opts = true; continue; }
        if (!end_opts && a.size() > 1 && a[0] == '-') {
            if (a == "-f" || a == "-e") {
                if (i + 1 >= argc) { std::fprintf(stderr, "awk: option requires argument -- '%c'\n", a[1]); return; }
                std::string p = read_prog(argv[++i]);
                program += p + "\n";
                program_set = true;
            } else if (a.rfind("-f", 0) == 0) {
                program += read_prog(a.substr(2).c_str()) + "\n";
                program_set = true;
            } else if (a.rfind("-e", 0) == 0) {
                program += a.substr(2) + "\n";
                program_set = true;
            } else if (a == "-F") {
                if (i + 1 >= argc) return;
                fs = argv[++i];
                have_fs = true;
            } else if (a.rfind("-F", 0) == 0) {
                fs = a.substr(2);
                have_fs = true;
            } else if (a == "-v") {
                if (i + 1 >= argc) return;
                add_assign(argv[++i]);
            } else if (a.rfind("-v", 0) == 0) {
                add_assign(a.substr(2));
            } else if (a.rfind("-W", 0) == 0) {
                std::string w = a.substr(2);
                if (w.rfind("assign=", 0) == 0) add_assign(w.substr(7));
                else if (w == "version") { std::printf("GNU Awk (modbox) 1.0\n"); return; }
            } else if (a == "-d" || a.rfind("-d", 0) == 0) {
                // dump: ignore, treated as no-op
            } else if (a == "-h" || a == "--help") {
                print_help();
                return;
            } else if (a == "-V" || a == "--version") {
                std::printf("GNU Awk (modbox) 1.0\n");
                return;
            } else {
                if (!program_set) { program = a; program_set = true; }
                else { files.push_back(a); }
            }
        } else {
            if (!program_set) { program = a; program_set = true; }
            else { files.push_back(a); }
        }
    }
    for (; i < argc; i++) { files.push_back(argv[i]); }

    if (!program_set) {
        print_help();
        return;
    }

    Parser parser(program);
    try {
        parser.parse();
    } catch (...) {
        (void)std::fprintf(stderr, "awk: parse error\n");
        return;
    }

    Awk awk;
    if (have_fs) {
        assigns.insert(assigns.begin(), {"FS", fs});
    }
    awk.run(parser, files, assigns);
}

static std::string read_prog(const char* path) {
    const std::ifstream f(path);
    if (!f) { (void)std::fprintf(stderr, "awk: cannot open %s\n", path); return ""; }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void print_help() {
    std::printf(
        "Usage: awk [POSIX or GNU style options] -f progfile [--] file ...\n"
        "       awk [POSIX or GNU style options] [--] 'program' file ...\n"
        "Options:\n"
        "  -f progfile     program file\n"
        "  -F fs           input field separator\n"
        "  -v var=val      assign value to variable before program\n"
        "  -e program      program text\n"
        "  -W assign=var=val, -W version\n"
        "  --              end of options\n");
}

REGISTER_COMMAND("awk", awk_command, "Pattern scanning and processing language");
