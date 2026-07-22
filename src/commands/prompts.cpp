#include <argtable3.h>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#include "commands/prompts.hpp"
#include "commands/command_macros.hpp"

namespace fs = std::filesystem;

// ── Color / Style System ────────────────────────────────────────────────

enum class Attr { Bold, Dim, Italic, Underline, Blink, Reverse };

static const std::map<std::string, int> NAMED_COLORS = {
    {"black", 0},   {"red", 1},     {"green", 2},  {"yellow", 3},
    {"blue", 4},    {"magenta", 5}, {"cyan", 6},   {"white", 7},
};

struct Style {
    int fg = -1;
    int bg = -1;
    bool fg_bright = false;
    bool bg_bright = false;
    bool fg_256 = false;
    int fg_256_val = 0;
    bool bg_256 = false;
    int bg_256_val = 0;
    bool fg_true = false;
    int fg_r = 0, fg_g = 0, fg_b = 0;
    bool bg_true = false;
    int bg_r = 0, bg_g = 0, bg_b = 0;
    int attrs = 0; // bitmask: 1=bold,2=dim,4=italic,8=underline,16=blink,32=reverse
};

static int attr_bit(Attr a) {
    switch (a) {
    case Attr::Bold: return 1;
    case Attr::Dim: return 2;
    case Attr::Italic: return 4;
    case Attr::Underline: return 8;
    case Attr::Blink: return 16;
    case Attr::Reverse: return 32;
    }
    return 0;
}

static int attr_code(Attr a) {
    switch (a) {
    case Attr::Bold: return 1;
    case Attr::Dim: return 2;
    case Attr::Italic: return 3;
    case Attr::Underline: return 4;
    case Attr::Blink: return 5;
    case Attr::Reverse: return 7;
    }
    return 0;
}

static const std::map<std::string, Attr> ATTR_NAMES = {
    {"bold", Attr::Bold},       {"dim", Attr::Dim},
    {"italic", Attr::Italic},   {"underline", Attr::Underline},
    {"blink", Attr::Blink},     {"reverse", Attr::Reverse},
};

static std::string color_escape(int code) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%dm", code);
    return buf;
}

static std::string color_escape_256(bool fg, int val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;5;%dm", fg ? 38 : 48, val);
    return buf;
}

static std::string color_escape_true(bool fg, int r, int g, int b) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;2;%d;%d;%dm", fg ? 38 : 48, r, g, b);
    return buf;
}

// Parse a hex color like #ff8800
static bool parse_hex(const std::string& s, int& r, int& g, int& b) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (int i = 1; i < 7; i++)
        if (!isxdigit(static_cast<unsigned char>(s[i]))) return false;
    auto hex = [&](int i) -> int {
        char c = s[i];
        if (c >= '0' && c <= '9') return c - '0';
        return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - 'A' + 10;
    };
    r = (hex(1) << 4) | hex(2);
    g = (hex(3) << 4) | hex(4);
    b = (hex(5) << 4) | hex(6);
    return true;
}

// Parse a single color token.
// Returns true on success, fills in the relevant fields of `st`.
static bool parse_color_token(const std::string& token, Style& st, bool fg) {
    if (token == "default") return true; // keep as -1 (default terminal)

    // bright-<color>
    if (token.size() > 7 && token.substr(0, 7) == "bright-") {
        std::string base = token.substr(7);
        auto it = NAMED_COLORS.find(base);
        if (it != NAMED_COLORS.end()) {
            if (fg) { st.fg = it->second; st.fg_bright = true; }
            else    { st.bg = it->second; st.bg_bright = true; }
            return true;
        }
        return false;
    }

    // color-<N> (256-color)
    if (token.size() > 6 && token.substr(0, 6) == "color-") {
        char* end = nullptr;
        long val = strtol(token.c_str() + 6, &end, 10);
        if (*end == '\0' && val >= 0 && val <= 255) {
            if (fg) { st.fg_256 = true; st.fg_256_val = static_cast<int>(val); }
            else    { st.bg_256 = true; st.bg_256_val = static_cast<int>(val); }
            return true;
        }
        return false;
    }

    // gray-<N> maps to 232 + N (0-23)
    if (token.size() > 5 && token.substr(0, 5) == "gray-") {
        char* end = nullptr;
        long val = strtol(token.c_str() + 5, &end, 10);
        if (*end == '\0' && val >= 0 && val <= 23) {
            int code = 232 + static_cast<int>(val);
            if (fg) { st.fg_256 = true; st.fg_256_val = code; }
            else    { st.bg_256 = true; st.bg_256_val = code; }
            return true;
        }
        return false;
    }

    // #hex truecolor
    if (token.size() == 7 && token[0] == '#') {
        int r, g, b;
        if (parse_hex(token, r, g, b)) {
            if (fg) { st.fg_true = true; st.fg_r = r; st.fg_g = g; st.fg_b = b; }
            else    { st.bg_true = true; st.bg_r = r; st.bg_g = g; st.bg_b = b; }
            return true;
        }
        return false;
    }

    // Named color
    auto it = NAMED_COLORS.find(token);
    if (it != NAMED_COLORS.end()) {
        if (fg) { st.fg = it->second; st.fg_bright = false; }
        else    { st.bg = it->second; st.bg_bright = false; }
        return true;
    }

    return false;
}

