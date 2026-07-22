#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "commands/sh.hpp"
#include "commands/command_macros.hpp"

// ── Data Structures ────────────────────────────────────────────────────────

struct Redirect {
    enum Type { INPUT, OUTPUT, APPEND, HEREDOC, FD_INPUT, FD_OUTPUT };
    Type type;
    int fd;
    bool close_flag;
    std::string target;
};

struct SimpleCommand {
    std::vector<std::string> args;
    std::vector<bool> arg_literal; // true = single-quoted, skip expansion
    std::vector<Redirect> redirects;
};

struct Pipeline {
    std::vector<SimpleCommand> commands;
};

enum class NodeType { SIMPLE, PIPELINE, IF, FOR, WHILE, UNTIL, CASE, SEQ, AND_OR };

struct Node {
    NodeType type;

    // SIMPLE / PIPELINE
    Pipeline pipeline;

    // IF
    std::unique_ptr<Node> condition;
    std::unique_ptr<Node> then_body;
    std::unique_ptr<Node> else_body;
    std::vector<std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>>> elifs;

    // FOR
    std::string for_var;
    std::vector<std::string> for_words;
    std::unique_ptr<Node> for_body;

    // WHILE / UNTIL
    std::unique_ptr<Node> loop_cond;
    std::unique_ptr<Node> loop_body;

    // CASE
    std::string case_word;
    std::vector<std::pair<std::string, std::unique_ptr<Node>>> case_entries;

    // SEQ
    std::vector<std::unique_ptr<Node>> seq_children;

    // AND_OR
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    bool is_and;
};

struct ShellState {
    std::unordered_map<std::string, std::string> vars;
    std::vector<std::string> pos_args;
    int last_exit_code = 0;
    bool errexit = false;
    bool xtrace = false;
    bool running = true;
    bool stdin_tty = false;
    std::string ps1 = "$ ";
    std::string ps2 = "> ";
    int line_number = 0;

    // Input source: used by read_line()
    enum InputMode { INPUT_INTERACTIVE, INPUT_STRING, INPUT_FILE };
    InputMode input_mode = INPUT_INTERACTIVE;
    FILE* input_file = nullptr;
    const char* input_str = nullptr;
    size_t input_str_pos = 0;
    bool input_str_done = false;
};

// ── Token Types ────────────────────────────────────────────────────────────

enum class TokenType {
    WORD,
    PIPE,
    SEMI,
    NEWLINE,
    REDIRECT,
    IF,
    THEN,
    ELIF,
    ELSE,
    FI,
    FOR,
    DO,
    DONE,
    IN,
    WHILE,
    UNTIL,
    CASE,
    ESAC,
    DSEMI,
    AND_IF,
    OR_IF,
    LPAREN,
    RPAREN,
    END
};

struct Token {
    TokenType type;
    std::string text;

    // For REDIRECT tokens
    Redirect redirect;

    // For WORD tokens: whether this was fully single-quoted (no expansion)
    bool literal = false;
};

// ── Forward Declarations ───────────────────────────────────────────────────

static ShellState g_state;

static std::string expand_string(const std::string& s, bool literal);
static void expand_simple(SimpleCommand& cmd);
static int execute_node(const Node& node);
static bool is_builtin(const std::string& name);
static int run_builtin(const std::string& name, std::vector<std::string>& args);
static void apply_redirects(const std::vector<Redirect>& redirects);
static int execute_pipeline_internal(const Pipeline& pipeline);
static bool find_in_path(const std::string& name, std::string& out);
static std::string read_line();
static std::string read_line_raw();
static std::vector<Token> tokenize(const std::string& input);
static size_t parse_pipeline_tokens(const std::vector<Token>& tokens, size_t pos,
                                    Pipeline& pipeline);
static std::unique_ptr<Node> parse_program(const std::vector<Token>& tokens);
static std::unique_ptr<Node> parse_list(const std::vector<Token>& tokens, size_t& pos);
static std::unique_ptr<Node> parse_and_or(const std::vector<Token>& tokens, size_t& pos);
static std::unique_ptr<Node> parse_pipeline_node(const std::vector<Token>& tokens, size_t& pos);
static std::unique_ptr<Node> parse_command(const std::vector<Token>& tokens, size_t& pos);
static std::unique_ptr<Node> parse_simple_command(const std::vector<Token>& tokens, size_t& pos);
static bool match(const std::vector<Token>& tokens, size_t& pos, TokenType type);
static bool match_word(const std::vector<Token>& tokens, size_t& pos, const std::string& word);

// ── ShellState Helpers ─────────────────────────────────────────────────────

static void shell_init() {
    g_state = ShellState();
    g_state.vars["PATH"] = "/usr/local/bin:/usr/bin:/bin";
    g_state.vars["IFS"] = " \t\n";
    g_state.vars["PS1"] = "$ ";
    g_state.vars["PS2"] = "> ";
    char* home = getenv("HOME");
    if (home) {
        g_state.vars["HOME"] = home;
        g_state.vars["PWD"] = home;
    } else {
        g_state.vars["HOME"] = "/";
        g_state.vars["PWD"] = "/";
    }
    g_state.vars["OLDPWD"] = g_state.vars["PWD"];
    g_state.stdin_tty = isatty(STDIN_FILENO);
    g_state.pos_args.push_back("sh");
}

// ── Read Line ──────────────────────────────────────────────────────────────

static std::string read_line_raw() {
    if (g_state.input_mode == ShellState::INPUT_STRING) {
        if (g_state.input_str_done) return "";
        const char* s = g_state.input_str + g_state.input_str_pos;
        if (*s == '\0') {
            g_state.input_str_done = true;
            return "";
        }
        const char* nl = strchr(s, '\n');
        std::string line;
        if (nl) {
            line.assign(s, nl - s);
            g_state.input_str_pos += (nl - s) + 1;
        } else {
            line = s;
            g_state.input_str_pos += strlen(s);
            g_state.input_str_done = true;
        }
        return line;
    }

    if (g_state.input_mode == ShellState::INPUT_FILE && g_state.input_file) {
        char buf[4096];
        if (!fgets(buf, sizeof(buf), g_state.input_file)) return "";
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        return buf;
    }

    // Interactive
    std::string line;
    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        line += static_cast<char>(c);
    }
    if (c == EOF && line.empty()) return "";
    return line;
}

// ── Tokenizer ──────────────────────────────────────────────────────────────

static bool is_redirect_char(char c) {
    return c == '>' || c == '<';
}

