#include <argtable3.h>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <unistd.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "commands/sed.hpp"

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct SedAddr {
    enum Type { NONE, LINE_NUM, LAST_LINE, REGEX, STEP };
    Type type = NONE;
    int line_num = 0;
    int step = 1;
    std::string regex_str;
    std::regex re;
};

struct SedSubst {
    std::string regex_str;
    std::regex re;
    std::string replacement;
    bool global = false;
    bool print = false;
    std::string write_file;
    bool case_insensitive = false;
    int nth = 0; // 0-based: which occurrence to replace (0=first, etc.)
};

struct SedCmd {
    enum Type {
        CMD_SUBST, CMD_DELETE, CMD_PRINT, CMD_QUIT,
        CMD_APPEND, CMD_INSERT, CMD_CHANGE,
        CMD_LINE_NUM, CMD_TRANSLIT,
        CMD_NEXT, CMD_NEXT_APPEND,
        CMD_WRITE, CMD_READ,
        CMD_NOP
    };
    int type = CMD_NOP;
    SedAddr addr1;
    SedAddr addr2;
    bool has_range = false;
    bool negated = false;

    SedSubst subst;
    std::string text;           // for a/i/c
    std::string y_src, y_dst;   // for y/// (char transliteration)
    std::string file_path;      // for w, r
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string escape_for_regex(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '.' || c == '*' || c == '+' ||
            c == '^' || c == '$' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '(' || c == ')' ||
            c == '|' || c == '?' || c == '/')
            out += '\\';
        out += c;
    }
    return out;
}

// Convert BRE (Basic Regular Expression) to ECMAScript-compatible form
// BRE: \( \) \{ \} for groups/quantifiers, \? \+ \| for ERE features
static std::string bre_to_ecma(const std::string& bre) {
    std::string out;
    bool in_class = false;
    for (size_t i = 0; i < bre.size(); i++) {
        if (bre[i] == '[' && !in_class) {
            in_class = true;
            out += '[';
            continue;
        }
        if (bre[i] == ']' && in_class) {
            in_class = false;
            out += ']';
            continue;
        }
        if (in_class) {
            out += bre[i];
            continue;
        }
        if (bre[i] == '\\' && i + 1 < bre.size()) {
            char n = bre[i + 1];
            if (n == '(' || n == ')' || n == '{' || n == '}') {
                out += n; // strip backslash: \( → (
                i++;
            } else if (n == '?' || n == '+' || n == '|') {
                out += n; // \? → ?, \+ → +, \| → |
                i++;
            } else {
                out += '\\';
                out += n;
                i++;
            }
        } else {
            out += bre[i];
        }
    }
    return out;
}

// Convert sed replacement format to std::regex ECMAScript format
// sed: & → full match, \1-\9 → backrefs, \n → newline
// regex: $& → full match, $1-$9 → backrefs
static std::string convert_replacement(const std::string& s, bool extended) {
    (void)extended;
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            out += "$&";
        } else if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n >= '1' && n <= '9') {
                out += '$';
                out += n;
                i++;
            } else if (n == 'n') {
                out += '\n';
                i++;
            } else if (n == '&') {
                out += '&';
                i++;
            } else {
                out += '\\';
                out += n;
                i++;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Address matching
// ---------------------------------------------------------------------------

static bool addr_matches(const SedAddr& addr, int line_num, int last_line) {
    switch (addr.type) {
    case SedAddr::NONE:
        return true;
    case SedAddr::LINE_NUM:
        return line_num == addr.line_num;
    case SedAddr::LAST_LINE:
        return last_line != 0;
    case SedAddr::REGEX:
        return std::regex_search(std::to_string(line_num), addr.re);
    default:
        return false;
    }
}

// For regex addresses, we need to match against the pattern space
static bool addr_matches_line(const SedAddr& addr, const std::string& line,
                               int line_num, int last_line) {
    switch (addr.type) {
    case SedAddr::NONE:
        return true;
    case SedAddr::LINE_NUM:
        return line_num == addr.line_num;
    case SedAddr::LAST_LINE:
        return last_line != 0;
    case SedAddr::REGEX:
        return std::regex_search(line, addr.re);
    case SedAddr::STEP: {
        if (addr.line_num <= 0) return false;
        if (line_num < addr.line_num) return false;
        if (addr.step <= 0) return false;
        return (line_num - addr.line_num) % addr.step == 0;
    }
    default:
        return false;
    }
}

// Check if a command should be executed for the current line
static bool should_execute(const SedCmd& cmd, const std::string& pattern_space,
                            int line_num, int last_line,
                            bool in_range, bool* out_active_range) {
    bool match = false;
    *out_active_range = false;

    if (cmd.has_range) {
        if (in_range) {
            // We're inside an active range
            match = true;
            // Check if end address matches (end the range)
            if (addr_matches_line(cmd.addr2, pattern_space, line_num, last_line)) {
                *out_active_range = false;
            } else {
                *out_active_range = true;
            }
        } else {
            // Check if start address matches (begin the range)
            if (addr_matches_line(cmd.addr1, pattern_space, line_num, last_line)) {
                match = true;
                // Check if we also match the end address (one-liner range)
                if (addr_matches_line(cmd.addr2, pattern_space, line_num, last_line)) {
                    *out_active_range = false;
                } else {
                    *out_active_range = true;
                }
            }
        }
    } else {
        // Single address or no address
        if (cmd.addr1.type == SedAddr::NONE) {
            match = true;
        } else {
            match = addr_matches_line(cmd.addr1, pattern_space, line_num, last_line);
        }
    }

    if (cmd.negated) {
        match = !match;
    }

    return match;
}

// ---------------------------------------------------------------------------
// Script parsing
// ---------------------------------------------------------------------------

// Read one line from script (handles \ at end of line for continuation)
// Returns true if more data available
static bool read_script_line(const std::string& script, size_t& pos,
                              std::string& line) {
    line.clear();
    if (pos >= script.size()) return false;

    while (pos < script.size()) {
        char c = script[pos];
        if (c == '\n') {
            pos++;
            if (!line.empty() && line.back() == '\\') {
                // Continuation: remove the backslash, keep the newline in text
                line.pop_back();
                line += '\n';
                continue;
            }
            return true;
        }
        if (c == ';') {
            // Only break if we have non-empty content
            // ';' is a command separator
            pos++;
            if (!line.empty()) {
                // Put back the semicolon as a marker? No - just return what we have.
                // The caller will handle it. But we consumed the ; so it won't be
                // seen again. That's correct for most commands, but for a\i\c\,
                // the text ends at newline, not semicolon.
                return true;
            }
            continue;
        }
        line += c;
        pos++;
    }
    return !line.empty();
}

// Get the next "segment" of script, respecting that text commands (a/i/c)
// consume lines differently
static void skip_spaces(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        pos++;
}

static char next_char(const std::string& s, size_t& pos) {
    if (pos < s.size()) return s[pos];
    return '\0';
}

static bool has_more(const std::string& s, size_t pos) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != ';' && c != '\r')
            return true;
        pos++;
    }
    return false;
}