// Parse a full style string: "<fg>[:<bg>[:<attrs>]]"
// Returns the parsed Style object. On parse failure, returns default Style (no styling).
static Style parse_style(const std::string& spec) {
    Style st;
    if (spec.empty()) return st;

    // Split by ':'
    std::vector<std::string> parts;
    size_t start = 0, pos;
    while ((pos = spec.find(':', start)) != std::string::npos) {
        parts.push_back(spec.substr(start, pos - start));
        start = pos + 1;
    }
    parts.push_back(spec.substr(start));

    auto trim = [](std::string& s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
            s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
    };

    if (parts.size() >= 1 && !parts[0].empty()) {
        trim(parts[0]);
        parse_color_token(parts[0], st, true);
    }
    if (parts.size() >= 2 && !parts[1].empty()) {
        trim(parts[1]);
        parse_color_token(parts[1], st, false);
    }
    if (parts.size() >= 3 && !parts[2].empty()) {
        // attrs are comma-separated
        size_t apos = 0, aend;
        std::string attrs_str = parts[2];
        while ((aend = attrs_str.find(',', apos)) != std::string::npos) {
            std::string tok = attrs_str.substr(apos, aend - apos);
            trim(tok);
            auto it = ATTR_NAMES.find(tok);
            if (it != ATTR_NAMES.end())
                st.attrs |= attr_bit(it->second);
            apos = aend + 1;
        }
        std::string tok = attrs_str.substr(apos);
        trim(tok);
        auto it = ATTR_NAMES.find(tok);
        if (it != ATTR_NAMES.end())
            st.attrs |= attr_bit(it->second);
    }

    return st;
}

// Build the ANSI escape sequence for a given Style.
static std::string style_to_ansi(const Style& st) {
    std::string out;

    // Attributes
    if (st.attrs & attr_bit(Attr::Bold))       out += color_escape(1);
    if (st.attrs & attr_bit(Attr::Dim))        out += color_escape(2);
    if (st.attrs & attr_bit(Attr::Italic))     out += color_escape(3);
    if (st.attrs & attr_bit(Attr::Underline))  out += color_escape(4);
    if (st.attrs & attr_bit(Attr::Blink))      out += color_escape(5);
    if (st.attrs & attr_bit(Attr::Reverse))    out += color_escape(7);

    // Foreground
    if (st.fg_true) {
        out += color_escape_true(true, st.fg_r, st.fg_g, st.fg_b);
    } else if (st.fg_256) {
        out += color_escape_256(true, st.fg_256_val);
    } else if (st.fg >= 0) {
        int code = st.fg_bright ? (90 + st.fg) : (30 + st.fg);
        out += color_escape(code);
    }

    // Background
    if (st.bg_true) {
        out += color_escape_true(false, st.bg_r, st.bg_g, st.bg_b);
    } else if (st.bg_256) {
        out += color_escape_256(false, st.bg_256_val);
    } else if (st.bg >= 0) {
        int code = st.bg_bright ? (100 + st.bg) : (40 + st.bg);
        out += color_escape(code);
    }

    return out;
}

static const char* RESET = "\033[0m";