static std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> result;
    size_t i = 0;
    size_t len = input.size();

    auto skip_ws = [&]() {
        while (i < len && (input[i] == ' ' || input[i] == '\t')) i++;
    };

    while (i < len) {
        skip_ws();
        if (i >= len) break;

        char c = input[i];

        // Comment
        if (c == '#') break;

        // Newline
        if (c == '\n') {
            i++;
            result.push_back({TokenType::NEWLINE, "\n", {}, false});
            continue;
        }

        // Semicolon and ;;
        if (c == ';') {
            i++;
            if (i < len && input[i] == ';') {
                i++;
                result.push_back({TokenType::DSEMI, ";;", {}, false});
            } else {
                result.push_back({TokenType::SEMI, ";", {}, false});
            }
            continue;
        }

        // || and |
        if (c == '|') {
            if (i + 1 < len && input[i + 1] == '|') {
                i += 2;
                result.push_back({TokenType::OR_IF, "||", {}, false});
                continue;
            }
            i++;
            result.push_back({TokenType::PIPE, "|", {}, false});
            continue;
        }

        // && and &
        if (c == '&') {
            if (i + 1 < len && input[i + 1] == '&') {
                i += 2;
                result.push_back({TokenType::AND_IF, "&&", {}, false});
                continue;
            }
            i++;
            continue; // ignore single & for now
        }

        // Remaining single & is now handled above

        // Parentheses
        if (c == '(') {
            i++;
            result.push_back({TokenType::LPAREN, "(", {}, false});
            continue;
        }
        if (c == ')') {
            i++;
            result.push_back({TokenType::RPAREN, ")", {}, false});
            continue;
        }

        // Redirections
        if (c == '>' || c == '<') {
            int fd = -1;
            bool is_heredoc = false;

            // Check for digit prefix (e.g., "2>")
            if (c == '>' || c == '<') {
                // No digit prefix means default fd
                fd = (c == '>') ? 1 : 0;
            }

            char op_char = c;
            i++;

            std::string op_text;
            op_text += op_char;

            Redirect::Type rtype;
            bool close_flag = false;

            if (op_char == '<') {
                if (i < len && input[i] == '<') {
                    // Here-document
                    op_text += '<';
                    i++;
                    is_heredoc = true;
                    rtype = Redirect::HEREDOC;
                } else {
                    rtype = Redirect::INPUT;
                }
                fd = 0;
            } else {
                rtype = Redirect::OUTPUT;
                fd = 1;
            }

            // Handle >> and >& and <&
            if (!is_heredoc && op_char == '>' && i < len && input[i] == '>') {
                op_text += '>';
                i++;
                rtype = Redirect::APPEND;
            } else if (!is_heredoc && i < len && input[i] == '&') {
                op_text += '&';
                i++;
                if (op_char == '>') {
                    rtype = Redirect::FD_OUTPUT;
                } else {
                    rtype = Redirect::FD_INPUT;
                }
                close_flag = true;
            }

            // Parse fd prefix by checking the character before the redirect
            // We handle leading digits by looking backward from i back past the operator
            // Actually, we need to check if a digit precedes the original < or >
            // Since we've consumed the operator, let's re-check the input

            // Actually, for "2>file", the tokenizer would see '2' first as a WORD,
            // then '>' as REDIRECT. The redirect parser handles the digit prefix
            // during parsing. So we don't need to worry about it here.
            // EXCEPT: for "2>&1" the tokenizer needs to produce "2>&1" or similar.

            // Let's handle digit-prefixed redirects by checking if we already
            // have a WORD token that's just a number:
            if (!result.empty() && result.back().type == TokenType::WORD) {
                const std::string& last = result.back().text;
                bool all_digits = !last.empty() && last[0] >= '0' && last[0] <= '9' &&
                                  std::all_of(last.begin(), last.end(),
                                              [](char ch) { return ch >= '0' && ch <= '9'; });
                if (all_digits && last.size() < 3) {
                    fd = std::stoi(last);
                    result.pop_back();
                    // For <, default fd is 0; for >, default is 1.
                    // But if user specified e.g. "2>", override to 2.
                }
            }

            Token tok;
            tok.type = TokenType::REDIRECT;
            tok.text = op_text;
            tok.redirect.type = rtype;
            tok.redirect.fd = fd;
            tok.redirect.close_flag = close_flag;

            // Read redirect target (skip whitespace)
            skip_ws();

            // For here-doc, delimiter is next word
            if (is_heredoc) {
                std::string delim;
                bool delim_quoted = false;
                bool in_q = false;
                char qchar = 0;
                while (i < len && input[i] != '\n' && input[i] != '#' &&
                       !(input[i] == ' ' || input[i] == '\t')) {
                    char ch = input[i];
                    if (!in_q && (ch == '\'' || ch == '"')) {
                        qchar = ch;
                        in_q = true;
                        delim_quoted = true;
                        i++;
                        continue;
                    }
                    if (in_q && ch == qchar) {
                        in_q = false;
                        i++;
                        continue;
                    }
                    delim += ch;
                    i++;
                }
                tok.redirect.target = delim;
                tok.redirect.close_flag = delim_quoted;
                // If delimiter is quoted, no variable expansion in here-doc body
                // We'll handle that during here-doc collection
            } else {
                // Regular redirect: read filename
                skip_ws();
                std::string target;
                bool in_sq = false, in_dq = false;
                while (i < len && input[i] != '\n' && input[i] != ' ' && input[i] != '\t' &&
                       input[i] != '#' && !is_redirect_char(input[i]) && input[i] != '|' &&
                       input[i] != ';' && input[i] != '&' && input[i] != '(' && input[i] != ')') {
                    char ch = input[i];
                    if (!in_dq && !in_sq && ch == '\'') {
                        in_sq = true;
                        i++;
                        continue;
                    }
                    if (in_sq && ch == '\'') {
                        in_sq = false;
                        i++;
                        continue;
                    }
                    if (!in_sq && !in_dq && ch == '"') {
                        in_dq = true;
                        i++;
                        continue;
                    }
                    if (in_dq && ch == '"') {
                        in_dq = false;
                        i++;
                        continue;
                    }
                    if (!in_sq && !in_dq && ch == '\\' && i + 1 < len) {
                        i++;
                        target += input[i];
                        i++;
                        continue;
                    }
                    target += ch;
                    i++;
                }
                // For >& and <&, the target could be a fd number or "-"
                tok.redirect.target = target;
                if (!target.empty() && target[0] == '-') {
                    tok.redirect.close_flag = true;
                }
            }

            result.push_back(tok);
            continue;
        }

        // Word token
        std::string word;
        bool literal = true; // becomes false if any non-single-quoted part
        bool in_sq = false, in_dq = false;

        while (i < len) {
            char ch = input[i];

            // Whitespace ends word (outside quotes)
            if (!in_sq && !in_dq && (ch == ' ' || ch == '\t')) break;

            // Metacharacters end word (outside quotes)
            if (!in_sq && !in_dq &&
                (ch == '\n' || ch == ';' || ch == '|' || ch == '&' ||
                 ch == '>' || ch == '<' || ch == '(' || ch == ')' || ch == '#')) {
                break;
            }

            if (ch == '\\' && !in_sq && i + 1 < len) {
                i++;
                word += input[i];
                i++;
                literal = false;
                continue;
            }

            if (!in_dq && !in_sq && ch == '\'') {
                in_sq = true;
                i++;
                continue;
            }
            if (in_sq && ch == '\'') {
                in_sq = false;
                i++;
                continue;
            }
            if (!in_sq && !in_dq && ch == '"') {
                in_dq = true;
                literal = false;
                i++;
                continue;
            }
            if (in_dq && ch == '"') {
                in_dq = false;
                i++;
                continue;
            }
            if (in_dq && ch == '\\' && i + 1 < len) {
                char next = input[i + 1];
                if (next == '"' || next == '\\' || next == '`' || next == '$') {
                    i++;
                    word += next;
                    i++;
                    continue;
                }
            }

            if (!in_sq) literal = false;
            word += ch;
            i++;
        }

        if (!word.empty()) {
            // Check for keywords (only at start of a new command context,
            // but we'll handle this in parsing; for now just emit as WORD)
            Token tok;
            tok.type = TokenType::WORD;
            tok.text = word;
            tok.literal = literal;
            result.push_back(tok);
        }
    }

    return result;
}

// ── Variable Expansion ─────────────────────────────────────────────────────