// Skip over a sed address starting at pos, update pos
static void skip_one_addr(const std::string& s, size_t& pos) {
    skip_spaces(s, pos);
    if (pos >= s.size()) return;
    if (s[pos] == '$') {
        pos++;
    } else if (s[pos] == '/' || s[pos] == '\\') {
        char delim = s[pos];
        pos++;
        while (pos < s.size() && s[pos] != delim) {
            if (s[pos] == '\\') { pos += 2; continue; }
            pos++;
        }
        if (pos < s.size()) pos++; // skip closing delimiter
        // Skip optional address flags (I only)
        while (pos < s.size() && s[pos] == 'I') pos++;
    } else if (std::isdigit((unsigned char)s[pos])) {
        char* end = nullptr;
        std::strtol(s.c_str() + pos, &end, 10);
        pos = (size_t)(end - s.c_str());
    } else if (s[pos] == '~') {
        pos++;
    }
}

// Find the actual command character in a sed script, skipping addresses
static char find_cmd_char(const std::string& s, size_t start) {
    size_t pos = start;
    // Try to parse up to 2 addresses
    skip_one_addr(s, pos);
    skip_spaces(s, pos);
    if (pos < s.size() && s[pos] == ',') {
        pos++;
        skip_spaces(s, pos);
        skip_one_addr(s, pos);
        skip_spaces(s, pos);
    }
    // Skip '!' negation
    if (pos < s.size() && s[pos] == '!') pos++;
    skip_spaces(s, pos);
    // Now we should be at the command character
    if (pos < s.size()) return s[pos];
    return 0;
}