// Apply a style string to text, returning the ANSI-wrapped string.
static std::string apply_style(const std::string& text, const std::string& style_spec) {
    if (text.empty() || style_spec.empty()) return text;
    Style st = parse_style(style_spec);
    std::string ansi = style_to_ansi(st);
    if (ansi.empty()) return text;
    return ansi + text + RESET;
}

// Get a style from env var or return default.
static std::string get_style(const char* env_name, const char* default_style) {
    const char* val = std::getenv(env_name);
    return (val && val[0] != '\0') ? std::string(val) : std::string(default_style);
}

// Get a Nerd Font icon from env var or return default.
// Users can set PROMPTS_ICON_<MODULE> to customize or empty to hide.
static std::string get_icon(const char* env_name, const char* default_icon) {
    const char* val = std::getenv(env_name);
    if (val && val[0] != '\0') {
        return std::string(val);
    }
    return std::string(default_icon);
}

// ── Module Context ──────────────────────────────────────────────────────

struct PromptsCtx {
    std::string cwd;           // current working directory
    std::string home;          // HOME directory
    int dir_trunc = 3;
    int last_exit_code = 0;
    double cmd_duration = 0;   // in seconds
};

// ── Module: username ────────────────────────────────────────────────────

static std::string module_username(const PromptsCtx& ctx) {
    (void)ctx;
    const char* user = std::getenv("USER");
    if (!user || user[0] == '\0') {
        user = std::getenv("LOGNAME");
        if (!user || user[0] == '\0') return "";
    }

    std::string icon = get_icon("PROMPTS_ICON_USERNAME", "\uf007");
    std::string style = get_style("PROMPTS_STYLE_USERNAME", "green");
    // Root gets a different style
    if (geteuid() == 0) {
        style = get_style("PROMPTS_STYLE_ROOT", "bright-red");
    }

    return apply_style(icon, style) + " " + apply_style(user, style);
}

// ── Module: hostname ────────────────────────────────────────────────────

static std::string module_hostname(const PromptsCtx& ctx) {
    (void)ctx;
    char host[256] = {};
    if (gethostname(host, sizeof(host)) != 0) return "";

    // Only show short hostname (before first dot)
    char* dot = strchr(host, '.');
    if (dot) *dot = '\0';

    std::string icon = get_icon("PROMPTS_ICON_HOSTNAME", "\uf109");
    std::string style = get_style("PROMPTS_STYLE_HOSTNAME", "bright-cyan");
    return apply_style(icon, style) + " " + apply_style(host, style);
}

// ── Module: directory ───────────────────────────────────────────────────

static std::string module_directory(const PromptsCtx& ctx) {
    std::string path = ctx.cwd;
    std::string icon = get_icon("PROMPTS_ICON_DIRECTORY", "\uf07c");
    std::string style = get_style("PROMPTS_STYLE_DIRECTORY", "bright-blue");

    // Replace HOME with ~
    std::string home = ctx.home;
    if (!home.empty() && path.size() >= home.size() && path.substr(0, home.size()) == home) {
        if (path.size() == home.size()) {
            return apply_style("~", style);
        }
        if (path[home.size()] == '/') {
            path = "~" + path.substr(home.size());
        }
    }

    // Smart truncation: keep last N components
    int max_components = ctx.dir_trunc;
    std::string result;

    // Count components
    std::vector<std::string> parts;
    size_t s = 0;
    // Skip leading / or ~/
    if (!path.empty() && path[0] == '/') {
        parts.push_back("/");
        s = 1;
    } else if (!path.empty() && path[0] == '~') {
        parts.push_back("~");
        s = 1;
        if (path.size() > 1 && path[1] == '/') s = 2;
    }

    for (size_t i = s; i < path.size();) {
        if (path[i] == '/') { i++; continue; }
        size_t e = path.find('/', i);
        if (e == std::string::npos) e = path.size();
        parts.push_back(path.substr(i, e - i));
        i = e;
    }

    if (parts.size() <= static_cast<size_t>(max_components + 1)) {
        result = path;
    } else {
        // Truncate: keep the last max_components parts
        bool is_absolute = (!path.empty() && path[0] == '/');
        std::string trunc_indicator = "...";
        result = trunc_indicator;
        for (size_t i = parts.size() - max_components; i < parts.size(); i++) {
            if (parts[i] == "/" || parts[i] == "~") continue;
            result += "/" + parts[i];
        }
        // If the original was absolute and we truncated heavily, prefix with /
        if (is_absolute && path[0] == '/' && result.front() != '/') {
            result = "/" + result;
        }
    }

    return apply_style(icon, style) + " " + apply_style(result, style);
}