static std::string expand_string(const std::string& s, bool literal) {
    if (literal) return s;

    std::string result;
    size_t i = 0;
    size_t len = s.size();

    while (i < len) {
        if (s[i] == '$' && i + 1 < len) {
            if (s[i + 1] == '{') {
                // ${VAR} or ${VAR:-default}
                size_t end = s.find('}', i + 2);
                if (end == std::string::npos) {
                    result += s.substr(i);
                    break;
                }
                std::string varname = s.substr(i + 2, end - i - 2);
                std::string default_val;
                size_t colon = varname.find(":-");
                if (colon != std::string::npos) {
                    default_val = varname.substr(colon + 2);
                    varname = varname.substr(0, colon);
                }
                auto it = g_state.vars.find(varname);
                if (it != g_state.vars.end()) {
                    result += it->second;
                } else if (!default_val.empty()) {
                    result += default_val;
                }
                i = end + 1;
            } else if (s[i + 1] == '?') {
                result += std::to_string(g_state.last_exit_code);
                i += 2;
            } else if (s[i + 1] == '$') {
                result += std::to_string(getpid());
                i += 2;
            } else if (s[i + 1] == '!' || s[i + 1] == '@' || s[i + 1] == '*' ||
                       s[i + 1] == '#' || s[i + 1] == '-') {
                // Special variables: minimal support
                if (s[i + 1] == '#') {
                    result += std::to_string(g_state.pos_args.size() - 1);
                } else if (s[i + 1] == '*') {
                    for (size_t a = 1; a < g_state.pos_args.size(); a++) {
                        if (a > 1) result += ' ';
                        result += g_state.pos_args[a];
                    }
                } else if (s[i + 1] == '@') {
                    for (size_t a = 1; a < g_state.pos_args.size(); a++) {
                        if (a > 1) result += ' ';
                        result += g_state.pos_args[a];
                    }
                } else if (s[i + 1] == '-') {
                    result += "h"; // minimal: indicate we're interactive
                }
                i += 2;
            } else {
                // $NAME
                i++;
                std::string varname;
                if (i < len && (isalpha(s[i]) || s[i] == '_')) {
                    varname += s[i];
                    i++;
                    while (i < len && (isalnum(s[i]) || s[i] == '_')) {
                        varname += s[i];
                        i++;
                    }
                }
                // Check positional parameters ($1, $2, ...)
                if (!varname.empty() && varname[0] >= '0' && varname[0] <= '9') {
                    int idx = std::stoi(varname);
                    if (idx >= 0 && (size_t)idx < g_state.pos_args.size()) {
                        result += g_state.pos_args[idx];
                    }
                } else {
                    auto it = g_state.vars.find(varname);
                    if (it != g_state.vars.end()) {
                        result += it->second;
                    }
                }
            }
        } else if (s[i] == '\\' && i + 1 < len && !literal) {
            i++;
            result += s[i];
            i++;
        } else {
            result += s[i];
            i++;
        }
    }

    return result;
}

static void expand_simple(SimpleCommand& cmd) {
    for (size_t i = 0; i < cmd.args.size(); i++) {
        bool lit = i < cmd.arg_literal.size() ? cmd.arg_literal[i] : false;
        cmd.args[i] = expand_string(cmd.args[i], lit);
    }
    for (auto& redir : cmd.redirects) {
        if (!redir.close_flag) {
            redir.target = expand_string(redir.target, false);
        }
    }
}

// ── PATH Lookup ────────────────────────────────────────────────────────────