// Find end of a command segment (; or newline, but not inside s/// or y///)
static size_t find_cmd_end(const std::string& s, size_t start) {
    size_t i = start;
    // Find the actual command character (skipping addresses)
    char cmd_char = find_cmd_char(s, start);

    // For a, i, c: scan to end of line (do not stop at ;);
    // also skip the command char itself in the scan
    if (cmd_char == 'a' || cmd_char == 'i' || cmd_char == 'c') {
        // Find where the command char is
        size_t tmp = start;
        skip_one_addr(s, tmp);
        skip_spaces(s, tmp);
        if (tmp < s.size() && s[tmp] == ',') { tmp++; skip_spaces(s, tmp); skip_one_addr(s, tmp); skip_spaces(s, tmp); }
        if (tmp < s.size() && s[tmp] == '!') tmp++;
        skip_spaces(s, tmp);
        // Skip the command char itself
        if (tmp < s.size()) tmp++;
        // Skip backslash if present
        if (tmp < s.size() && s[tmp] == '\\') tmp++;
        // Scan to end of line
        while (i < s.size()) {
            if (s[i] == '\n') {
                i++;
                return i;
            }
            i++;
        }
        return i;
    }

    // For s and y: find the closing delimiter
    if (cmd_char == 's' || cmd_char == 'y') {
        // Find the position of the command char (skip addresses)
        size_t cmd_pos = start;
        skip_one_addr(s, cmd_pos);
        skip_spaces(s, cmd_pos);
        if (cmd_pos < s.size() && s[cmd_pos] == ',') {
            cmd_pos++; skip_spaces(s, cmd_pos); skip_one_addr(s, cmd_pos); skip_spaces(s, cmd_pos);
        }
        if (cmd_pos < s.size() && s[cmd_pos] == '!') cmd_pos++;
        skip_spaces(s, cmd_pos);
        // Now cmd_pos should be at the command char
        if (cmd_pos < s.size() && (s[cmd_pos] == 's' || s[cmd_pos] == 'y')) {
            cmd_pos++;
            skip_spaces(s, cmd_pos);
        }
        if (cmd_pos < s.size()) {
            char delim = s[cmd_pos];
            // Skip the delimiter
            i = cmd_pos + 1;
            int delim_count = 0;
            int max_delim = (cmd_char == 's') ? 3 : 2;
            while (i < s.size() && delim_count < max_delim) {
                if (s[i] == '\\') {
                    i += 2; // skip escaped char
                    continue;
                }
                if (s[i] == delim) {
                    delim_count++;
                    if (delim_count < max_delim) {
                        i++;
                    }
                    continue;
                }
                // Newline or semicolon after at least some content ends the command
                if (s[i] == '\n' || s[i] == ';') {
                    break;
                }
                i++;
            }
            // If we found all 3 delimiters, scan flags after them
            if (delim_count >= max_delim) {
                while (i < s.size() && s[i] != '\n') {
                    if (s[i] == ';') {
                        size_t check = cmd_pos;
                        if (check > 0) check -= 1; // start from before cmd char
                        int dc = 0;
                        bool has_w = false;
                        while (check < i && dc < 3) {
                            if (s[check] == '\\') { check += 2; continue; }
                            if (s[check] == delim) { dc++; check++; continue; }
                            if (dc >= 3) {
                                if (s[check] == 'w') { has_w = true; break; }
                            }
                            check++;
                        }
                        if (!has_w) break;
                    }
                    i++;
                }
            }
            if (i < s.size() && s[i] == '\n') i++;
            return i;
        }
    }

    // For other commands: scan to ; or newline
    while (i < s.size()) {
        if (s[i] == ';' || s[i] == '\n') {
            i++;
            return i;
        }
        if (s[i] == '\\') {
            i += 2;
            continue;
        }
        i++;
    }
    return i;
}

// Parse a single sed address from a script string
// Returns the type and advances pos
static SedAddr parse_addr(const std::string& script, size_t& pos,
                           bool extended) {
    SedAddr addr;
    skip_spaces(script, pos);
    if (pos >= script.size()) return addr;

    if (script[pos] == '$') {
        addr.type = SedAddr::LAST_LINE;
        pos++;
    } else if (script[pos] == '/' || script[pos] == '\\') {
        // Regex address: /pattern/ or \XpatternX
        char delim = script[pos];
        pos++;
        std::string pattern;
        bool case_insensitive = false;
        while (pos < script.size()) {
            if (script[pos] == '\\') {
                if (pos + 1 < script.size() && script[pos + 1] == delim) {
                    pattern += delim;
                    pos += 2;
                } else {
                    pattern += script[pos];
                    pos++;
                }
            } else if (script[pos] == delim) {
                pos++;
                // Check for flags (I for case-insensitive)
                while (pos < script.size() && script[pos] == 'I') {
                    case_insensitive = true;
                    pos++;
                }
                break;
            } else {
                pattern += script[pos];
                pos++;
            }
        }
        addr.type = SedAddr::REGEX;
        addr.regex_str = pattern;
        auto flags = std::regex::ECMAScript | std::regex::optimize;
        if (case_insensitive) flags |= std::regex::icase;
        try {
            std::string re_pattern = extended ? pattern : bre_to_ecma(pattern);
            addr.re = std::regex(re_pattern, flags);
        } catch (...) {
            // If regex fails, keep a placeholder
            addr.re = std::regex(".*");
        }
    } else if (std::isdigit(static_cast<unsigned char>(script[pos]))) {
        char* end = nullptr;
        long n = std::strtol(script.c_str() + pos, &end, 10);
        if (end && *end == '~') {
            addr.type = SedAddr::STEP;
            addr.line_num = static_cast<int>(n);
            pos = static_cast<size_t>(end - script.c_str()) + 1;
            if (pos < script.size() && std::isdigit(static_cast<unsigned char>(script[pos]))) {
                char* e2 = nullptr;
                long s = std::strtol(script.c_str() + pos, &e2, 10);
                addr.step = static_cast<int>(s);
                pos = static_cast<size_t>(e2 - script.c_str());
            }
        } else if (end) {
            addr.type = SedAddr::LINE_NUM;
            addr.line_num = static_cast<int>(n);
            pos = static_cast<size_t>(end - script.c_str());
        }
    }
    // Handle step addressing: first~step
    else if (script[pos] == '~') {
        // This is actually after a line number, handled above
        // If we get here alone, it's invalid
        pos++;
    }

    return addr;
}