// ── Module: git_status ──────────────────────────────────────────────────

// Find the .git path by walking up from start_dir.
// Returns the path to the .git directory (or the resolved gitdir from a worktree file),
// or empty string if not found.
static std::string find_git_dir(const std::string& start_dir) {
    fs::path cur = fs::absolute(start_dir);
    // Limit walk to 10 levels to avoid runaway
    for (int i = 0; i < 10; i++) {
        fs::path git_path = cur / ".git";
        std::error_code ec;
        if (fs::is_directory(git_path, ec)) {
            return git_path.string();
        }
        if (fs::is_regular_file(git_path, ec)) {
            // It's a worktree / submodule pointer
            FILE* f = fopen(git_path.c_str(), "r");
            if (!f) return "";
            char buf[4096] = {};
            if (fgets(buf, sizeof(buf), f)) {
                fclose(f);
                std::string line(buf);
                // Strip trailing whitespace
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
                    line.pop_back();
                }
                if (line.substr(0, 8) == "gitdir: ") {
                    fs::path actual = fs::path(line.substr(8));
                    if (actual.is_relative()) {
                        actual = cur / actual;
                    }
                    std::error_code ec2;
                    return fs::canonical(actual, ec2).string();
                }
            } else {
                fclose(f);
            }
            return "";
        }
        // Check parent
        fs::path parent = cur.parent_path();
        if (parent == cur) break;
        cur = parent;
    }
    return "";
}

// Read git HEAD and return branch name (empty if detached or error).
static std::string read_git_branch(const std::string& git_dir) {
    fs::path head_path = fs::path(git_dir) / "HEAD";
    FILE* f = fopen(head_path.c_str(), "r");
    if (!f) return "";
    char buf[4096] = {};
    std::string branch;
    if (fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        const char* prefix = "ref: refs/heads/";
        if (line.substr(0, strlen(prefix)) == prefix) {
            branch = line.substr(strlen(prefix));
        } else {
            // Detached HEAD
            branch = line.substr(0, 7); // short hash
        }
    }
    fclose(f);
    return branch;
}

// Run `git status --porcelain -b` to get status info.
// Returns a string of status indicators: * for modified, + for staged, ? for untracked, etc.
static std::string get_git_status(const std::string& cwd) {
    std::string cmd = "cd " + cwd + " 2>/dev/null && git status --porcelain -b 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    char buf[4096];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    int status = pclose(pipe);
    if (status != 0) return "";

    // Parse the output
    bool modified = false, staged = false, untracked = false, conflicted = false;
    int ahead = 0, behind = 0;

    std::istringstream stream(output);
    std::string line;
    bool first_line = true;
    while (std::getline(stream, line)) {
        if (first_line) {
            // Branch line: "## main...origin/main [ahead 1, behind 2]" or just "## HEAD (detached)"
            first_line = false;
            // Extract ahead/behind
            auto ab_pos = line.find("ahead ");
            if (ab_pos != std::string::npos) {
                ahead = atoi(line.c_str() + ab_pos + 6);
            }
            auto bb_pos = line.find("behind ");
            if (bb_pos != std::string::npos) {
                behind = atoi(line.c_str() + bb_pos + 7);
            }
            continue;
        }

        if (line.size() < 3) continue;
        char x = line[0];
        char y = line[1];

        if (x == '?' && y == '?') {
            untracked = true;
        } else if (x != ' ' && x != '!' && x != '?') {
            staged = true;
            if (x == 'U' || y == 'U') conflicted = true;
        }
        if (y != ' ') {
            modified = true;
            if (x == 'U' || y == 'U') conflicted = true;
        }
    }

    // Build status string
    std::string indicators;
    if (conflicted)       indicators += "\342\234\227"; // ✗
    else if (staged)      indicators += "+";
    else if (modified)    indicators += "*";
    else if (untracked)   indicators += "?";
    // ahead/behind arrows
    if (ahead > 0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "\342\206\221%d", ahead); // ↑N
        indicators += tmp;
    }
    if (behind > 0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "\342\206\223%d", behind); // ↓N
        indicators += tmp;
    }

    return indicators;
}