static bool find_in_path(const std::string& name, std::string& out) {
    if (name.find('/') != std::string::npos) {
        if (access(name.c_str(), X_OK) == 0) {
            out = name;
            return true;
        }
        return false;
    }

    auto it = g_state.vars.find("PATH");
    std::string path_str = (it != g_state.vars.end()) ? it->second : "/usr/local/bin:/usr/bin:/bin";

    size_t start = 0;
    while (start < path_str.size()) {
        size_t end = path_str.find(':', start);
        if (end == std::string::npos) end = path_str.size();
        std::string dir = path_str.substr(start, end - start);
        if (!dir.empty()) {
            std::string full = dir + "/" + name;
            if (access(full.c_str(), X_OK) == 0) {
                out = full;
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

// ── Parser ─────────────────────────────────────────────────────────────────

static bool match(const std::vector<Token>& tokens, size_t& pos, TokenType type) {
    if (pos < tokens.size() && tokens[pos].type == type) {
        pos++;
        return true;
    }
    return false;
}

static bool match_word(const std::vector<Token>& tokens, size_t& pos, const std::string& word) {
    if (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
        tokens[pos].text == word) {
        pos++;
        return true;
    }
    return false;
}

static size_t parse_simple_command_tokens(const std::vector<Token>& tokens, size_t pos,
                                          SimpleCommand& cmd) {
    bool had_command = false;
    while (pos < tokens.size()) {
        const Token& tok = tokens[pos];

        if (tok.type == TokenType::REDIRECT) {
            Redirect r = tok.redirect;
            pos++;
            if (r.type == Redirect::HEREDOC) {
                // Already have target set by tokenizer
            } else if (r.type == Redirect::FD_INPUT || r.type == Redirect::FD_OUTPUT) {
                // Target is a fd number or "-"
                if (pos < tokens.size() && tokens[pos].type == TokenType::WORD) {
                    r.target = tokens[pos].text;
                    pos++;
                }
            } else {
                if (pos < tokens.size() && tokens[pos].type == TokenType::WORD) {
                    r.target = tokens[pos].text;
                    pos++;
                }
            }
            cmd.redirects.push_back(r);
            continue;
        }

        if (tok.type == TokenType::WORD) {
    cmd.args.push_back(tok.text);
    cmd.arg_literal.push_back(tok.literal);
    had_command = true;
            pos++;
            continue;
        }

        // End of simple command
        break;
    }

    return pos;
}

static size_t parse_pipeline_tokens(const std::vector<Token>& tokens, size_t pos,
                                    Pipeline& pipeline) {
    while (pos < tokens.size()) {
        SimpleCommand cmd;
        pos = parse_simple_command_tokens(tokens, pos, cmd);
        pipeline.commands.push_back(cmd);

        if (pos < tokens.size() && tokens[pos].type == TokenType::PIPE) {
            pos++;
            continue;
        }
        break;
    }
    return pos;
}

// ── AST Parser ─────────────────────────────────────────────────────────────

static std::unique_ptr<Node> parse_simple_command(const std::vector<Token>& tokens, size_t& pos) {
    auto node = std::make_unique<Node>();
    node->type = NodeType::SIMPLE;
    pos = parse_simple_command_tokens(tokens, pos, node->pipeline.commands.emplace_back());
    if (node->pipeline.commands[0].args.empty() &&
        node->pipeline.commands[0].redirects.empty()) {
        return nullptr;
    }
    return node;
}

static std::unique_ptr<Node> parse_command(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size()) return nullptr;

    // Check for compound commands
    if (tokens[pos].type == TokenType::WORD) {
        const std::string& w = tokens[pos].text;

        if (w == "if" || w == "IF") {
            pos++;
            auto node = std::make_unique<Node>();
            node->type = NodeType::IF;

            // Condition
            node->condition = parse_and_or(tokens, pos);
            if (!node->condition) {
                // Allow empty condition
                node->condition = std::make_unique<Node>();
                node->condition->type = NodeType::SIMPLE;
            }

            // Expect newline or semicolon before then
            if (pos < tokens.size() && (tokens[pos].type == TokenType::NEWLINE ||
                                        tokens[pos].type == TokenType::SEMI)) {
                pos++;
            }

            // then
            if (pos >= tokens.size() || !match_word(tokens, pos, "then")) {
                // Parse error: expected then
                auto fallback = std::make_unique<Node>();
                fallback->type = NodeType::SEQ;
                // Add what we have so far
                return fallback;
            }
            // newline after then
            if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;

            // then body
            node->then_body = parse_list(tokens, pos);

            // elif / else / fi
            while (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
                   tokens[pos].text == "elif") {
                pos++;
                auto elif_cond = parse_and_or(tokens, pos);
                if (pos < tokens.size() && (tokens[pos].type == TokenType::NEWLINE ||
                                            tokens[pos].type == TokenType::SEMI)) {
                    pos++;
                }
                if (pos >= tokens.size() || !match_word(tokens, pos, "then")) break;
                if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;
                auto elif_body = parse_list(tokens, pos);
                node->elifs.emplace_back(std::move(elif_cond), std::move(elif_body));
            }

            if (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
                tokens[pos].text == "else") {
                pos++;
                if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;
                node->else_body = parse_list(tokens, pos);
            }

            if (pos >= tokens.size() || !match_word(tokens, pos, "fi")) {
                // Missing fi - create anyway
            }
            return node;
        }

        if (w == "for" || w == "FOR") {
            pos++;
            auto node = std::make_unique<Node>();
            node->type = NodeType::FOR;

            if (pos < tokens.size() && tokens[pos].type == TokenType::WORD) {
                node->for_var = tokens[pos].text;
                pos++;
            } else {
                return nullptr;
            }

            if (pos < tokens.size() && match_word(tokens, pos, "in")) {
                while (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
                       tokens[pos].text != "do" && tokens[pos].text != "DO") {
                    node->for_words.push_back(tokens[pos].text);
                    pos++;
                }
            }

            // semicolon or newline before do
            if (pos < tokens.size() && (tokens[pos].type == TokenType::SEMI ||
                                        tokens[pos].type == TokenType::NEWLINE)) {
                pos++;
            }

            if (pos >= tokens.size() || !match_word(tokens, pos, "do")) {
                return nullptr;
            }
            if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;

            node->for_body = parse_list(tokens, pos);

            // Optionally match "done"
            if (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
                tokens[pos].text == "done") {
                pos++;
            }
            return node;
        }

        if (w == "while" || w == "WHILE") {
            pos++;
            auto node = std::make_unique<Node>();
            node->type = NodeType::WHILE;
            node->loop_cond = parse_and_or(tokens, pos);
            if (!node->loop_cond) return nullptr;

            if (pos < tokens.size() && (tokens[pos].type == TokenType::NEWLINE ||
                                        tokens[pos].type == TokenType::SEMI)) {
                pos++;
            }
            if (pos >= tokens.size() || !match_word(tokens, pos, "do")) return nullptr;
            if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;

            node->loop_body = parse_list(tokens, pos);
            if (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
                tokens[pos].text == "done") {
                pos++;
            }
            return node;
        }

        if (w == "until" || w == "UNTIL") {
            pos++;
            auto node = std::make_unique<Node>();
            node->type = NodeType::UNTIL;
            node->loop_cond = parse_and_or(tokens, pos);
            if (!node->loop_cond) return nullptr;

            if (pos < tokens.size() && (tokens[pos].type == TokenType::NEWLINE ||
                                        tokens[pos].type == TokenType::SEMI)) {
                pos++;
            }
            if (pos >= tokens.size() || !match_word(tokens, pos, "do")) return nullptr;
            if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;

            node->loop_body = parse_list(tokens, pos);
            if (pos < tokens.size() && tokens[pos].type == TokenType::WORD &&
                tokens[pos].text == "done") {
                pos++;
            }
            return node;
        }

        if (w == "case" || w == "CASE") {
            pos++;
            auto node = std::make_unique<Node>();
            node->type = NodeType::CASE;

            if (pos < tokens.size() && tokens[pos].type == TokenType::WORD) {
                node->case_word = tokens[pos].text;
                pos++;
            } else {
                return nullptr;
            }

            // "in" keyword
            if (pos >= tokens.size() || !match_word(tokens, pos, "in")) {
                return nullptr;
            }
            if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;

            // Pattern cases: pattern) body ;;
            while (pos < tokens.size()) {
                if (tokens[pos].type == TokenType::WORD && tokens[pos].text == "esac") {
                    pos++;
                    break;
                }

                // Pattern (one or more WORD tokens until ')')
                std::string pattern;
                while (pos < tokens.size() && tokens[pos].type == TokenType::WORD) {
                    if (!pattern.empty()) pattern += ' ';
                    pattern += tokens[pos].text;
                    pos++;
                }

                // Expect ')'
                if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                    pos++;
                } else {
                    break;
                }

                if (pos < tokens.size() && tokens[pos].type == TokenType::NEWLINE) pos++;

                // Body (until ;; or esac)
                auto body = std::make_unique<Node>();
                body->type = NodeType::SEQ;
                while (pos < tokens.size()) {
                    if (tokens[pos].type == TokenType::DSEMI) {
                        pos++;
                        break;
                    }
                    if (tokens[pos].type == TokenType::WORD && tokens[pos].text == "esac") {
                        break;
                    }
                    auto stmt = parse_and_or(tokens, pos);
                    if (stmt) {
                        body->seq_children.push_back(std::move(stmt));
                    } else {
                        // Skip this token
                        if (pos < tokens.size() &&
                            tokens[pos].type == TokenType::NEWLINE) {
                            pos++;
                        } else if (pos < tokens.size()) {
                            pos++;
                        } else {
                            break;
                        }
                        break;
                    }
                }

                node->case_entries.emplace_back(pattern, std::move(body));
            }

            return node;
        }
    }

    // Simple command or pipeline
    return parse_simple_command(tokens, pos);
}

static std::unique_ptr<Node> parse_pipeline_node(const std::vector<Token>& tokens, size_t& pos) {
    auto first = parse_command(tokens, pos);
    if (!first) return nullptr;

    // Handle pipe: a pipeline is a command followed by | command
    if (pos < tokens.size() && tokens[pos].type == TokenType::PIPE) {
        auto pipe_node = std::make_unique<Node>();
        pipe_node->type = NodeType::PIPELINE;

        // Collect command into pipeline
        {
            SimpleCommand sc;
            if (first->type == NodeType::SIMPLE && !first->pipeline.commands.empty()) {
                sc = std::move(first->pipeline.commands[0]);
            } else {
                // For compound commands in a pipeline, execute and pipe
                // For simplicity, create a sub-shell kind of handling
                // We'll handle this by treating the compound as executing
                // and piping its output
                // For now, just collect the command
                sc.args.push_back("(");
            }
            pipe_node->pipeline.commands.push_back(std::move(sc));
        }

        while (pos < tokens.size() && tokens[pos].type == TokenType::PIPE) {
            pos++;
            auto next = parse_command(tokens, pos);
            if (!next) break;
            if (next->type == NodeType::SIMPLE && !next->pipeline.commands.empty()) {
                pipe_node->pipeline.commands.push_back(
                    std::move(next->pipeline.commands[0]));
            } else {
                break;
            }
        }

        if (pipe_node->pipeline.commands.size() >= 2) {
            return pipe_node;
        }
        // Fallback: just return the single command
        return first;
    }

    return first;
}

static std::unique_ptr<Node> parse_and_or(const std::vector<Token>& tokens, size_t& pos) {
    auto node = parse_pipeline_node(tokens, pos);
    if (!node) return nullptr;

    while (pos < tokens.size()) {
        if (tokens[pos].type == TokenType::AND_IF) {
            pos++;
            auto right = parse_pipeline_node(tokens, pos);
            if (!right) break;
            auto and_node = std::make_unique<Node>();
            and_node->type = NodeType::AND_OR;
            and_node->left = std::move(node);
            and_node->right = std::move(right);
            and_node->is_and = true;
            node = std::move(and_node);
        } else if (tokens[pos].type == TokenType::OR_IF) {
            pos++;
            auto right = parse_pipeline_node(tokens, pos);
            if (!right) break;
            auto or_node = std::make_unique<Node>();
            or_node->type = NodeType::AND_OR;
            or_node->left = std::move(node);
            or_node->right = std::move(right);
            or_node->is_and = false;
            node = std::move(or_node);
        } else {
            break;
        }
    }

    return node;
}

static std::unique_ptr<Node> parse_list(const std::vector<Token>& tokens, size_t& pos) {
    auto first = parse_and_or(tokens, pos);
    if (!first) return nullptr;

    // Check for semicolons and newlines
    while (pos < tokens.size() &&
           (tokens[pos].type == TokenType::SEMI || tokens[pos].type == TokenType::NEWLINE)) {
        pos++;
    }

    // Start sequence
    auto seq = std::make_unique<Node>();
    seq->type = NodeType::SEQ;
    seq->seq_children.push_back(std::move(first));

    while (pos < tokens.size()) {
        // Check for keywords that end compound commands
        if (tokens[pos].type == TokenType::WORD) {
            const std::string& w = tokens[pos].text;
            if (w == "fi" || w == "else" || w == "elif" || w == "done" || w == "esac") {
                break;
            }
            if (w == "then" || w == "do") {
                break;
            }
        }

        auto next = parse_and_or(tokens, pos);
        if (!next) {
            // Skip newlines/semicolons
            if (pos < tokens.size() &&
                (tokens[pos].type == TokenType::NEWLINE ||
                 tokens[pos].type == TokenType::SEMI)) {
                pos++;
                continue;
            }
            break;
        }
        seq->seq_children.push_back(std::move(next));

        // Skip separators
        while (pos < tokens.size() &&
               (tokens[pos].type == TokenType::SEMI || tokens[pos].type == TokenType::NEWLINE)) {
            pos++;
        }
    }

    if (seq->seq_children.size() == 1) {
        return std::move(seq->seq_children[0]);
    }
    return seq;
}

static std::unique_ptr<Node> parse_program(const std::vector<Token>& tokens) {
    size_t pos = 0;
    auto result = parse_list(tokens, pos);
    if (!result) {
        result = std::make_unique<Node>();
        result->type = NodeType::SEQ;
    }
    return result;
}

// ── Apply Redirects ────────────────────────────────────────────────────────

static void apply_redirects(const std::vector<Redirect>& redirects) {
    for (const auto& r : redirects) {
        if (r.close_flag && r.target.empty()) {
            close(r.fd);
            continue;
        }

        int new_fd = -1;

        switch (r.type) {
        case Redirect::INPUT:
            new_fd = open(r.target.c_str(), O_RDONLY);
            if (new_fd < 0) {
                fprintf(stderr, "sh: %s: cannot open: %s\n",
                        r.target.c_str(), strerror(errno));
                _exit(1);
            }
            break;
        case Redirect::OUTPUT:
            new_fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (new_fd < 0) {
                fprintf(stderr, "sh: %s: cannot create: %s\n",
                        r.target.c_str(), strerror(errno));
                _exit(1);
            }
            break;
        case Redirect::APPEND:
            new_fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (new_fd < 0) {
                fprintf(stderr, "sh: %s: cannot create: %s\n",
                        r.target.c_str(), strerror(errno));
                _exit(1);
            }
            break;
        case Redirect::FD_INPUT: {
            int target_fd;
            if (r.close_flag) {
                close(r.fd);
                continue;
            }
            if (r.target == "-") {
                close(r.fd);
                continue;
            }
            target_fd = std::stoi(r.target);
            dup2(target_fd, r.fd);
            continue;
        }
        case Redirect::FD_OUTPUT: {
            int target_fd;
            if (r.close_flag) {
                close(r.fd);
                continue;
            }
            if (r.target == "-") {
                close(r.fd);
                continue;
            }
            target_fd = std::stoi(r.target);
            dup2(target_fd, r.fd);
            continue;
        }
        default:
            continue;
        }

        if (new_fd >= 0) {
            if (new_fd != r.fd) {
                dup2(new_fd, r.fd);
                close(new_fd);
            }
        }
    }
}

// ── Built-in Commands ──────────────────────────────────────────────────────

static bool is_builtin(const std::string& name) {
    static const char* builtins[] = {
        "cd", "exit", "export", "echo", "exec", ".", "type", "unset", "read", "set"
    };
    for (auto b : builtins) {
        if (name == b) return true;
    }
    return false;
}

static int run_builtin(const std::string& name, std::vector<std::string>& args) {
    if (name == "cd") {
        std::string target;
        if (args.empty()) {
            auto it = g_state.vars.find("HOME");
            target = (it != g_state.vars.end()) ? it->second : "/";
        } else if (args[0] == "-") {
            auto it = g_state.vars.find("OLDPWD");
            if (it == g_state.vars.end()) {
                fprintf(stderr, "sh: cd: OLDPWD not set\n");
                return 1;
            }
            target = it->second;
            printf("%s\n", target.c_str());
        } else {
            target = args[0];
        }

        char old_cwd[4096];
        if (!getcwd(old_cwd, sizeof(old_cwd))) old_cwd[0] = '\0';

        if (chdir(target.c_str()) != 0) {
            fprintf(stderr, "sh: cd: %s: %s\n", target.c_str(), strerror(errno));
            return 1;
        }

        g_state.vars["OLDPWD"] = old_cwd;
        char new_cwd[4096];
        if (getcwd(new_cwd, sizeof(new_cwd))) {
            g_state.vars["PWD"] = new_cwd;
        }
        return 0;
    }

    if (name == "exit") {
        if (!args.empty()) {
            g_state.last_exit_code = atoi(args[0].c_str());
        }
        g_state.running = false;
        return g_state.last_exit_code;
    }

    if (name == "export") {
        if (args.empty()) {
            for (const auto& v : g_state.vars) {
                printf("export %s=%s\n", v.first.c_str(), v.second.c_str());
            }
            return 0;
        }
        for (const auto& arg : args) {
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                std::string k = arg.substr(0, eq);
                std::string v = arg.substr(eq + 1);
                g_state.vars[k] = v;
                setenv(k.c_str(), v.c_str(), 1);
            } else {
                auto it = g_state.vars.find(arg);
                if (it != g_state.vars.end()) {
                    setenv(arg.c_str(), it->second.c_str(), 1);
                }
            }
        }
        return 0;
    }

    if (name == "echo") {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) printf(" ");
            printf("%s", args[i].c_str());
        }
        printf("\n");
        return 0;
    }

    if (name == "exec") {
        if (args.empty()) return 0;
        std::string fullpath;
        std::vector<const char*> exec_argv;
        exec_argv.push_back(args[0].c_str());
        for (size_t i = 1; i < args.size(); i++) {
            exec_argv.push_back(args[i].c_str());
        }
        exec_argv.push_back(nullptr);

        if (find_in_path(args[0], fullpath)) {
            execv(fullpath.c_str(), const_cast<char* const*>(exec_argv.data()));
        } else {
            execvp(args[0].c_str(), const_cast<char* const*>(exec_argv.data()));
        }
        fprintf(stderr, "sh: exec: %s: %s\n", args[0].c_str(), strerror(errno));
        g_state.running = false;
        return 126;
    }

    if (name == ".") {
        if (args.empty()) return 0;
        const std::string& path = args[0];
        FILE* f = fopen(path.c_str(), "r");
        if (!f) {
            fprintf(stderr, "sh: %s: %s\n", path.c_str(), strerror(errno));
            return 127;
        }

        // Save input state
        auto saved_mode = g_state.input_mode;
        auto saved_file = g_state.input_file;
        auto saved_str = g_state.input_str;
        auto saved_str_pos = g_state.input_str_pos;
        auto saved_str_done = g_state.input_str_done;

        g_state.input_mode = ShellState::INPUT_FILE;
        g_state.input_file = f;

        while (g_state.running) {
            std::string line = read_line_raw();
            if (line.empty() && feof(f)) break;
            g_state.line_number++;
            if (line.empty()) continue;

            // Check for pure comment
            bool all_ws = true;
            for (char ch : line) {
                if (ch != ' ' && ch != '\t') { all_ws = false; break; }
            }
            if (all_ws) continue;

            auto tokens = tokenize(line);
            if (tokens.empty()) continue;

            auto ast = parse_program(tokens);
            if (ast) {
                int ec = execute_node(*ast);
                g_state.last_exit_code = ec;
            }
        }

        fclose(f);

        // Restore input state
        g_state.input_mode = saved_mode;
        g_state.input_file = saved_file;
        g_state.input_str = saved_str;
        g_state.input_str_pos = saved_str_pos;
        g_state.input_str_done = saved_str_done;

        return g_state.last_exit_code;
    }

    if (name == "type") {
        int ret = 0;
        for (const auto& arg : args) {
            if (is_builtin(arg)) {
                printf("%s is a shell builtin\n", arg.c_str());
            } else {
                std::string path;
                if (find_in_path(arg, path)) {
                    printf("%s is %s\n", arg.c_str(), path.c_str());
                } else {
                    fprintf(stderr, "sh: type: %s: not found\n", arg.c_str());
                    ret = 1;
                }
            }
        }
        return ret;
    }

    if (name == "unset") {
        for (const auto& arg : args) {
            g_state.vars.erase(arg);
            unsetenv(arg.c_str());
        }
        return 0;
    }

    if (name == "read") {
        std::string line;
        int c;
        bool raw = false;
        size_t arg_start = 0;

        if (!args.empty() && args[0] == "-r") {
            raw = true;
            arg_start = 1;
        }

        while ((c = getchar()) != EOF && c != '\n') {
            if (!raw && c == '\\') {
                int next = getchar();
                if (next == EOF) break;
                if (next == '\n') continue;
                line += static_cast<char>(c);
                line += static_cast<char>(next);
                continue;
            }
            line += static_cast<char>(c);
        }

        if (arg_start < args.size()) {
            g_state.vars[args[arg_start]] = line;
        } else {
            g_state.vars["REPLY"] = line;
        }
        return (c == EOF && line.empty()) ? 1 : 0;
    }

    if (name == "set") {
        if (args.empty()) {
            for (const auto& v : g_state.vars) {
                printf("%s=%s\n", v.first.c_str(), v.second.c_str());
            }
            return 0;
        }
        for (const auto& arg : args) {
            if (arg.size() >= 2 && arg[0] == '-') {
                for (size_t i = 1; i < arg.size(); i++) {
                    switch (arg[i]) {
                    case 'e': g_state.errexit = true; break;
                    case 'x': g_state.xtrace = true; break;
                    case '+': break; // ignore
                    default: break;
                    }
                }
            } else if (arg.size() >= 2 && arg[0] == '+' && arg[1] == 'e') {
                g_state.errexit = false;
            } else if (arg.size() >= 2 && arg[0] == '+' && arg[1] == 'x') {
                g_state.xtrace = false;
            }
        }
        return 0;
    }

    return 0;
}

