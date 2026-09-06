#include <argtable3.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "commands/man.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return out;
}

std::filesystem::path get_man_dir() {
    std::filesystem::path src(__FILE__);
    std::filesystem::path base = src.parent_path().parent_path().parent_path();
    return base / "docs" / "man";
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string find_name_line(const std::string& content) {
    std::istringstream iss(content);
    std::string line;
    bool in_name = false;
    while (std::getline(iss, line)) {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch){ return !std::isspace(ch); }));
        if (trimmed.rfind("# NAME", 0) == 0) {
            in_name = true;
            continue;
        }
        if (in_name) {
            if (trimmed.empty()) continue;
            if (trimmed.rfind("# ", 0) == 0) break;
            return trimmed;
        }
    }
    return {};
}

std::string basename_no_ext(const std::filesystem::path& p) {
    std::string name = p.stem().string();
    return name;
}

std::vector<std::filesystem::path> list_man_pages(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> pages;
    if (!std::filesystem::exists(dir)) return pages;
    for (auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".md") {
            pages.push_back(e.path());
        }
    }
    return pages;
}

std::filesystem::path resolve_page(const std::string& name, const std::filesystem::path& dir) {
    std::string lower = to_lower(name);
    std::string base = name;
    if (base.rfind("modbox-", 0) == 0) {
        base = base.substr(7);
    }
    std::filesystem::path candidate = dir / ("modbox-" + base + ".1.md");
    if (std::filesystem::exists(candidate)) return candidate;
    candidate = dir / (name + ".md");
    if (std::filesystem::exists(candidate)) return candidate;
    candidate = dir / ("modbox-" + name + ".1.md");
    if (std::filesystem::exists(candidate)) return candidate;
    for (auto& e : list_man_pages(dir)) {
        std::string stem = to_lower(e.stem().string());
        if (stem == "modbox-" + lower || stem == "modbox-" + lower + ".1" || stem.find(lower) != std::string::npos) {
            if (stem.find(lower) != std::string::npos) {
                return e.path();
            }
        }
    }
    return {};
}

void print_help(const char* prog) {
    printf("Usage: %s [OPTIONS] [PAGE]\n", prog);
    printf("Display manual pages for modbox commands.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -k, --apropos KEYWORD   search manual pages for KEYWORD\n");
    printf("  -f, --whatis PAGE       display one-line description\n");
    printf("  -a, --all               show all matching pages\n");
    printf("  -h, --help              display this help and exit\n");
    printf("  -V, --version           output version information and exit\n");
}

int man_command(int argc, char** argv) {
    const char* prog = argv[0];

    struct arg_lit* apropos_opt = arg_lit0("k", "apropos", "search manual pages for keyword");
    struct arg_lit* whatis_opt = arg_lit0("f", "whatis", "display one-line description");
    struct arg_lit* all_opt = arg_lit0("a", "all", "show all matching pages");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_str* page_arg = arg_str0(nullptr, nullptr, "PAGE", 0, 1, "manual page name");
    struct arg_end* end = arg_end(20);

    ArgTable at({apropos_opt, whatis_opt, all_opt, help_opt, version_opt, page_arg, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_help(prog);
        return 0;
    }
    if (version_opt->count > 0) {
        print_version("man");
        return 0;
    }
    if (nerrors > 0) {
        return at.print_errors(end, prog);
    }

    std::filesystem::path man_dir = get_man_dir();
    if (!std::filesystem::exists(man_dir)) {
        fprintf(stderr, "%s: manual page directory not found\n", prog);
        return 1;
    }

    if (apropos_opt->count > 0) {
        if (page_arg->count == 0) {
            fprintf(stderr, "%s: -k requires a keyword\n", prog);
            return 1;
        }
        std::string keyword = to_lower(page_arg->sval[0]);
        bool found = false;
        auto pages = list_man_pages(man_dir);
        for (auto& p : pages) {
            std::string content = read_file(p);
            if (content.empty()) continue;
            std::string lc = to_lower(content);
            if (lc.find(keyword) != std::string::npos) {
                std::string name_line = find_name_line(content);
                std::string base = basename_no_ext(p);
                if (name_line.empty()) name_line = base;
                printf("%s - %s\n", base.c_str(), name_line.c_str());
                found = true;
                if (!all_opt->count) {
                }
            }
        }
        if (!found) {
            fprintf(stderr, "%s: no manual entry for '%s'\n", prog, page_arg->sval[0]);
            return 1;
        }
        return 0;
    }

    if (whatis_opt->count > 0) {
        if (page_arg->count == 0) {
            fprintf(stderr, "%s: -f requires a page name\n", prog);
            return 1;
        }
        std::string name = page_arg->sval[0];
        auto p = resolve_page(name, man_dir);
        if (!p.empty()) {
            std::string content = read_file(p);
            std::string name_line = find_name_line(content);
            std::string base = basename_no_ext(p);
            if (name_line.empty()) name_line = base;
            printf("%s - %s\n", base.c_str(), name_line.c_str());
            return 0;
        } else {
            fprintf(stderr, "%s: no manual entry for '%s'\n", prog, name.c_str());
            return 1;
        }
    }

    if (page_arg->count == 0) {
        fprintf(stderr, "%s: missing operand\n", prog);
        fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return 1;
    }
    std::string name = page_arg->sval[0];
    auto p = resolve_page(name, man_dir);
    if (p.empty()) {
        fprintf(stderr, "%s: no manual entry for '%s'\n", prog, name.c_str());
        return 1;
    }
    std::string content = read_file(p);
    if (content.empty()) {
        fprintf(stderr, "%s: cannot read manual page\n", prog);
        return 1;
    }
    fwrite(content.c_str(), 1, content.size(), stdout);
    return 0;
}

REGISTER_COMMAND("man", man_command, "Display manual pages");