static std::string module_git_status(const PromptsCtx& ctx) {
    std::string git_dir = find_git_dir(ctx.cwd);
    if (git_dir.empty()) return "";

    std::string branch = read_git_branch(git_dir);
    if (branch.empty()) return "";

    std::string indicators = get_git_status(ctx.cwd);

    std::string icon = get_icon("PROMPTS_ICON_GIT", "\ue0a0");
    std::string text = branch;
    if (!indicators.empty()) {
        text += " " + indicators;
    }

    std::string style = get_style("PROMPTS_STYLE_GIT", "yellow");
    return apply_style(icon, style) + " " + apply_style(text, style);
}

// ── Module: cmd_duration ────────────────────────────────────────────────

static std::string format_duration(double secs) {
    if (secs < 0.001) return "";

    char buf[32];
    if (secs >= 3600) {
        int h = static_cast<int>(secs / 3600);
        int m = static_cast<int>((secs - h * 3600) / 60);
        int s = static_cast<int>(secs) % 60;
        snprintf(buf, sizeof(buf), "%dh%dm%ds", h, m, s);
    } else if (secs >= 60) {
        int m = static_cast<int>(secs / 60);
        int s = static_cast<int>(secs) % 60;
        snprintf(buf, sizeof(buf), "%dm%ds", m, s);
    } else if (secs >= 1.0) {
        snprintf(buf, sizeof(buf), "%.1fs", secs);
    } else if (secs >= 0.01) {
        snprintf(buf, sizeof(buf), "%dms", static_cast<int>(secs * 1000));
    } else {
        snprintf(buf, sizeof(buf), "<1ms");
    }

    return buf;
}

static std::string module_cmd_duration(const PromptsCtx& ctx) {
    if (ctx.cmd_duration <= 0) return "";

    // Only show if >= 2 seconds by default (configurable via threshold env var)
    const char* threshold_env = std::getenv("PROMPTS_DURATION_THRESHOLD");
    double threshold = 2.0;
    if (threshold_env) {
        char* end = nullptr;
        double val = strtod(threshold_env, &end);
        if (end != threshold_env && val >= 0) threshold = val;
    }

    if (ctx.cmd_duration < threshold) return "";

    std::string text = format_duration(ctx.cmd_duration);
    if (text.empty()) return "";

    std::string icon = get_icon("PROMPTS_ICON_CMD_DURATION", "\uf017");
    std::string style = get_style("PROMPTS_STYLE_CMD_DURATION", "bright-yellow");
    return apply_style(icon, style) + " " + apply_style(text, style);
}

// ── Module: exit_code ───────────────────────────────────────────────────

static std::string module_exit_code(const PromptsCtx& ctx) {
    if (ctx.last_exit_code == 0) return "";

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", ctx.last_exit_code);
    std::string text = buf;

    std::string icon = get_icon("PROMPTS_ICON_EXIT_CODE", "\uf071");
    std::string style = get_style("PROMPTS_STYLE_EXIT_CODE", "red");
    return apply_style(icon, style) + " " + apply_style(text, style);
}

// ── Module: line_break ──────────────────────────────────────────────────

static std::string module_line_break(const PromptsCtx& ctx) {
    (void)ctx;
    return "\n";
}

// ── Module: shell_char ──────────────────────────────────────────────────

static std::string module_shell_char(const PromptsCtx& ctx) {
    const char* ch = (geteuid() == 0) ? "#" : "$";

    std::string icon = get_icon("PROMPTS_ICON_SHELL_CHAR", "\uf054");
    std::string style;
    if (ctx.last_exit_code == 0) {
        style = get_style("PROMPTS_STYLE_SHELL_CHAR", "green");
    } else {
        style = get_style("PROMPTS_STYLE_SHELL_CHAR_ERROR", "red");
    }

    return apply_style(icon, style) + " " + apply_style(ch, style) + " ";
}

// ── Module Info ─────────────────────────────────────────────────────────

struct ModuleInfo {
    const char* name;
    const char* description;
    const char* default_style;
    const char* env_vars;
    const char* default_icon;
    const char* icon_env;
};