// ── External Execution ─────────────────────────────────────────────────────

static void execute_external(const SimpleCommand& cmd) {
    if (cmd.args.empty()) _exit(0);

    std::vector<const char*> argv;
    for (const auto& a : cmd.args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);

    std::string fullpath;
    if (find_in_path(cmd.args[0], fullpath)) {
        execv(fullpath.c_str(), const_cast<char* const*>(argv.data()));
    } else {
        execvp(cmd.args[0].c_str(), const_cast<char* const*>(argv.data()));
    }

    // exec failed
    if (errno == ENOENT) {
        fprintf(stderr, "sh: %s: not found\n", cmd.args[0].c_str());
        _exit(127);
    } else {
        fprintf(stderr, "sh: %s: %s\n", cmd.args[0].c_str(), strerror(errno));
        _exit(126);
    }
}

// ── Pipeline Execution ─────────────────────────────────────────────────────

static int execute_pipeline_internal(const Pipeline& pipeline) {
    if (pipeline.commands.empty()) return 0;

    auto& first_cmd = pipeline.commands[0];

    // Helper: split args into variable assignments and real args
    auto split_args = [](const std::vector<std::string>& args,
                         std::vector<std::pair<std::string, std::string>>& env_assign,
                         std::vector<std::string>& real_args) {
        for (const auto& arg : args) {
            size_t eq = arg.find('=');
            if (eq != std::string::npos && eq > 0) {
                bool valid = true;
                if (!isalpha(arg[0]) && arg[0] != '_') valid = false;
                for (size_t k = 1; k < eq && valid; k++) {
                    if (!isalnum(arg[k]) && arg[k] != '_') valid = false;
                }
                if (valid) {
                    env_assign.push_back({arg.substr(0, eq), arg.substr(eq + 1)});
                    continue;
                }
            }
            real_args.push_back(arg);
        }
    };

    // Single command, not a pipe
    if (pipeline.commands.size() == 1) {
        std::vector<std::pair<std::string, std::string>> env_assign;
        std::vector<std::string> real_args;
        split_args(first_cmd.args, env_assign, real_args);

        // Standalone variable assignments
        if (real_args.empty()) {
            for (const auto& ea : env_assign) {
                g_state.vars[ea.first] = ea.second;
            }
            return 0;
        }

        // Check if it's a builtin
        if (!first_cmd.args.empty() && is_builtin(real_args[0])) {
            // Set env assignments as shell variables
            for (const auto& ea : env_assign) {
                g_state.vars[ea.first] = ea.second;
            }

            if (!first_cmd.redirects.empty()) {
                pid_t pid = fork();
                if (pid == 0) {
                    setbuf(stdout, NULL);
                    setbuf(stderr, NULL);
                    apply_redirects(first_cmd.redirects);
                    if (real_args.size() == 1) { _exit(0); }
                    std::string name = real_args[0];
                    std::vector<std::string> builtin_args(real_args.begin() + 1,
                                                          real_args.end());
                    fflush(stdout); fflush(stderr);
                    _exit(run_builtin(name, builtin_args));
                }
                int status;
                waitpid(pid, &status, 0);
                return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            }

            // Run builtin directly in shell process
            std::string name = real_args[0];
            std::vector<std::string> builtin_args(real_args.begin() + 1, real_args.end());
            return run_builtin(name, builtin_args);
        }

        // External command (no pipe)
        pid_t pid = fork();
        if (pid == 0) {
            // Set temp environment variables
            for (const auto& ea : env_assign) {
                setenv(ea.first.c_str(), ea.second.c_str(), 1);
            }
            if (!first_cmd.redirects.empty()) {
                apply_redirects(first_cmd.redirects);
            }
            SimpleCommand exec_cmd;
            exec_cmd.args = real_args;
            execute_external(exec_cmd);
            _exit(127);
        }
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }

    // Pipeline with multiple commands
    size_t n_cmds = pipeline.commands.size();
    std::vector<std::array<int, 2>> pipes(n_cmds - 1);

    for (size_t i = 0; i < n_cmds - 1; i++) {
        if (::pipe(pipes[i].data()) < 0) {
            perror("sh: pipe");
            return 1;
        }
    }

    std::vector<pid_t> children;

    for (size_t i = 0; i < n_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: unbuffer stdio for pipe correctness
            setbuf(stdout, NULL);
            setbuf(stderr, NULL);

            // Set up stdin from previous pipe
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            // Set up stdout to next pipe
            if (i < n_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipe fds in child
            for (size_t j = 0; j < n_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Apply redirects
            if (!pipeline.commands[i].redirects.empty()) {
                apply_redirects(pipeline.commands[i].redirects);
            }

            // Execute (including builtins - they run in child)
            const auto& cmd = pipeline.commands[i];
            if (!cmd.args.empty() && is_builtin(cmd.args[0])) {
                std::string name = cmd.args[0];
                std::vector<std::string> bargs(cmd.args.begin() + 1, cmd.args.end());
                fflush(stdout);
                fflush(stderr);
                _exit(run_builtin(name, bargs));
            } else if (!cmd.args.empty()) {
                execute_external(cmd);
            }
            fflush(stdout);
            fflush(stderr);
            _exit(0);
        }

        children.push_back(pid);

        // In parent: close pipe fds that are no longer needed
        if (i > 0) {
            // Close the completed pipe between cmd[i-1] and cmd[i]
            close(pipes[i - 1][0]);
            close(pipes[i - 1][1]);
        }
    }

    // Close any remaining pipe fds in parent (safety net)
    // Close all pipe fds in parent
    for (size_t j = 0; j < n_cmds - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    // Wait for all children
    int last_status = 0;
    for (size_t i = 0; i < children.size(); i++) {
        int status;
        waitpid(children[i], &status, 0);
        if (i == children.size() - 1) {
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        }
    }

    return last_status;
}

// ── AST Execution ──────────────────────────────────────────────────────────

static int execute_node(const Node& node) {
    int ec = 0;

    switch (node.type) {
    case NodeType::SIMPLE:
    case NodeType::PIPELINE: {
        // Expand variables in all commands
        Pipeline p = node.pipeline;
        for (auto& cmd : p.commands) {
            expand_simple(cmd);
        }

        // Xtrace
        if (g_state.xtrace) {
            fprintf(stderr, "+");
            for (const auto& cmd : p.commands) {
                for (const auto& a : cmd.args) {
                    fprintf(stderr, " %s", a.c_str());
                }
            }
            fprintf(stderr, "\n");
        }

        ec = execute_pipeline_internal(p);
        break;
    }

    case NodeType::IF: {
        ec = execute_node(*node.condition);
        if (ec == 0) {
            if (node.then_body) ec = execute_node(*node.then_body);
        } else {
            bool handled = false;
            for (const auto& elif : node.elifs) {
                ec = execute_node(*elif.first);
                if (ec == 0) {
                    if (elif.second) ec = execute_node(*elif.second);
                    handled = true;
                    break;
                }
            }
            if (!handled && node.else_body) {
                ec = execute_node(*node.else_body);
            }
        }
        break;
    }

    case NodeType::FOR: {
        if (node.for_words.empty()) {
            // Default to positional parameters
            for (size_t i = 1; i < g_state.pos_args.size(); i++) {
                g_state.vars[node.for_var] = g_state.pos_args[i];
                if (node.for_body) ec = execute_node(*node.for_body);
            }
        } else {
            for (const auto& w : node.for_words) {
                g_state.vars[node.for_var] = expand_string(w, false);
                if (node.for_body) ec = execute_node(*node.for_body);
            }
        }
        break;
    }

    case NodeType::WHILE: {
        while (true) {
            ec = execute_node(*node.loop_cond);
            if (ec != 0) break;
            if (node.loop_body) ec = execute_node(*node.loop_body);
        }
        break;
    }

    case NodeType::UNTIL: {
        while (true) {
            ec = execute_node(*node.loop_cond);
            if (ec == 0) break;
            if (node.loop_body) ec = execute_node(*node.loop_body);
        }
        break;
    }

    case NodeType::CASE: {
        std::string word = expand_string(node.case_word, false);
        for (const auto& entry : node.case_entries) {
            // Simple glob matching: supports * at start/end, ?
            // Walk the pattern and word
            const std::string& pattern = entry.first;
            bool matched = false;

            // Simple glob matching
            size_t pi = 0, wi = 0;
            while (pi < pattern.size() && wi < word.size()) {
                if (pattern[pi] == '*') {
                    // Match remainder
                    if (pi + 1 == pattern.size()) {
                        matched = true;
                        break;
                    }
                    // Try to match rest
                    std::string rest = pattern.substr(pi + 1);
                    size_t found = word.find(rest, wi);
                    if (found != std::string::npos) {
                        matched = true;
                        break;
                    }
                    wi++;
                } else if (pattern[pi] == '?' || pattern[pi] == word[wi]) {
                    pi++;
                    wi++;
                } else {
                    break;
                }
            }
            if (pi == pattern.size() && wi == word.size()) matched = true;
            if (pattern == "*") matched = true;

            if (matched) {
                if (entry.second) ec = execute_node(*entry.second);
                break;
            }
        }
        break;
    }

    case NodeType::SEQ: {
        for (const auto& child : node.seq_children) {
            if (!g_state.running) break;
            ec = execute_node(*child);
            g_state.last_exit_code = ec;
            if (g_state.errexit && ec != 0) break;
        }
        break;
    }

    case NodeType::AND_OR: {
        ec = execute_node(*node.left);
        g_state.last_exit_code = ec;
        if (node.is_and) {
            if (ec == 0 && node.right) {
                ec = execute_node(*node.right);
            }
        } else {
            if (ec != 0 && node.right) {
                ec = execute_node(*node.right);
            }
        }
        break;
    }
    }

    g_state.last_exit_code = ec;
    if (g_state.errexit && ec != 0 && node.type != NodeType::AND_OR) {
        // Don't exit in &&/|| context
    }

    return ec;
}

// ── Read Complete Command (with continuation) ──────────────────────────────

static bool needs_continuation(const std::string& line) {
    // Check if compound constructs are balanced
    int if_depth = 0, for_depth = 0, while_depth = 0, case_depth = 0;

    // Tokenize to check properly
    auto tokens = tokenize(line);
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::WORD) {
            if (tok.text == "if") if_depth++;
            else if (tok.text == "fi") if_depth--;
            else if (tok.text == "for") for_depth++;
            else if (tok.text == "while" || tok.text == "until") while_depth++;
            else if (tok.text == "do") {
                if (for_depth > 0) for_depth--;
                else if (while_depth > 0) while_depth--;
            }
            else if (tok.text == "done") { /* already decremented above */ }
            else if (tok.text == "case") case_depth++;
            else if (tok.text == "esac") case_depth--;
        }
    }

    // Also check for backslash continuation
    if (!line.empty() && line.back() == '\\') return true;

    // Check for unclosed quotes
    bool in_sq = false, in_dq = false;
    for (char c : line) {
        if (c == '\'' && !in_dq) in_sq = !in_sq;
        if (c == '"' && !in_sq) in_dq = !in_dq;
    }

    return if_depth > 0 || for_depth > 0 || while_depth > 0 ||
           case_depth > 0 || in_sq || in_dq;
}