// Parse a single sed command from a script string at position pos
// Advances pos past the command
static SedCmd parse_cmd(const std::string& script, size_t& pos,
                         bool extended) {
    SedCmd cmd;

    skip_spaces(script, pos);
    if (pos >= script.size()) return cmd;

    // Grab the first segment up to ; or \n for parsing
    size_t seg_end = find_cmd_end(script, pos);
    std::string seg = script.substr(pos, seg_end - pos);
    size_t seg_pos = 0;

    // Trim trailing whitespace but not newlines
    size_t real_end = seg.size();
    while (real_end > 0 && (seg[real_end - 1] == ' ' || seg[real_end - 1] == '\t'))
        real_end--;
    seg = seg.substr(0, real_end);

    // Skip leading whitespace
    skip_spaces(seg, seg_pos);
    if (seg_pos >= seg.size() || seg[seg_pos] == '#' || seg[seg_pos] == '\n') {
        pos = seg_end;
        cmd.type = SedCmd::CMD_NOP;
        return cmd;
    }

    // Save start for address parsing
    size_t saved_pos = seg_pos;

    // ---- Address parsing ----
    // First, try to parse up to 2 addresses followed by a command character
    // Addresses can be: number, $, /regex/, number~step
    // We need to distinguish address from command '1' vs '1d'

    // Read the whole "token" to determine structure
    std::string raw = seg.substr(seg_pos);

    // Parse addresses manually
    cmd.addr1 = parse_addr(seg, seg_pos, extended);

    skip_spaces(seg, seg_pos);

    cmd.has_range = false;
    if (seg_pos < seg.size() && seg[seg_pos] == ',') {
        seg_pos++;
        cmd.has_range = true;
        skip_spaces(seg, seg_pos);
        cmd.addr2 = parse_addr(seg, seg_pos, extended);
        skip_spaces(seg, seg_pos);
    }

    // '!' negation
    cmd.negated = false;
    if (seg_pos < seg.size() && seg[seg_pos] == '!') {
        cmd.negated = true;
        seg_pos++;
        skip_spaces(seg, seg_pos);
    }

    // ---- Command parsing ----
    if (seg_pos >= seg.size()) {
        pos = seg_end;
        cmd.type = SedCmd::CMD_NOP;
        return cmd;
    }

    char c = seg[seg_pos];
    seg_pos++;

    // Skip spaces between command and its arguments
    skip_spaces(seg, seg_pos);

    switch (c) {
    case 's': {
        cmd.type = SedCmd::CMD_SUBST;
        if (seg_pos >= seg.size()) break;

        char delim = seg[seg_pos];
        seg_pos++;

        // Parse regex pattern — preserve escape sequences for std::regex
        std::string pattern;
        while (seg_pos < seg.size()) {
            if (seg[seg_pos] == '\\') {
                pattern += '\\';
                seg_pos++;
                if (seg_pos < seg.size()) {
                    pattern += seg[seg_pos];
                    seg_pos++;
                }
                continue;
            }
            if (seg[seg_pos] == delim) { seg_pos++; break; }
            pattern += seg[seg_pos];
            seg_pos++;
        }

        // Parse replacement — preserve escape sequences for convert_replacement
        std::string repl;
        while (seg_pos < seg.size()) {
            if (seg[seg_pos] == '\\') {
                repl += '\\';
                seg_pos++;
                if (seg_pos < seg.size()) {
                    repl += seg[seg_pos];
                    seg_pos++;
                }
                continue;
            }
            if (seg[seg_pos] == delim) { seg_pos++; break; }
            repl += seg[seg_pos];
            seg_pos++;
        }

        // Parse flags
        while (seg_pos < seg.size() && seg[seg_pos] != ' ' && seg[seg_pos] != '\t') {
            char f = seg[seg_pos];
            if (f == 'g') {
                cmd.subst.global = true;
                seg_pos++;
            } else if (f == 'p') {
                cmd.subst.print = true;
                seg_pos++;
            } else if (f == 'i' || f == 'I') {
                cmd.subst.case_insensitive = true;
                seg_pos++;
            } else if (f == 'w') {
                seg_pos++;
                skip_spaces(seg, seg_pos);
                cmd.subst.write_file = seg.substr(seg_pos);
                // w flag consumes rest of line
                seg_pos = seg.size();
            } else if (std::isdigit(static_cast<unsigned char>(f))) {
                char* e = nullptr;
                long n = std::strtol(seg.c_str() + seg_pos, &e, 10);
                if (n > 0) {
                    cmd.subst.nth = static_cast<int>(n - 1); // 0-based
                    seg_pos = static_cast<size_t>(e - seg.c_str());
                } else {
                    seg_pos++;
                }
            } else {
                seg_pos++;
            }
        }

        cmd.subst.regex_str = pattern;
        auto re_flags = std::regex::ECMAScript | std::regex::optimize;
        if (cmd.subst.case_insensitive) re_flags |= std::regex::icase;
        try {
            std::string subst_pattern = extended ? pattern : bre_to_ecma(pattern);
            cmd.subst.re = std::regex(subst_pattern, re_flags);
        } catch (const std::regex_error& e) {
            fprintf(stderr, "sed: invalid regex '%s': %s\n",
                    pattern.c_str(), e.what());
            cmd.type = SedCmd::CMD_NOP;
        }
        cmd.subst.replacement = convert_replacement(repl, extended);
        break;
    }

    case 'd':
        cmd.type = SedCmd::CMD_DELETE;
        break;

    case 'p':
        cmd.type = SedCmd::CMD_PRINT;
        break;

    case 'q':
        cmd.type = SedCmd::CMD_QUIT;
        break;

    case '=':
        cmd.type = SedCmd::CMD_LINE_NUM;
        break;

    case 'n':
        cmd.type = SedCmd::CMD_NEXT;
        break;

    case 'N':
        cmd.type = SedCmd::CMD_NEXT_APPEND;
        break;

    case 'a':
        cmd.type = SedCmd::CMD_APPEND;
        // Text after 'a\' is everything else
        if (seg_pos < seg.size() && seg[seg_pos] == '\\') seg_pos++;
        if (seg_pos < seg.size() && seg[seg_pos] == '\n') seg_pos++;
        cmd.text = seg.substr(seg_pos);
        break;

    case 'i':
        cmd.type = SedCmd::CMD_INSERT;
        if (seg_pos < seg.size() && seg[seg_pos] == '\\') seg_pos++;
        if (seg_pos < seg.size() && seg[seg_pos] == '\n') seg_pos++;
        cmd.text = seg.substr(seg_pos);
        break;

    case 'c':
        cmd.type = SedCmd::CMD_CHANGE;
        if (seg_pos < seg.size() && seg[seg_pos] == '\\') seg_pos++;
        if (seg_pos < seg.size() && seg[seg_pos] == '\n') seg_pos++;
        cmd.text = seg.substr(seg_pos);
        break;

    case 'y': {
        cmd.type = SedCmd::CMD_TRANSLIT;
        if (seg_pos >= seg.size()) break;
        char delim = seg[seg_pos];
        seg_pos++;

        // Parse src
        while (seg_pos < seg.size()) {
            if (seg[seg_pos] == delim) { seg_pos++; break; }
            cmd.y_src += seg[seg_pos];
            seg_pos++;
        }

        // Parse dst
        while (seg_pos < seg.size()) {
            if (seg[seg_pos] == delim) { seg_pos++; break; }
            cmd.y_dst += seg[seg_pos];
            seg_pos++;
        }
        break;
    }

    case 'w':
        cmd.type = SedCmd::CMD_WRITE;
        cmd.file_path = seg.substr(seg_pos);
        break;

    case 'r':
        cmd.type = SedCmd::CMD_READ;
        cmd.file_path = seg.substr(seg_pos);
        break;

    default:
        // Unknown character, treat as NOP
        cmd.type = SedCmd::CMD_NOP;
        break;
    }

    pos = seg_end;
    return cmd;
}