static const ModuleInfo MODULES[] = {
    {"username",     "Current username",              "green",       "PROMPTS_STYLE_USERNAME",                    "\uf007", "PROMPTS_ICON_USERNAME"},
    {"hostname",     "Machine hostname",              "bright-cyan", "PROMPTS_STYLE_HOSTNAME",                    "\uf109", "PROMPTS_ICON_HOSTNAME"},
    {"directory",    "Current directory (truncated)", "bright-blue", "PROMPTS_STYLE_DIRECTORY, PROMPTS_DIR_TRUNC", "\uf07c", "PROMPTS_ICON_DIRECTORY"},
    {"git_status",   "Git branch and status",         "yellow",      "PROMPTS_STYLE_GIT",                         "\ue0a0", "PROMPTS_ICON_GIT"},
    {"cmd_duration", "Last command duration",         "bright-yellow", "PROMPTS_STYLE_CMD_DURATION, PROMPTS_DURATION_THRESHOLD", "\uf017", "PROMPTS_ICON_CMD_DURATION"},
    {"exit_code",    "Last exit code (if non-zero)",  "red",         "PROMPTS_STYLE_EXIT_CODE",                   "\uf071", "PROMPTS_ICON_EXIT_CODE"},
    {"line_break",   "Newline between sections",      "",            "",                                          "",      ""},
    {"shell_char",   "Prompt character ($ or #)",     "green/red",   "PROMPTS_STYLE_SHELL_CHAR, PROMPTS_STYLE_SHELL_CHAR_ERROR", "\uf054", "PROMPTS_ICON_SHELL_CHAR"},
};

// ── Render ──────────────────────────────────────────────────────────────

// Default module order
static const char* DEFAULT_MODULES[] = {
    "username", "hostname", "directory", "git_status",
    "cmd_duration", "exit_code", "line_break", "shell_char",
    nullptr
};

static std::vector<std::string> get_enabled_modules() {
    const char* env = std::getenv("PROMPTS_ENABLED");
    if (!env || env[0] == '\0') {
        // Return default order
        std::vector<std::string> result;
        for (int i = 0; DEFAULT_MODULES[i] != nullptr; i++)
            result.emplace_back(DEFAULT_MODULES[i]);
        return result;
    }

    std::vector<std::string> result;
    std::string s(env);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        if (comma == std::string::npos) comma = s.size();
        std::string tok = s.substr(pos, comma - pos);
        // Trim
        while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
            tok.erase(tok.begin());
        while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t'))
            tok.pop_back();
        if (!tok.empty()) result.push_back(tok);
        pos = comma + 1;
    }
    return result;
}

using ModuleFn = std::string (*)(const PromptsCtx&);

static ModuleFn find_module(const std::string& name) {
    static const std::map<std::string, ModuleFn, std::less<>> MODULE_MAP = {
        {"username", module_username},
        {"hostname", module_hostname},
        {"directory", module_directory},
        {"git_status", module_git_status},
        {"cmd_duration", module_cmd_duration},
        {"exit_code", module_exit_code},
        {"line_break", module_line_break},
        {"shell_char", module_shell_char},
    };
    auto it = MODULE_MAP.find(name);
    if (it != MODULE_MAP.end()) return it->second;
    return nullptr;
}

// Build full prompt by iterating modules.
static std::string render_prompt(const PromptsCtx& ctx) {
    auto modules = get_enabled_modules();
    std::string result;

    for (const auto& name : modules) {
        ModuleFn fn = find_module(name);
        if (!fn) continue;

        std::string segment = fn(ctx);

        if (segment.empty()) continue;

        // Add separator between segments (but not before line_break)
        if (!result.empty() && name != "line_break") {
            // Check if the last char of result is a newline (i.e., we just did a line break)
            if (!result.empty() && result.back() != '\n') {
                // Add a dim separator
                std::string sep_style = get_style("PROMPTS_STYLE_SEPARATOR", "bright-black");
                result += apply_style(" ", sep_style);
            }
        }

        result += segment;
    }

    return result;
}

// ── Subcommand: render ──────────────────────────────────────────────────