static std::string read_whole_command() {
    std::string full;

    while (true) {
        if (g_state.stdin_tty && g_state.input_mode == ShellState::INPUT_INTERACTIVE) {
            if (full.empty()) {
                fprintf(stderr, "%s", g_state.ps1.c_str());
            } else {
                fprintf(stderr, "%s", g_state.ps2.c_str());
            }
            fflush(stderr);
        }

        std::string line = read_line_raw();
        if (line.empty()) {
            if (full.empty()) return "";
            break;
        }

        // Handle backslash continuation
        if (!line.empty() && line.back() == '\\') {
            line.pop_back();
            full += line + '\n';
            continue;
        }

        full += line;

        if (!needs_continuation(full)) break;
        full += '\n';
    }

    return full;
}

// ── Heredoc Pre-processing ──────────────────────────────────────────────────

static std::vector<std::string> g_temp_files;

static std::string make_temp_file(const std::string& content) {
    char path[] = "/tmp/sh_heredoc_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return "";
    FILE* f = fdopen(fd, "w");
    if (!f) { close(fd); return ""; }
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    g_temp_files.push_back(path);
    return path;
}

static void cleanup_temp_files() {
    for (const auto& p : g_temp_files) {
        unlink(p.c_str());
    }
    g_temp_files.clear();
}

