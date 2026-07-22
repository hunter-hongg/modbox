#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "commands/zoxide.hpp"
#include "commands/command_macros.hpp"

namespace fs = std::filesystem;

static std::string get_db_path() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg) + "/zoxide/db";
    }
    const char* home = std::getenv("HOME");
    if (!home) home = "/root";
    return std::string(home) + "/.local/share/zoxide/db";
}

static std::vector<ZoxideEntry> load_db(const std::string& path) {
    std::vector<ZoxideEntry> entries;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return entries;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        size_t p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        size_t p2 = line.find('|', p1 + 1);
        if (p2 == std::string::npos) continue;

        ZoxideEntry e;
        try {
            e.rank = std::stod(line.substr(0, p1));
            e.timestamp = std::stoll(line.substr(p1 + 1, p2 - p1 - 1));
            e.path = line.substr(p2 + 1);
        } catch (...) {
            continue;
        }
        if (!e.path.empty()) {
            entries.push_back(std::move(e));
        }
    }
    return entries;
}

static void save_db(const std::string& path, const std::vector<ZoxideEntry>& entries) {
    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        fprintf(stderr, "zoxide: cannot write database '%s': %s\n", path.c_str(), strerror(errno));
        return;
    }
    for (const auto& e : entries) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6f", e.rank);
        ofs << buf << '|' << e.timestamp << '|' << e.path << '\n';
    }
}

static double compute_frecency(double rank, int64_t last_access) {
    int64_t now = static_cast<int64_t>(time(nullptr));
    int64_t age = now - last_access;
    if (age < 0) age = 0;

    double factor;
    if (age < 3600)        factor = 4.0;
    else if (age < 86400)  factor = 2.0;
    else if (age < 604800) factor = 1.0;
    else if (age < 2592000) factor = 0.5;
    else                   factor = 0.25;

    return rank * factor;
}