static void cmd_render(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);
    void* argtable[] = {help_opt, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s render [OPTIONS]\n", argv[0]);
        printf("Render the prompt string.\n");
        printf("Called by shell integration to generate the prompt.\n");
        printf("\n");
        printf("  -h, --help  display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    // Build context from environment
    PromptsCtx ctx;

    // CWD
    char cwd_buf[4096];
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        ctx.cwd = cwd_buf;
    } else {
        ctx.cwd = "?";
    }

    // HOME
    const char* home = std::getenv("HOME");
    ctx.home = home ? home : "";

    // Dir truncation
    const char* trunc_env = std::getenv("PROMPTS_DIR_TRUNC");
    if (trunc_env) {
        char* end = nullptr;
        long val = strtol(trunc_env, &end, 10);
        if (end != trunc_env && val > 0) ctx.dir_trunc = static_cast<int>(val);
    }

    // Exit code from environment (set by shell integration)
    const char* ec_env = std::getenv("PROMPTS_EXIT_CODE");
    if (ec_env) {
        ctx.last_exit_code = atoi(ec_env);
    }

    // Command duration from environment (set by shell integration)
    const char* dur_env = std::getenv("PROMPTS_CMD_DURATION");
    if (dur_env) {
        char* end = nullptr;
        double val = strtod(dur_env, &end);
        if (end != dur_env && val >= 0) ctx.cmd_duration = val;
    }

    std::string prompt = render_prompt(ctx);
    printf("%s", prompt.c_str());
}

// ── Subcommand: init ────────────────────────────────────────────────────

static void cmd_init(const char* shell) {
    if (!shell || shell[0] == '\0') {
        fprintf(stderr, "prompts: please specify a shell: bash, zsh, fish\n");
        return;
    }

    std::string s(shell);

    if (s == "bash") {
        printf(R"SH(__prompts_preexec() {
    __PROMPTS_START=${__PROMPTS_START:-${EPOCHREALTIME:-$(date +%%s.%%N)}}
}
__prompts_callback() {
    local __rc=$?
    export PROMPTS_EXIT_CODE=$__rc
    if [ -n "${__PROMPTS_START:-}" ]; then
        local __now=${EPOCHREALTIME:-$(date +%%s.%%N)}
        PROMPTS_CMD_DURATION=$(echo "$__now - $__PROMPTS_START" | bc 2>/dev/null || echo 0)
        export PROMPTS_CMD_DURATION
    fi
    PS1="$(command modbox prompts render)"
}
if [ -z "${BASH_VERSION:-}" ]; then
    return
fi
if [[ ";${PROMPT_COMMAND[*]:-};" != *";__prompts_callback;"* ]]; then
    if [ "${BASH_VERSINFO[0]:-0}" -ge 5 ]; then
        PROMPT_COMMAND+=(__prompts_callback)
    else
        PROMPT_COMMAND="__prompts_callback;${PROMPT_COMMAND:-}"
    fi
fi
trap '__prompts_preexec' DEBUG
)SH");
    } else if (s == "zsh") {
        printf(R"SH(__prompts_preexec() {
    __PROMPTS_START=${EPOCHREALTIME:-$(date +%%s.%%N)}
}
__prompts_callback() {
    export PROMPTS_EXIT_CODE=$?
    if [ -n "${__PROMPTS_START:-}" ]; then
        local __now=${EPOCHREALTIME:-$(date +%%s.%%N)}
        PROMPTS_CMD_DURATION=$(echo "$__now - $__PROMPTS_START" | bc 2>/dev/null || echo 0)
        export PROMPTS_CMD_DURATION
    fi
    PROMPT="$(command modbox prompts render)"
}
typeset -ag precmd_functions
if (( ! ${precmd_functions[(I)__prompts_callback]} )); then
    precmd_functions+=(__prompts_callback)
fi
typeset -ag preexec_functions
if (( ! ${preexec_functions[(I)__prompts_preexec]} )); then
    preexec_functions+=(__prompts_preexec)
fi
)SH");
    } else if (s == "fish") {
        printf(R"SH(function __prompts_preexec --on-event fish_preexec
    set -g __PROMPTS_START (date +%%s.%%N)
end
function __prompts_callback --on-event fish_prompt
    set -g PROMPTS_EXIT_CODE $status
    if set -q __PROMPTS_START
        set -g PROMPTS_CMD_DURATION (math (date +%%s.%%N) - $__PROMPTS_START 2>/dev/null; or echo 0)
    end
    command modbox prompts render
end
function fish_prompt
    __prompts_callback
end
)SH");
    } else {
        fprintf(stderr, "prompts: unsupported shell '%s'. Supported: bash, zsh, fish\n", shell);
    }
}