// Pre-process input to extract here-documents before tokenization.
// Replaces <<DELIM with </path/to/tempfile and removes the body lines.
static std::string preprocess_heredocs(const std::string& input) {
    std::string result;
    size_t i = 0;
    size_t len = input.size();
    bool in_sq = false, in_dq = false;

    while (i < len) {
        char c = input[i];

        // Track quotes
        if (c == '\'' && !in_dq) { in_sq = !in_sq; result += c; i++; continue; }
        if (c == '"' && !in_sq) { in_dq = !in_dq; result += c; i++; continue; }
        if (in_sq || in_dq) { result += c; i++; continue; }

        if (c == '<' && i + 1 < len && input[i + 1] == '<') {
            // Found heredoc: <<DELIM
            i += 2;

            // Skip whitespace after <<
            while (i < len && (input[i] == ' ' || input[i] == '\t')) i++;

            // Collect delimiter
            std::string delim;
            bool delim_quoted = false;
            char qchar = 0;

            if (i < len && (input[i] == '\'' || input[i] == '"')) {
                delim_quoted = true;
                qchar = input[i];
                i++;
                while (i < len && input[i] != qchar) {
                    delim += input[i];
                    i++;
                }
                if (i < len) i++; // skip closing quote
            } else {
                while (i < len && input[i] != ' ' && input[i] != '\t' &&
                       input[i] != '\n' && input[i] != '#' &&
                       input[i] != ';' && input[i] != '|') {
                    delim += input[i];
                    i++;
                }
            }

            // Find the delimiter line in the remaining input
            std::string body;
            size_t search_start = i;

            // Skip to end of current line first
            while (i < len && input[i] != '\n') i++;
            if (i < len) i++; // skip newline

            // Read lines until delimiter
            while (i < len) {
                // Check for delimiter at start of line
                size_t line_start = i;
                while (i < len && input[i] != '\n') i++;
                std::string line = input.substr(line_start, i - line_start);

                // Trim trailing whitespace for delimiter comparison
                std::string trimmed = line;
                while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) {
                    trimmed.pop_back();
                }

                if (trimmed == delim) {
                    // Found delimiter, skip the newline
                    if (i < len) i++;
                    break;
                }

                if (!body.empty()) body += '\n';
                body += line;
                // If not doing variable expansion (quoted delimiter), use as-is
                if (i < len) i++; // skip newline
            }

            // Create temp file and replace in output
            std::string temp_path = make_temp_file(body);
            if (!temp_path.empty()) {
                result += "< " + temp_path;
            } else {
                result += "</dev/null";
            }
        } else {
            result += c;
            i++;
        }
    }

    return result;
}