// Parse entire script into a vector of commands
static std::vector<SedCmd> parse_script(const std::string& script,
                                         bool extended) {
    std::vector<SedCmd> cmds;
    size_t pos = 0;

    while (pos < script.size()) {
        SedCmd cmd = parse_cmd(script, pos, extended);
        if (cmd.type != SedCmd::CMD_NOP) {
            cmds.push_back(cmd);
        } else {
            // Skip whitespace/newlines
            while (pos < script.size() &&
                   (script[pos] == ' ' || script[pos] == '\t' ||
                    script[pos] == '\n' || script[pos] == ';' ||
                    script[pos] == '\r'))
                pos++;
        }
    }

    return cmds;
}

// ---------------------------------------------------------------------------
// Command execution
// ---------------------------------------------------------------------------

struct SedContext {
    const std::vector<SedCmd>* cmds;
    bool suppress_print;
    bool extended;
    std::vector<std::string> append_queue; // for a command (printed after cycle)
    std::vector<std::string> read_queue;   // for r command (printed after cycle)
    std::string change_text;    // for c command
    bool changed = false;       // whether c command triggered
    bool deleted = false;
    bool quit = false;
    int line_num = 0;
    int last_line = 0;
    std::string pattern_space;

    // Tracking active ranges for commands (one flag per command index)
    std::vector<bool> range_active;
};

// Execute y/// (transliterate) on a string
static std::string do_translit(const std::string& input,
                                const std::string& src,
                                const std::string& dst) {
    unsigned char tbl[256];
    for (int i = 0; i < 256; i++)
        tbl[i] = static_cast<unsigned char>(i);

    size_t n = src.size() < dst.size() ? src.size() : dst.size();
    for (size_t i = 0; i < n; i++) {
        tbl[static_cast<unsigned char>(src[i])] = static_cast<unsigned char>(dst[i]);
    }
    // If src is longer than dst, the extra src chars map to dst's last char
    if (src.size() > dst.size() && !dst.empty()) {
        for (size_t i = dst.size(); i < src.size(); i++) {
            tbl[static_cast<unsigned char>(src[i])] = static_cast<unsigned char>(dst.back());
        }
    }

    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input) {
        result += static_cast<char>(tbl[c]);
    }
    return result;
}