// ── Subcommand: list ────────────────────────────────────────────────────

static void cmd_list() {
    printf("Available modules for 'prompts':\n\n");
    for (const auto& m : MODULES) {
        printf("  %-16s %s\n", m.name, m.description);
        printf("  %-16s style: %s\n", "", m.default_style);
        if (m.icon_env[0] != '\0') {
            printf("  %-16s icon:  %s (env: %s)\n", "", m.default_icon, m.icon_env);
        }
        if (m.env_vars[0] != '\0') {
            printf("  %-16s env:   %s\n", "", m.env_vars);
        }
        printf("\n");
    }
    printf("Configuration via environment variables:\n");
    printf("  PROMPTS_ENABLED              Comma-separated list of modules to show (default: all)\n");
    printf("  PROMPTS_DIR_TRUNC            Number of directory components to keep (default: 3)\n");
    printf("  PROMPTS_DURATION_THRESHOLD   Min duration in seconds to show (default: 2.0)\n");
    printf("  PROMPTS_ICON_<MODULE>        Nerd Font icon for a module (set empty to hide)\n");
    printf("  PROMPTS_STYLE_<MODULE>       Style string for a module: <fg>[:<bg>[:<attrs>]]\n");
    printf("  PROMPTS_STYLE_SEPARATOR      Style for the separator between modules\n");
    printf("\n");
    printf("Usage: eval \"$(modbox prompts init bash)\"   # bash\n");
    printf("       eval \"$(modbox prompts init zsh)\"    # zsh\n");
    printf("       modbox prompts init fish | source     # fish\n");
}

// ── Subcommand: modules ─────────────────────────────────────────────────

static void cmd_modules(const char* name) {
    if (name && name[0] != '\0') {
        for (const auto& m : MODULES) {
            if (strcmp(m.name, name) == 0) {
                printf("Module: %s\n", m.name);
                printf("  Description: %s\n", m.description);
                printf("  Default style: %s\n", m.default_style);
                if (m.icon_env[0] != '\0') {
                    printf("  Icon: %s (env: %s)\n", m.default_icon, m.icon_env);
                }
                if (m.env_vars[0] != '\0') {
                    printf("  Environment: %s\n", m.env_vars);
                }
                return;
            }
        }
        fprintf(stderr, "prompts: unknown module '%s'\n", name);
        fprintf(stderr, "Run 'prompts list' to see available modules.\n");
    } else {
        printf("Available modules:\n");
        for (const auto& m : MODULES) {
            printf("  %-16s %s\n", m.name, m.description);
        }
    }
}

// ── Main Command Entry Point ────────────────────────────────────────────

void prompts_command(int argc, char** argv) {
    if (argc < 2) {
        // Default: render
        cmd_render(argc, argv);
        return;
    }

    std::string subcmd = argv[1];

    if (subcmd == "--help" || subcmd == "-h") {
        printf("Usage: %s [SUBCOMMAND] [OPTIONS]\n", argv[0]);
        printf("Modern shell prompt renderer.\n");
        printf("\n");
        printf("Subcommands:\n");
        printf("  render             Render and output the prompt string (default)\n");
        printf("  init <SHELL>       Print shell integration code (bash, zsh, fish)\n");
        printf("  list               List all modules and configuration options\n");
        printf("  modules [NAME]     Show info about a module\n");
        printf("\n");
        printf("Shell integration:\n");
        printf("  eval \"$(modbox prompts init bash)\"   # bash\n");
        printf("  eval \"$(modbox prompts init zsh)\"    # zsh\n");
        printf("  modbox prompts init fish | source     # fish\n");
        return;
    }

    if (subcmd == "render") {
        cmd_render(argc - 1, argv + 1);
    } else if (subcmd == "init") {
        const char* shell = (argc >= 3) ? argv[2] : "";
        cmd_init(shell);
    } else if (subcmd == "list") {
        cmd_list();
    } else if (subcmd == "modules") {
        const char* name = (argc >= 3) ? argv[2] : nullptr;
        cmd_modules(name);
    } else {
        fprintf(stderr, "prompts: unknown subcommand '%s'\n", subcmd.c_str());
        fprintf(stderr, "Run 'prompts --help' for usage.\n");
    }
}

REGISTER_COMMAND("prompts", prompts_command, "Render shell prompt");