// ── Shell Runners ──────────────────────────────────────────────────────────

static void process_and_execute(const std::string& raw_input) {
    if (raw_input.empty()) return;

    // Check for pure comment or whitespace
    bool all_blank = true;
    for (char c : raw_input) {
        if (c != ' ' && c != '\t' && c != '\n') { all_blank = false; break; }
    }
    if (all_blank) return;

    // Pre-process heredocs
    std::string input = preprocess_heredocs(raw_input);

    auto tokens = tokenize(input);
    if (tokens.empty()) return;

    auto ast = parse_program(tokens);
    if (!ast) return;

    int ec = execute_node(*ast);
    g_state.last_exit_code = ec;
}

static void shell_run_interactive() {
    g_state.input_mode = ShellState::INPUT_INTERACTIVE;
    while (g_state.running) {
        std::string input = read_whole_command();
        if (input.empty()) {
            // EOF
            if (g_state.stdin_tty) printf("\n");
            break;
        }
        g_state.line_number++;
        process_and_execute(input);
    }
}

static void shell_run_string(const char* script) {
    g_state.input_mode = ShellState::INPUT_STRING;
    g_state.input_str = script;
    g_state.input_str_pos = 0;
    g_state.input_str_done = false;

    // For -c mode, process the entire string as a single command
    std::string input = script;
    g_state.line_number++;
    process_and_execute(input);
}

static void shell_run_script(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "sh: %s: cannot open: %s\n", path, strerror(errno));
        g_state.last_exit_code = 127;
        return;
    }

    g_state.input_mode = ShellState::INPUT_FILE;
    g_state.input_file = f;

    while (g_state.running) {
        std::string input = read_whole_command();
        if (input.empty()) break;
        g_state.line_number++;
        process_and_execute(input);
    }

    fclose(f);
    g_state.input_file = nullptr;
}

// ── Entry Point ────────────────────────────────────────────────────────────

void sh_command(int argc, char** argv) {
    shell_init();

    if (argc == 1) {
        // Interactive mode
        shell_run_interactive();
        cleanup_temp_files();
        return;
    }

    // Parse options
    bool c_mode = false;
    const char* script_arg = nullptr;
    bool script_file_mode = false;
    const char* script_path = nullptr;
    std::vector<std::string> extra_args;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            c_mode = true;
            if (i + 1 < argc) {
                script_arg = argv[i + 1];
                i++;
                // Remaining args (if any) become $0, $1, ...
                g_state.pos_args.clear();
                g_state.pos_args.push_back(i + 1 < argc ? argv[i + 1] : "sh");
                for (int j = i + 1; j < argc; j++) {
                    extra_args.push_back(argv[j]);
                }
            } else {
                fprintf(stderr, "sh: -c: option requires an argument\n");
                g_state.last_exit_code = 2;
                return;
            }
        } else if (argv[i][0] != '-') {
            script_file_mode = true;
            script_path = argv[i];
            // Remaining args become $0, $1, ...
            g_state.pos_args.clear();
            g_state.pos_args.push_back(script_path);
            for (int j = i + 1; j < argc; j++) {
                extra_args.push_back(argv[j]);
            }
            break;
        } else if (strcmp(argv[i], "--") == 0) {
            break;
        } else {
            fprintf(stderr, "sh: %s: unknown option\n", argv[i]);
            g_state.last_exit_code = 2;
            return;
        }
    }

    // Set positional parameters
    for (const auto& a : extra_args) {
        g_state.pos_args.push_back(a);
    }

    if (c_mode && script_arg) {
        shell_run_string(script_arg);
    } else if (script_file_mode && script_path) {
        shell_run_script(script_path);
    } else {
        shell_run_interactive();
    }
}

REGISTER_COMMAND("sh", sh_command, "Execute a shell command");