static void execute_command(const SedCmd& cmd, SedContext& ctx) {
    switch (cmd.type) {
    case SedCmd::CMD_SUBST: {
        std::string result;
        int count = 0;
        int nth = cmd.subst.nth;
        bool global = cmd.subst.global;

        if (global) {
            result = std::regex_replace(ctx.pattern_space, cmd.subst.re,
                                        cmd.subst.replacement,
                                        std::regex_constants::format_default);
            if (result != ctx.pattern_space) count = 1; // at least one replacement
        } else if (nth > 0) {
            // Replace nth occurrence
            std::string input = ctx.pattern_space;
            std::string output;
            auto words_begin = std::sregex_iterator(input.begin(), input.end(), cmd.subst.re);
            auto words_end = std::sregex_iterator();
            int idx = 0;
            size_t last_end = 0;
            for (auto it = words_begin; it != words_end; ++it, ++idx) {
                output += it->prefix().str();
                if (idx == nth) {
                    output += it->format(cmd.subst.replacement);
                    count = 1;
                } else {
                    output += it->str();
                }
                last_end = it->position() + it->length();
            }
            output += input.substr(last_end);
            result = output;
        } else {
            // Replace first occurrence only
            result = std::regex_replace(ctx.pattern_space, cmd.subst.re,
                                        cmd.subst.replacement,
                                        std::regex_constants::format_first_only);
            if (result != ctx.pattern_space) count = 1;
        }

        if (count > 0) {
            ctx.pattern_space = result;

            if (cmd.subst.print) {
                printf("%s\n", ctx.pattern_space.c_str());
            }

            if (!cmd.subst.write_file.empty()) {
                FILE* wf = fopen(cmd.subst.write_file.c_str(), "a");
                if (wf) {
                    fprintf(wf, "%s\n", ctx.pattern_space.c_str());
                    fclose(wf);
                } else {
                    fprintf(stderr, "sed: couldn't open file '%s': %s\n",
                            cmd.subst.write_file.c_str(), strerror(errno));
                }
            }
        }
        break;
    }

    case SedCmd::CMD_DELETE:
        ctx.deleted = true;
        break;

    case SedCmd::CMD_PRINT:
        printf("%s\n", ctx.pattern_space.c_str());
        break;

    case SedCmd::CMD_QUIT:
        ctx.quit = true;
        // When q is reached, print pattern space if -n not set
        // and this is the selected line, GNU sed does NOT auto-print after q
        break;

    case SedCmd::CMD_APPEND:
        ctx.append_queue.push_back(cmd.text);
        break;

    case SedCmd::CMD_INSERT:
        // Handled directly in main loop
        break;

    case SedCmd::CMD_CHANGE:
        ctx.change_text = cmd.text;
        ctx.changed = true;
        break;

    case SedCmd::CMD_LINE_NUM:
        printf("%d\n", ctx.line_num);
        break;

    case SedCmd::CMD_TRANSLIT:
        ctx.pattern_space = do_translit(ctx.pattern_space, cmd.y_src, cmd.y_dst);
        break;

    case SedCmd::CMD_NEXT: {
        // Print pattern space if not suppressed, then read next line
        if (!ctx.suppress_print) {
            printf("%s\n", ctx.pattern_space.c_str());
        }
        // Signal that we need to read next line — handled in main loop
        ctx.deleted = true; // hack: triggers next-cycle behavior
        // But we need to NOT delete — we need to read next line and continue
        // Let's handle this differently
        break;
    }

    case SedCmd::CMD_NEXT_APPEND: {
        // Append next line to pattern space — handled in main loop
        // Signal that we need to read next line
        // We'll set a flag
        // This is handled in the main processing loop
        break;
    }

    case SedCmd::CMD_WRITE: {
        FILE* wf = fopen(cmd.file_path.c_str(), "a");
        if (wf) {
            fprintf(wf, "%s\n", ctx.pattern_space.c_str());
            fclose(wf);
        } else {
            fprintf(stderr, "sed: couldn't open file '%s': %s\n",
                    cmd.file_path.c_str(), strerror(errno));
        }
        break;
    }

    case SedCmd::CMD_READ: {
        FILE* rf = fopen(cmd.file_path.c_str(), "r");
        if (rf) {
            char buf[4096];
            std::string content;
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), rf)) > 0) {
                content.append(buf, n);
            }
            fclose(rf);
            ctx.read_queue.push_back(content);
        } else {
            fprintf(stderr, "sed: couldn't open file '%s': %s\n",
                    cmd.file_path.c_str(), strerror(errno));
        }
        break;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// File processing
// ---------------------------------------------------------------------------