static bool matches_keywords(const std::string& path, const std::vector<std::string>& keywords) {
    for (const auto& kw : keywords) {
        std::string lower_kw = kw;
        std::transform(lower_kw.begin(), lower_kw.end(), lower_kw.begin(), ::tolower);

        bool found = false;
        fs::path p(path);
        for (const auto& component : p) {
            std::string comp = component.string();
            std::transform(comp.begin(), comp.end(), comp.begin(), ::tolower);
            if (comp.find(lower_kw) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

static void cmd_add(const std::string& db_path, const std::string& dir) {
    fs::path resolved;
    std::error_code ec;
    if (fs::path(dir).is_absolute()) {
        resolved = fs::canonical(dir, ec);
    } else {
        resolved = fs::canonical(fs::current_path() / dir, ec);
    }
    if (ec) {
        fprintf(stderr, "zoxide: cannot resolve path '%s': %s\n", dir.c_str(), ec.message().c_str());
        return;
    }

    std::string path_str = resolved.string();
    auto entries = load_db(db_path);

    int64_t now = static_cast<int64_t>(time(nullptr));
    bool found = false;
    for (auto& e : entries) {
        if (e.path == path_str) {
            e.rank += 1.0;
            e.timestamp = now;
            found = true;
            break;
        }
    }
    if (!found) {
        entries.push_back({path_str, 1.0, now});
    }

    save_db(db_path, entries);
}

static void cmd_remove(const std::string& db_path, const std::string& dir) {
    fs::path resolved;
    std::error_code ec;
    if (fs::path(dir).is_absolute()) {
        resolved = fs::canonical(dir, ec);
    } else {
        resolved = fs::canonical(fs::current_path() / dir, ec);
    }
    if (ec) {
        fprintf(stderr, "zoxide: cannot resolve path '%s': %s\n", dir.c_str(), ec.message().c_str());
        return;
    }

    std::string path_str = resolved.string();
    auto entries = load_db(db_path);

    size_t orig_size = entries.size();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                        [&](const ZoxideEntry& e) { return e.path == path_str; }),
        entries.end());

    if (entries.size() == orig_size) {
        fprintf(stderr, "zoxide: directory '%s' not found in database\n", path_str.c_str());
        return;
    }
    save_db(db_path, entries);
}

static void cmd_edit(const std::string& db_path) {
    const char* editor = std::getenv("EDITOR");
    if (!editor || editor[0] == '\0') editor = "vi";

    std::string cmd = std::string(editor) + " " + db_path;
    int ret = system(cmd.c_str());
    if (ret != 0) {
        fprintf(stderr, "zoxide: editor exited with status %d\n", ret);
    }
}

static void cmd_list(const std::string& db_path) {
    auto entries = load_db(db_path);

    std::sort(entries.begin(), entries.end(), [](const ZoxideEntry& a, const ZoxideEntry& b) {
        return compute_frecency(a.rank, a.timestamp) > compute_frecency(b.rank, b.timestamp);
    });

    for (const auto& e : entries) {
        double score = compute_frecency(e.rank, e.timestamp);
        printf("%-8.1f %s\n", score, e.path.c_str());
    }
}

static std::string cmd_query(const std::string& db_path, const std::vector<std::string>& keywords,
                              const std::string& exclude) {
    auto entries = load_db(db_path);

    struct Scored {
        double score;
        std::string path;
    };
    std::vector<Scored> matches;

    for (const auto& e : entries) {
        if (!exclude.empty() && e.path == exclude) continue;
        if (!matches_keywords(e.path, keywords)) continue;
        double score = compute_frecency(e.rank, e.timestamp);
        matches.push_back({score, e.path});
    }

    if (matches.empty()) {
        fprintf(stderr, "zoxide: no match found\n");
        return "";
    }

    std::sort(matches.begin(), matches.end(), [](const Scored& a, const Scored& b) {
        return a.score > b.score;
    });

    return matches[0].path;
}

static void cmd_init(const char* shell) {
    if (!shell || shell[0] == '\0') {
        fprintf(stderr, "zoxide: please specify a shell: bash, zsh, fish, nushell, posix\n");
        return;
    }

    std::string s(shell);

    if (s == "bash") {
        printf(R"SHELL(__zoxide_hook() {
    local ret=$?
    local dir
    dir=$(command pwd -L)
    command modbox zoxide add "$dir" 2>/dev/null
    return $ret
}
__zoxide_cd() {
    \builtin cd "$@" || return
    local dir
    dir=$(command pwd -L)
    command modbox zoxide add "$dir" 2>/dev/null
}
z() {
    if [ $# -eq 0 ]; then
        \builtin cd ~ || return
    elif [ $# -eq 1 ] && [ "$1" = "-" ]; then
        \builtin cd - || return
    elif [ $# -eq 1 ] && [ -d "$1" ]; then
        \builtin cd "$1" || return
    else
        local result
        result=$(command modbox zoxide query --exclude "$(pwd)" -- "$@") || return
        \builtin cd "$result" || return
    fi
}
zi() {
    local result
    result=$(command modbox zoxide query --interactive -- "$@") || return
    \builtin cd "$result" || return
}
if [[ ";${PROMPT_COMMAND[*]:-};" != *";__zoxide_hook;"* ]]; then
    if [ -n "${BASH_VERSION:-}" ] && [ "${BASH_VERSINFO[0]:-0}" -ge 5 ]; then
        PROMPT_COMMAND+=(__zoxide_hook)
    else
        PROMPT_COMMAND="__zoxide_hook;${PROMPT_COMMAND:-}"
    fi
fi
alias __zoxide_z=z
alias __zoxide_zi=zi
)SHELL");
    } else if (s == "zsh") {
        printf(R"SHELL(__zoxide_hook() {
    local dir
    dir=${PWD}
    command modbox zoxide add "$dir" 2>/dev/null
}
typeset -ag precmd_functions
if (( ! ${precmd_functions[(I)__zoxide_hook]} )); then
    precmd_functions+=(__zoxide_hook)
fi
z() {
    if [ $# -eq 0 ]; then
        builtin cd ~
    elif [ $# -eq 1 ] && [ "$1" = "-" ]; then
        builtin cd -
    elif [ $# -eq 1 ] && [ -d "$1" ]; then
        builtin cd "$1"
    else
        local result
        result=$(command modbox zoxide query --exclude "$PWD" -- "$@") || return
        builtin cd "$result"
    fi
}
zi() {
    local result
    result=$(command modbox zoxide query --interactive -- "$@") || return
    builtin cd "$result"
}
)SHELL");
    } else if (s == "fish") {
        printf(R"SHELL(function __zoxide_hook --on-event fish_prompt
    command modbox zoxide add (pwd -L) 2>/dev/null
end
function z
    if test (count $argv) -eq 0
        builtin cd ~
    else if test (count $argv) -eq 1; and test "$argv[1]" = "-"
        builtin cd -
    else if test (count $argv) -eq 1; and test -d "$argv[1]"
        builtin cd "$argv[1]"
    else
        set -l result (command modbox zoxide query --exclude (pwd -L) -- $argv)
        or return
        builtin cd "$result"
    end
end
function zi
    set -l result (command modbox zoxide query --interactive -- $argv)
    or return
    builtin cd "$result"
end
)SHELL");
    } else if (s == "posix") {
        printf(R"SHELL(z() {
    if [ $# -eq 0 ]; then
        cd ~ || return
    elif [ $# -eq 1 ] && [ "$1" = "-" ]; then
        cd - || return
    elif [ $# -eq 1 ] && [ -d "$1" ]; then
        cd "$1" || return
    else
        __zoxide_result=$(command modbox zoxide query --exclude "$(pwd)" -- "$@") || return
        cd "$__zoxide_result" || return
    fi
}
)SHELL");
    } else if (s == "nushell") {
        printf(R"SHELL(def --env z [...rest: string] {
    if ($rest | is-empty) {
        cd ~
    } else if ($rest | length) == 1 and ($rest | first) == "-" {
        cd -
    } else if ($rest | length) == 1 and ($rest | first | path exists) {
        cd ($rest | first)
    } else {
        let result = (modbox zoxide query --exclude (pwd) ...$rest | str trim)
        cd $result
    }
}
def --env zi [...rest: string] {
    let result = (modbox zoxide query --interactive ...$rest | str trim)
    cd $result
}
)SHELL");
    } else {
        fprintf(stderr, "zoxide: unsupported shell '%s'. Supported: bash, zsh, fish, nushell, posix\n", shell);
    }
}

void zoxide_command(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <SUBCOMMAND> [options]\n", argv[0]);
        printf("\n");
        printf("Subcommands:\n");
        printf("  add <PATH>       Add a directory to the database\n");
        printf("  edit             Edit the database\n");
        printf("  init <SHELL>     Print shell integration code\n");
        printf("  list             List all directories in the database\n");
        printf("  query [KEYWORDS] Search for a directory\n");
        printf("  remove <PATH>    Remove a directory from the database\n");
        printf("\n");
        printf("Options:\n");
        printf("  -h, --help       Display this help and exit\n");
        return;
    }

    std::string subcmd = argv[1];

    if (subcmd == "--help" || subcmd == "-h") {
        printf("Usage: %s <SUBCOMMAND> [options]\n", argv[0]);
        printf("\n");
        printf("Smart directory jumping based on frecency.\n");
        printf("\n");
        printf("Subcommands:\n");
        printf("  add <PATH>       Add a directory to the database\n");
        printf("  edit             Edit the database\n");
        printf("  init <SHELL>     Print shell integration code (bash, zsh, fish, nushell, posix)\n");
        printf("  list             List all directories in the database\n");
        printf("  query [KEYWORDS] Search for a directory matching keywords\n");
        printf("  remove <PATH>    Remove a directory from the database\n");
        printf("\n");
        printf("Shell integration:\n");
        printf("  eval \"$(modbox zoxide init bash)\"   # bash\n");
        printf("  eval \"$(modbox zoxide init zsh)\"    # zsh\n");
        printf("  modbox zoxide init fish | source     # fish\n");
        return;
    }

    std::string db_path = get_db_path();

    if (subcmd == "add") {
        if (argc < 3) {
            fprintf(stderr, "zoxide: 'add' requires a path argument\n");
            return;
        }
        cmd_add(db_path, argv[2]);
    } else if (subcmd == "remove") {
        if (argc < 3) {
            fprintf(stderr, "zoxide: 'remove' requires a path argument\n");
            return;
        }
        cmd_remove(db_path, argv[2]);
    } else if (subcmd == "edit") {
        cmd_edit(db_path);
    } else if (subcmd == "list" || subcmd == "ls") {
        cmd_list(db_path);
    } else if (subcmd == "query") {
        struct arg_lit *interactive_opt = arg_lit0("i", "interactive", "use interactive selection (requires fzf)");
        struct arg_str *exclude_opt = arg_str0(NULL, "exclude", "PATH", "exclude this path from results");
        struct arg_lit *help_opt = arg_lit0("h", "help", "display this help and exit");
        struct arg_str *keywords_arg = arg_strn(NULL, NULL, "KEYWORDS", 0, 100, "search keywords");
        struct arg_end *end = arg_end(20);

        void* argtable[] = {interactive_opt, exclude_opt, help_opt, keywords_arg, end};
        int nerrors = arg_parse(argc - 1, argv + 1, argtable);

        if (help_opt->count > 0) {
            printf("Usage: %s query [OPTION]... [KEYWORDS]...\n", argv[0]);
            printf("Search for a directory matching keywords.\n");
            printf("\n");
            printf("  -i, --interactive  use interactive selection (requires fzf)\n");
            printf("      --exclude=PATH exclude this path from results\n");
            printf("  -h, --help         display this help and exit\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        if (nerrors > 0) {
            arg_print_errors(stderr, end, argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        std::vector<std::string> keywords;
        for (int i = 0; i < keywords_arg->count; i++) {
            keywords.emplace_back(keywords_arg->sval[i]);
        }

        std::string exclude;
        if (exclude_opt->count > 0) {
            exclude = exclude_opt->sval[0];
        }

        if (interactive_opt->count > 0) {
            auto entries = load_db(db_path);
            struct Scored {
                double score;
                std::string path;
            };
            std::vector<Scored> matches;
            for (const auto& e : entries) {
                if (!exclude.empty() && e.path == exclude) continue;
                if (!keywords.empty() && !matches_keywords(e.path, keywords)) continue;
                double score = compute_frecency(e.rank, e.timestamp);
                matches.push_back({score, e.path});
            }
            std::sort(matches.begin(), matches.end(), [](const Scored& a, const Scored& b) {
                return a.score > b.score;
            });

            std::string fzf_input;
            for (const auto& m : matches) {
                fzf_input += m.path + "\n";
            }

            std::string cmd = "printf '%s' '" + fzf_input + "' | fzf --height=40% --reverse --select-1 --exit-0";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                fprintf(stderr, "zoxide: failed to run fzf\n");
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
            char buf[4096];
            std::string result;
            while (fgets(buf, sizeof(buf), pipe)) {
                result += buf;
            }
            int status = pclose(pipe);
            if (status != 0 || result.empty()) {
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
            printf("%s", result.c_str());
        } else {
            std::string result = cmd_query(db_path, keywords, exclude);
            if (!result.empty()) {
                printf("%s", result.c_str());
            }
        }

        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    } else if (subcmd == "init") {
        const char* shell = (argc >= 3) ? argv[2] : "";
        cmd_init(shell);
    } else {
        fprintf(stderr, "zoxide: unknown subcommand '%s'\n", subcmd.c_str());
        fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
    }
}

REGISTER_COMMAND("zoxide", zoxide_command, "Smart directory jumping based on frecency");
