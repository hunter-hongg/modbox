#include <argtable3.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/grep.hpp"
#include "commands/grep_tui.hpp"
#include "commands/search_common.hpp"
#include "commands/tui_base.hpp"

#include <ftxui/component/app.hpp>

using namespace ftxui;

#define GREP_MAX_FILES 200

static std::string truncate_line(const std::string& line, size_t max_len) {
    if (line.size() <= max_len) return line;
    return line.substr(0, max_len - 3) + "...";
}

static void open_in_editor(const char* path) {
    const char* editor = getenv("EDITOR");
    if (!editor) editor = getenv("PAGER");
    if (!editor) editor = "cat";

    pid_t pid = fork();
    if (pid == 0) {
        execlp(editor, editor, path, (char*)nullptr);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

class GrepTuiComponent : public TuiBase {
    std::vector<GrepMatch> entries_;
    std::string status_msg_;

public:
    explicit GrepTuiComponent(std::vector<GrepMatch> matches)
        : entries_(std::move(matches)) {}

    int entries_size() const override { return (int)entries_.size(); }
    int header_rows() const override { return 1; }
    void fill_entries() override { update_scroll_math(); }

    Element render_row(int idx) const override {
        if (idx < 0 || idx >= (int)entries_.size()) return text("");
        const auto& m = entries_[idx];

        char buf[64];
        snprintf(buf, sizeof(buf), "%d", m.line_number);

        std::string display = m.display_name + ":" + buf + ": " + truncate_line(m.line_content, 80);

        return text(display);
    }

    Element render_detail() const {
        if (entries_.empty() || selected_ < 0 || selected_ >= (int)entries_.size()) {
            return text("No matches") | dim | center;
        }
        const auto& m = entries_[selected_];

        Elements body;
        body.push_back(text(m.display_name) | bold);
        body.push_back(separator());

        if (m.line_content.empty()) {
            body.push_back(text("(empty line)") | dim);
        } else {
            Elements line_elements;
            if (m.match_start < m.line_content.size()) {
                if (m.match_start > 0) {
                    line_elements.push_back(text(m.line_content.substr(0, m.match_start)));
                }
                size_t match_len = m.match_end - m.match_start;
                if (match_len > m.line_content.size() - m.match_start) {
                    match_len = m.line_content.size() - m.match_start;
                }
                line_elements.push_back(text(m.line_content.substr(m.match_start, match_len)) | color(Color::Red) | bold);
                if (m.match_end < m.line_content.size()) {
                    line_elements.push_back(text(m.line_content.substr(m.match_end)));
                }
            } else {
                line_elements.push_back(text(m.line_content));
            }
            body.push_back(hbox(std::move(line_elements)));
        }

        body.push_back(separator());
        body.push_back(text("Line: " + std::to_string(m.line_number) + "  File: " + m.file_path) | color(Color::GrayLight));

        return vbox(body) | flex;
    }

    Element OnRender() override {
        using namespace ftxui;
        if (entries_.empty()) {
            return text("(no matches)") | dim | center;
        }

        Elements body;
        body.push_back(render_search_bar());
        body.push_back(separator());
        body.push_back(render_list() | flex);
        body.push_back(separator());
        body.push_back(render_detail() | flex);

        Elements footer;
        footer.push_back(text(
            " q:quit  /:search  n/N:next-prev  Enter:open  j/k:scroll"
        ) | color(Color::GrayLight));
        if (!status_msg_.empty()) {
            footer.push_back(text("  " + status_msg_) | bold | color(Color::Green));
        }
        body.push_back(separator());
        body.push_back(hbox(footer));

        return vbox(body) | flex;
    }

    bool OnEvent(Event event) override {
        using namespace ftxui;

        if (search_mode_) {
            if (event == Event::Escape) {
                search_mode_ = false;
                search_input_.clear();
                return true;
            }
            if (event == Event::Return) {
                search_mode_ = false;
                search_query_ = search_input_;
                search_input_.clear();
                selected_ = 0;
                scroll_offset_ = 0;
                update_scroll_math();
                return true;
            }
            if (handle_search(event)) return true;
            return ComponentBase::OnEvent(event);
        }

        if (handle_nav(event)) return true;

        if (event == Event::Character('q') || event == Event::Character('Q')) {
            if (auto* app = App::Active()) app->Exit();
            return true;
        }
        if (event == Event::Character('n')) {
            if (!search_query_.empty()) {
                for (int i = selected_ + 1; i < entries_size(); i++) {
                    if (entries_[i].line_content.find(search_query_) != std::string::npos) {
                        selected_ = i;
                        update_scroll_math();
                        break;
                    }
                }
                return true;
            }
        }
        if (event == Event::Character('N')) {
            if (!search_query_.empty()) {
                for (int i = selected_ - 1; i >= 0; i--) {
                    if (entries_[i].line_content.find(search_query_) != std::string::npos) {
                        selected_ = i;
                        update_scroll_math();
                        break;
                    }
                }
                return true;
            }
        }
        if (event == Event::Character('/')) {
            search_mode_ = true;
            search_input_ = search_query_;
            return true;
        }
        if (event == Event::Escape) {
            if (!search_query_.empty()) {
                search_query_.clear();
                selected_ = 0;
                scroll_offset_ = 0;
                update_scroll_math();
            }
            return true;
        }
        if (event == Event::Return || event == Event::Character('o')) {
            if (selected_ >= 0 && selected_ < (int)entries_.size()) {
                open_in_editor(entries_[selected_].file_path.c_str());
                status_msg_ = "Opened: " + entries_[selected_].file_path;
            }
            return true;
        }
        if (event == Event::Custom) {
            update_scroll_math();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }
};

static int collect_search_file(const char* path, bool is_stdin,
                               const char* display_name, const GrepOptions* opts,
                               const std::regex* re,
                               std::vector<GrepMatch>& matches) {
    FILE* fp;
    if (is_stdin) {
        fp = stdin;
        display_name = nullptr;
    } else {
        fp = fopen(path, "r");
        if (fp == nullptr) {
            (void)fprintf(stderr, "grep: %s: %s\n", path, strerror(errno));
            return 0;
        }
    }

    int match_count = 0;
    int line_count = 0;
    char* line = nullptr;
    size_t linecap = 0;

    while (getline(&line, &linecap, fp) > 0) {
        line_count++;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        bool matched = false;
        size_t m_start = 0;
        size_t m_end = 0;

        if (opts->mode == GrepMode::FIXED) {
            std::string work_line(line, len);
            if (opts->ignore_case) {
                for (auto& ch : work_line) {
                    ch = (char)std::tolower((unsigned char)ch);
                }
            }
            std::string lower_pat = opts->pattern;
            if (opts->ignore_case) {
                for (auto& ch : lower_pat) {
                    ch = (char)std::tolower((unsigned char)ch);
                }
            }
            matched = search_fixed_loop(work_line.c_str(), lower_pat.c_str(), len,
                                        &m_start, &m_end,
                                        opts->word_regexp, opts->line_regexp);
        } else if (opts->word_regexp) {
            std::string s(line, len);
            std::smatch m;
            auto search_start = s.cbegin();
            while (std::regex_search(search_start, s.cend(), m, *re)) {
                std::size_t abs_pos = (std::size_t)(m.position(0) + (search_start - s.cbegin()));
                if (search_check_word_boundary(line, abs_pos, abs_pos + m.length(0), len)) {
                    matched = true;
                    m_start = abs_pos;
                    m_end = abs_pos + m.length(0);
                    break;
                }
                search_start = m.suffix().first;
            }
        } else {
            std::string s(line, len);
            std::smatch m;
            if (std::regex_search(s, m, *re)) {
                matched = true;
                m_start = m.position(0);
                m_end = m.position(0) + m.length(0);
            }
        }

        if (opts->invert_match) {
            matched = !matched;
        }

        if (matched) {
            match_count++;
            GrepMatch gm;
            gm.file_path = display_name ? display_name : "(standard input)";
            gm.display_name = display_name ? display_name : "(standard input)";
            gm.line_number = line_count;
            gm.line_content = line;
            gm.match_start = m_start;
            gm.match_end = m_end;
            matches.push_back(std::move(gm));
        }
    }

    free(line);
    if (!is_stdin) {
        (void)fclose(fp);
    }
    return match_count;
}

static int collect_search_directory(const char* dirpath, const GrepOptions* opts,
                                    const std::regex* re,
                                    std::vector<GrepMatch>& matches) {
    std::error_code ec;
    std::filesystem::path dir(dirpath);

    int total_matches = 0;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            (void)fprintf(stderr, "grep: %s: %s\n", dirpath, strerror(errno));
            return total_matches;
        }

        std::string filename = entry.path().filename().string();
        if (filename == "." || filename == "..") {
            continue;
        }

        std::string full_path = entry.path().string();
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total_matches += collect_search_directory(full_path.c_str(), opts, re, matches);
            } else if (S_ISREG(st.st_mode)) {
                int m = collect_search_file(full_path.c_str(), false, full_path.c_str(), opts, re, matches);
                if (m > 0) total_matches += m;
            }
        }
    }

    return total_matches;
}