static void process_file(FILE* fp, const std::string& filename,
                          const std::vector<SedCmd>& cmds,
                          SedOptions& opts) {
    SedContext ctx;
    ctx.cmds = &cmds;
    ctx.suppress_print = (opts.suppress_print != 0);
    ctx.extended = (opts.extended_regex != 0);
    ctx.range_active.resize(cmds.size(), false);
    (void)filename;

    char* line_buf = nullptr;
    size_t line_cap = 0;
    ctx.line_num = 0;

    while (true) {
        ssize_t n = getline(&line_buf, &line_cap, fp);
        if (n < 0) break;

        // Remove trailing newline
        if (n > 0 && line_buf[n - 1] == '\n') {
            line_buf[n - 1] = '\0';
            n--;
        }

        ctx.line_num++;
        ctx.pattern_space = std::string(line_buf, static_cast<size_t>(n));

        // Peek ahead to check if this is the last line (for $ address)
        {
            int peek = fgetc(fp);
            ctx.last_line = (peek == EOF) ? 1 : 0;
            if (peek != EOF) ungetc(peek, fp);
        }

        ctx.deleted = false;
        ctx.quit = false;
        ctx.changed = false;
        ctx.change_text.clear();

        // Execute commands for this line
        for (size_t ci = 0; ci < cmds.size(); ci++) {
            if (ctx.quit) break;

            const SedCmd& cmd = cmds[ci];
            bool active_range = false;
            bool exec = should_execute(cmd, ctx.pattern_space,
                                        ctx.line_num, ctx.last_line,
                                        ctx.range_active[ci], &active_range);
            ctx.range_active[ci] = active_range;

            if (exec) {
                // Handle 'i' command: print text before this line
                if (cmd.type == SedCmd::CMD_INSERT) {
                    // Print the insert text before current line
                    // This is always printed (regardless of -n)
                    printf("%s\n", cmd.text.c_str());
                    continue;
                }

                // Handle 'n' command specially
                if (cmd.type == SedCmd::CMD_NEXT) {
                    // Print current pattern space
                    if (!ctx.suppress_print) {
                        printf("%s\n", ctx.pattern_space.c_str());
                    }
                    // Read next line
                    free(line_buf);
                    line_buf = nullptr;
                    line_cap = 0;
                    n = getline(&line_buf, &line_cap, fp);
                    if (n < 0) {
                        ctx.quit = true;
                        break;
                    }
                    if (n > 0 && line_buf[n - 1] == '\n') {
                        line_buf[n - 1] = '\0';
                        n--;
                    }
                    ctx.pattern_space = std::string(line_buf, static_cast<size_t>(n));
                    ctx.line_num++;
                    {
                        int peek = fgetc(fp);
                        ctx.last_line = (peek == EOF) ? 1 : 0;
                        if (peek != EOF) ungetc(peek, fp);
                    }
                    continue;
                }

                // Handle 'N' command specially
                if (cmd.type == SedCmd::CMD_NEXT_APPEND) {
                    std::string append_buf;
                    {
                        free(line_buf);
                        line_buf = nullptr;
                        line_cap = 0;
                        n = getline(&line_buf, &line_cap, fp);
                        if (n < 0) {
                            ctx.quit = true;
                            break;
                        }
                        if (n > 0 && line_buf[n - 1] == '\n') {
                            line_buf[n - 1] = '\0';
                            n--;
                        }
                        append_buf = std::string(line_buf, static_cast<size_t>(n));
                    }
                    ctx.pattern_space += '\n';
                    ctx.pattern_space += append_buf;
                    {
                        int peek = fgetc(fp);
                        ctx.last_line = (peek == EOF) ? 1 : 0;
                        if (peek != EOF) ungetc(peek, fp);
                    }
                    continue;
                }

                execute_command(cmd, ctx);

                if (ctx.deleted) break;
                if (ctx.quit) break;
            }
        }

        // Handle c command output — it replaces the line
        if (ctx.changed && !ctx.deleted && !ctx.suppress_print) {
            printf("%s\n", ctx.change_text.c_str());
        } else if (!ctx.suppress_print && !ctx.deleted) {
            // Auto-print pattern space
            printf("%s\n", ctx.pattern_space.c_str());
        }
        // Print append queue (for 'a' command — text printed after each cycle)
        for (const auto& t : ctx.append_queue) {
            printf("%s\n", t.c_str());
        }
        ctx.append_queue.clear();

        // Print read queue (for 'r' command — file content printed after each cycle)
        for (const auto& t : ctx.read_queue) {
            printf("%s", t.c_str());
            if (!t.empty() && t.back() != '\n') printf("\n");
        }
        ctx.read_queue.clear();

        if (ctx.quit) break;
    }

    free(line_buf);
}

// ---------------------------------------------------------------------------
// Main command entry
// ---------------------------------------------------------------------------

void sed_command(int argc, char** argv) {
    SedOptions opts;

    // ---- Pre-process -i to handle optional suffix ----
    // GNU sed: -i[SUFFIX] where SUFFIX is optional and can be separate arg
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            // No suffix
            opts.in_place = "";
            // Remove -i from argv for argtable
            for (int j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];
            argc--;
            break;
        }
        if (strncmp(argv[i], "-i", 2) == 0 && strlen(argv[i]) > 2) {
            opts.in_place = argv[i] + 2;
            // Remove the -iSUFFIX from argv
            for (int j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];
            argc--;
            break;
        }
    }

    // ---- argtable parsing ----
    struct arg_lit* suppress_opt =
        arg_lit0("n", "quiet", "suppress automatic printing of pattern space");
    struct arg_lit* suppress2_opt =
        arg_lit0(nullptr, "silent", "suppress automatic printing of pattern space");
    struct arg_str* expr_opt =
        arg_strn("e", "expression", "SCRIPT", 0, 200, "add the script to the commands to be executed");
    struct arg_file* script_file_opt =
        arg_file0("f", "file", "SCRIPT-FILE", "add the contents of script-file to the commands to be executed");
    struct arg_lit* extended_opt =
        arg_lit0("E", "extended-regexp", "extended regular expressions");
    struct arg_lit* extended2_opt =
        arg_lit0("r", "regexp-extended", "extended regular expressions");
    struct arg_lit* separate_opt =
        arg_lit0("s", "separate", "consider files as separate rather than as a single continuous stream");
    struct arg_lit* help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg =
        arg_filen(nullptr, nullptr, "FILE", 0, 200, "file(s) to process");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        suppress_opt, suppress2_opt, expr_opt, script_file_opt,
        extended_opt, extended2_opt, separate_opt,
        help_opt, file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... {script-only-if-no-other-script} [file]...\n", argv[0]);
        printf("Stream editor for filtering and transforming text.\n");
        printf("\n");
        printf("  -n, --quiet, --silent\n");
        printf("                 suppress automatic printing of pattern space\n");
        printf("  -e SCRIPT      add the script to the commands to be executed\n");
        printf("  -f FILE        add the contents of script-file to the commands to be executed\n");
        printf("  -E, -r         extended regular expressions\n");
        printf("  -s             consider files as separate rather than as a single continuous stream\n");
        printf("  -i[SUFFIX]     edit files in place (makes backup if SUFFIX supplied)\n");
        printf("  -h, --help     display this help and exit\n");
        printf("\n");
        printf("Commands (implemented):\n");
        printf("  s/regexp/replacement/[flags]  substitute\n");
        printf("  d                             delete pattern space\n");
        printf("  p                             print pattern space\n");
        printf("  q                             quit\n");
        printf("  a\\text                        append text after line\n");
        printf("  i\\text                        insert text before line\n");
        printf("  c\\text                        replace line with text\n");
        printf("  =                             print line number\n");
        printf("  y/src/dst/                    transliterate characters\n");
        printf("  n                             print pattern space then read next line\n");
        printf("  N                             append next line to pattern space\n");
        printf("  w file                        write pattern space to file\n");
        printf("  r file                        read file content\n");
        printf("\n");
        printf("Addresses:\n");
        printf("  number           line number\n");
        printf("  $                last line\n");
        printf("  /regex/          regular expression\n");
        printf("  addr1,addr2      range of lines\n");
        printf("  !                negate address\n");
        printf("\n");
        printf("Substitution flags:\n");
        printf("  g                global (replace all matches)\n");
        printf("  p                print after substitution\n");
        printf("  w file           write to file\n");
        printf("  i, I             case-insensitive regex\n");
        printf("  N                replace Nth occurrence\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // ---- Parse options ----
    if (suppress_opt->count > 0 || suppress2_opt->count > 0)
        opts.suppress_print = 1;
    if (extended_opt->count > 0 || extended2_opt->count > 0)
        opts.extended_regex = 1;
    if (separate_opt->count > 0)
        opts.separate_files = 1;

    // ---- Collect script ----
    std::string script;

    // Add -e scripts
    for (int i = 0; i < expr_opt->count; i++) {
        if (!script.empty()) script += '\n';
        script += expr_opt->sval[i];
    }

    // Add -f scripts
    for (int i = 0; i < script_file_opt->count; i++) {
        FILE* sf = fopen(script_file_opt->filename[i], "r");
        if (!sf) {
            fprintf(stderr, "sed: can't read %s: %s\n",
                    script_file_opt->filename[i], strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), sf)) > 0) {
            if (!script.empty() && script.back() != '\n') script += '\n';
            script.append(buf, n);
        }
        fclose(sf);
    }

    // If no -e or -f, first positional arg is the script
    if (expr_opt->count == 0 && script_file_opt->count == 0) {
        if (file_arg->count > 0) {
            script = file_arg->filename[0];
            // Shift remaining arguments
            file_arg->count--;
            for (int i = 0; i < file_arg->count; i++) {
                file_arg->filename[i] = file_arg->filename[i + 1];
            }
        }
    }

    if (script.empty()) {
        fprintf(stderr, "sed: no script specified\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // ---- Parse script into commands ----
    std::vector<SedCmd> cmds = parse_script(script, opts.extended_regex != 0);

    if (cmds.empty()) {
        fprintf(stderr, "sed: no valid commands in script\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // ---- Process files ----
    if (file_arg->count == 0) {
        // Read from stdin
        process_file(stdin, "(stdin)", cmds, opts);
    } else if (opts.in_place != nullptr) {
        // In-place editing
        for (int i = 0; i < file_arg->count; i++) {
            const char* fname = file_arg->filename[i];
            FILE* fp = fopen(fname, "r");
            if (!fp) {
                fprintf(stderr, "sed: can't read %s: %s\n", fname, strerror(errno));
                continue;
            }

            // Process to a temp file
            char tmpname[] = "/tmp/sed_XXXXXX";
            int tmpfd = mkstemp(tmpname);
            if (tmpfd < 0) {
                fprintf(stderr, "sed: can't create temp file: %s\n", strerror(errno));
                fclose(fp);
                continue;
            }

            // Redirect stdout to temp file
            fflush(stdout);
            int saved_stdout = dup(STDOUT_FILENO);
            dup2(tmpfd, STDOUT_FILENO);
            close(tmpfd);

            process_file(fp, fname, cmds, opts);

            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            fclose(fp);

            // Handle backup suffix
            if (opts.in_place[0] != '\0') {
                std::string backup = std::string(fname) + opts.in_place;
                rename(fname, backup.c_str());
            }

            // Move temp file to original
            rename(tmpname, fname);
        }
    } else {
        // Normal output — process each file
        for (int i = 0; i < file_arg->count; i++) {
            const char* fname = file_arg->filename[i];
            FILE* fp = fopen(fname, "r");
            if (!fp) {
                fprintf(stderr, "sed: can't read %s: %s\n", fname, strerror(errno));
                continue;
            }
            process_file(fp, fname, cmds, opts);
            fclose(fp);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