std::vector<GrepMatch> grep_collect_matches(const GrepOptions* opts,
                                            const std::regex* re,
                                            struct arg_file* file_arg) {
    std::vector<GrepMatch> matches;

    if (!opts->recursive) {
        for (int i = 0; i < file_arg->count && i < GREP_MAX_FILES; i++) {
            const char* fname = file_arg->filename[i];
            struct stat st;
            if (stat(fname, &st) == 0 && S_ISDIR(st.st_mode)) {
                (void)fprintf(stderr,
                              "grep: %s: Is a directory (use -r for recursive)\n",
                              fname);
                continue;
            }
            collect_search_file(fname, false, fname, opts, re, matches);
        }
    } else {
        for (int i = 0; i < file_arg->count && i < GREP_MAX_FILES; i++) {
            const char* fname = file_arg->filename[i];
            struct stat st;
            if (stat(fname, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    collect_search_directory(fname, opts, re, matches);
                } else {
                    collect_search_file(fname, false, fname, opts, re, matches);
                }
            }
        }
    }

    return matches;
}

void grep_tui_main(int argc, char** argv) {
    if (!isatty(STDERR_FILENO)) {
        (void)fprintf(stderr, "grep: --tui requires a terminal\n");
        return;
    }

    GrepOptions opts;
    opts.mode = GrepMode::BASIC;
    opts.color_mode = GrepColor::NEVER;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--color") == 0) {
            argv[i] = "--color=always";
            break;
        }
    }

    struct arg_lit* extended_opt =
        arg_lit0("E", "extended-regexp", "interpret pattern as extended regex (ERE)");
    struct arg_lit* fixed_opt =
        arg_lit0("F", "fixed-strings", "interpret pattern as fixed strings");
    struct arg_lit* ignore_case_opt =
        arg_lit0("i", "ignore-case", "ignore case distinctions");
    struct arg_lit* invert_opt =
        arg_lit0("v", "invert-match", "select non-matching lines");
    struct arg_lit* line_number_opt =
        arg_lit0("n", "line-number", "print line number with output lines");
    struct arg_lit* recursive_opt =
        arg_lit0("r", "recursive", "read all files under directories recursively");
    struct arg_lit* recursive2_opt =
        arg_lit0("R", "dereference-recursive",
                 "read all files under directories recursively (follow symlinks)");
    struct arg_lit* word_regexp_opt =
        arg_lit0("w", "word-regexp", "match only whole words");
    struct arg_lit* line_regexp_opt =
        arg_lit0("x", "line-regexp", "match only whole lines");
    struct arg_lit* only_matching_opt =
        arg_lit0("o", "only-matching", "show only matched part of line");
    struct arg_lit* help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_str* pattern_opt =
        arg_str0("e", "regexp", "PATTERN",
                 "use PATTERN as the pattern (protect patterns starting with -)");
    struct arg_file* file_arg =
        arg_filen(nullptr, nullptr, "FILE", 0, GREP_MAX_FILES, "file to search");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {extended_opt,  fixed_opt,
                        ignore_case_opt, invert_opt,     line_number_opt,
                        recursive_opt,  recursive2_opt,
                        word_regexp_opt, line_regexp_opt, only_matching_opt,
                        pattern_opt,    help_opt,
                        file_arg,        end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... PATTERN [FILE]...\n", argv[0]);
        printf("Search for PATTERN in each FILE or standard input.\n");
        printf("\n");
        printf("Pattern selection:\n");
        printf("  -E, --extended-regexp     PATTERN is an extended regular expression\n");
        printf("  -F, --fixed-strings       PATTERN is a set of fixed strings\n");
        printf("  -i, --ignore-case         ignore case distinctions\n");
        printf("  -n, --line-number         print line number with output lines\n");
        printf("  -r, --recursive           search directories recursively\n");
        printf("  -R                        like -r, but follow all symlinks\n");
        printf("  -w, --word-regexp         match only whole words\n");
        printf("  -x, --line-regexp         match only whole lines\n");
        printf("  -o, --only-matching       show only matched part of line\n");
        printf("  -e, --regexp=PATTERN      use PATTERN as the pattern\n");
        printf("      --tui                 interactive TUI viewer\n");
        printf("      --color=WHEN          highlight matching text\n");
        printf("  -h, --help                display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (extended_opt->count > 0) {
        opts.mode = GrepMode::EXTENDED;
    }
    if (fixed_opt->count > 0) {
        opts.mode = GrepMode::FIXED;
    }

    opts.ignore_case = (ignore_case_opt->count > 0);
    opts.invert_match = (invert_opt->count > 0);
    opts.line_number = (line_number_opt->count > 0);
    opts.recursive = (recursive_opt->count > 0) || (recursive2_opt->count > 0);
    opts.word_regexp = (word_regexp_opt->count > 0);
    opts.line_regexp = (line_regexp_opt->count > 0);
    opts.only_matching = (only_matching_opt->count > 0);

    const char* pattern = nullptr;
    if (pattern_opt->count > 0) {
        pattern = pattern_opt->sval[0];
    }
    if (pattern == nullptr && file_arg->count > 0) {
        pattern = file_arg->filename[0];
        file_arg->count--;
        for (int i = 0; i < file_arg->count; i++) {
            file_arg->filename[i] = file_arg->filename[i + 1];
        }
    }

    if (pattern == nullptr) {
        (void)fprintf(stderr, "grep: no pattern specified\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.pattern = pattern;

    std::optional<std::regex> re_opt;
    const std::regex* re = nullptr;
    if (opts.mode != GrepMode::FIXED) {
        auto re_flags = std::regex::optimize;
        if (opts.ignore_case) {
            re_flags |= std::regex::icase;
        }
        try {
            re_opt.emplace(search_compile_pattern(opts.pattern, opts.word_regexp,
                                                 opts.line_regexp, re_flags));
            re = &*re_opt;
        } catch (const std::regex_error& e) {
            (void)fprintf(stderr, "grep: invalid pattern '%s': %s\n", pattern, e.what());
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    auto matches = grep_collect_matches(&opts, re, file_arg);

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (matches.empty()) {
        return;
    }

    auto component = std::make_shared<GrepTuiComponent>(std::move(matches));
    component->fill_entries();
    auto screen = App::FitComponent();
    screen.Loop(component);
}
