#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <unistd.h>

#include "commands/tui_base.hpp"
#include "commands/cat/helpers.hpp"
#include "commands/cat/highlight.hpp"

using namespace ftxui;

struct CatFileData {
    std::vector<std::string> lines;
    std::string display_name;
    std::string ext;
    bool is_stdin;
};

struct StyledSegment {
    std::string text;
    Color color;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
};

static std::vector<StyledSegment> parse_ansi(const std::string& s) {
    std::vector<StyledSegment> segs;
    if (s.empty()) return segs;

    Color cur_color = Color::Default;
    bool cur_bold = false;
    bool cur_dim = false;
    bool cur_italic = false;
    bool cur_underline = false;

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j < s.size() && s[j] != 'm') j++;
            if (j < s.size()) {
                std::string param = s.substr(i + 2, j - i - 2);
                if (param.empty() || param == "0") {
                    cur_color = Color::Default;
                    cur_bold = false;
                    cur_dim = false;
                    cur_italic = false;
                    cur_underline = false;
                } else {
                    size_t p = 0;
                    while (p < param.size()) {
                        size_t semi = param.find(';', p);
                        std::string tok = param.substr(p, semi == std::string::npos ? semi : semi - p);
                        int val = atoi(tok.c_str());
                        if (val == 0) {
                            cur_color = Color::Default;
                            cur_bold = false;
                            cur_dim = false;
                            cur_italic = false;
                            cur_underline = false;
                        } else if (val == 1) {
                            cur_bold = true;
                        } else if (val == 2) {
                            cur_dim = true;
                        } else if (val == 3) {
                            cur_italic = true;
                        } else if (val == 4) {
                            cur_underline = true;
                        } else if (val >= 30 && val <= 37) {
                            static const Color table[] = {
                                Color::Black, Color::Red, Color::Green, Color::Yellow,
                                Color::Blue, Color::Magenta, Color::Cyan, Color::White
                            };
                            cur_color = table[val - 30];
                        } else if (val >= 90 && val <= 97) {
                            static const Color table[] = {
                                Color::GrayLight, Color::RedLight, Color::GreenLight,
                                Color::YellowLight, Color::BlueLight, Color::MagentaLight,
                                Color::CyanLight, Color::White
                            };
                            cur_color = table[val - 90];
                        }
                        p = semi == std::string::npos ? param.size() : semi + 1;
                    }
                }
                i = j + 1;
                continue;
            }
        }
        size_t start = i;
        while (i < s.size() && s[i] != '\033') i++;
        if (i > start) {
            std::string chunk = s.substr(start, i - start);
            StyledSegment seg;
            seg.text = chunk;
            seg.color = cur_color;
            seg.bold = cur_bold;
            seg.dim = cur_dim;
            seg.italic = cur_italic;
            seg.underline = cur_underline;
            segs.push_back(seg);
        }
    }
    if (segs.empty()) {
        StyledSegment seg;
        seg.text = s;
        segs.push_back(seg);
    }
    return segs;
}

static std::string highlight_line(const char* line, const char* ext) {
    if (!line || !ext || !ext[0]) return line;
    char* buf = nullptr;
    size_t size = 0;
    FILE* fp = open_memstream(&buf, &size);
    if (!fp) return line;
    print_highlighted(line, ext, fp);
    fclose(fp);
    if (!buf) return line;
    std::string result(buf, size);
    free(buf);
    return result;
}

class CatTuiComponent : public TuiBase {
    std::vector<CatFileData> files_;
    int current_file_ = 0;
    bool show_line_numbers_ = false;
    bool show_nonempty_line_numbers_ = false;
    int number_format_ = 0;
    int number_width_ = 5;
    bool highlight_mode_ = false;

public:
    CatTuiComponent() = default;

    void configure(bool number_mode, bool nonempty_number_mode, int number_format, bool highlight_mode) {
        show_line_numbers_ = number_mode;
        show_nonempty_line_numbers_ = nonempty_number_mode;
        number_format_ = number_format;
        if (number_mode || nonempty_number_mode) {
            number_width_ = 6;
        }
        highlight_mode_ = highlight_mode;
    }

    void add_file(std::string lines_str, std::string name, std::string ext, bool is_stdin) {
        CatFileData fd;
        fd.display_name = std::move(name);
        fd.ext = std::move(ext);
        fd.is_stdin = is_stdin;

        size_t pos = 0;
        while (pos < lines_str.size()) {
            size_t nl = lines_str.find('\n', pos);
            if (nl == std::string::npos) {
                fd.lines.push_back(lines_str.substr(pos));
                pos = lines_str.size();
            } else {
                fd.lines.push_back(lines_str.substr(pos, nl - pos));
                pos = nl + 1;
            }
        }
        files_.push_back(std::move(fd));
    }

    int entries_size() const override { return current_file_ >= 0 && current_file_ < (int)files_.size() ? (int)files_[current_file_].lines.size() : 0; }

    void fill_entries() override {}

    ftxui::Element render_row(int idx) const override {
        if (current_file_ < 0 || current_file_ >= (int)files_.size()) return text("");
        const auto& fd = files_[current_file_];
        if (idx < 0 || idx >= (int)fd.lines.size()) return text("");

        const std::string& raw = fd.lines[idx];
        int blank = raw.empty();

        std::vector<Element> row_elements;

        if (show_line_numbers_) {
            bool should_number = show_line_numbers_ || (show_nonempty_line_numbers_ && !blank);
            if (should_number) {
                char buf[32];
                if (number_format_ == 1) snprintf(buf, sizeof(buf), "0x%04x", idx + 1);
                else if (number_format_ == 2) snprintf(buf, sizeof(buf), "%06o", idx + 1);
                else snprintf(buf, sizeof(buf), "%*d", number_width_, idx + 1);
                row_elements.push_back(text(buf) | color(Color::GrayLight));
            } else {
                row_elements.push_back(text(std::string(number_width_, ' ')));
            }
        }

        std::string line = raw;
        if (highlight_mode_ && !fd.ext.empty()) {
            line = highlight_line(raw.c_str(), fd.ext.c_str());
        }

        auto segs = parse_ansi(line);
        Elements line_elements;
        for (const auto& seg : segs) {
            Element el = text(seg.text);
            if (seg.bold) el = el | bold;
            if (seg.dim) el = el | dim;
            if (seg.italic) el = el | italic;
            if (seg.underline) el = el | underlined;
            if (seg.color != Color::Default) el = el | color(seg.color);
            line_elements.push_back(el);
        }
        row_elements.push_back(hbox(std::move(line_elements)));

        return hbox(std::move(row_elements));
    }

    bool OnEvent(Event event) override {
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
                    if (files_[current_file_].lines[i].find(search_query_) != std::string::npos) {
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
                    if (files_[current_file_].lines[i].find(search_query_) != std::string::npos) {
                        selected_ = i;
                        update_scroll_math();
                        break;
                    }
                }
                return true;
            }
        }
        if (event == Event::Tab) {
            if (!files_.empty()) {
                current_file_ = (current_file_ + 1) % (int)files_.size();
                selected_ = 0;
                scroll_offset_ = 0;
                update_scroll_math();
            }
            return true;
        }
        if (event == Event::Character('p')) {
            if (!files_.empty()) {
                current_file_ = (current_file_ - 1 + (int)files_.size()) % (int)files_.size();
                selected_ = 0;
                scroll_offset_ = 0;
                update_scroll_math();
            }
            return true;
        }
        if (event == Event::Character('x')) {
            if (files_.size() > 1) {
                files_.erase(files_.begin() + current_file_);
                if (current_file_ >= (int)files_.size()) current_file_ = (int)files_.size() - 1;
                if (current_file_ < 0) current_file_ = 0;
                selected_ = 0;
                scroll_offset_ = 0;
                update_scroll_math();
            } else if (files_.size() == 1) {
                if (auto* app = App::Active()) app->Exit();
                return true;
            }
            return true;
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
        if (event == Event::Custom) {
            update_scroll_math();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

    Element OnRender() override {
        using namespace ftxui;

        if (files_.empty()) return text("(empty)") | dim | center;

        Elements body;

        if (files_.size() > 1) {
            Elements tabs;
            for (size_t i = 0; i < files_.size(); i++) {
                auto label = text(files_[i].display_name);
                if ((int)i == current_file_) {
                    label = label | bold | color(Color::Cyan);
                }
                if ((int)i == current_file_) {
                    tabs.push_back(label | bgcolor(Color::Blue));
                } else {
                    tabs.push_back(label);
                }
                if (i + 1 < files_.size()) tabs.push_back(text(" "));
            }
            body.push_back(hbox(tabs));
            body.push_back(separator());
        }

        body.push_back(render_list());

        Elements footer;
        footer.push_back(text(" q:quit  /:search  n/N:next-prev  Tab/Shift+Tab:switch  x:close  j/k:scroll ") | color(Color::GrayLight));
        body.push_back(separator());
        body.push_back(hbox(footer));

        return vbox(body) | flex;
    }
};

void cat_tui_main(int file_count, const char** filenames, bool number_mode, bool nonempty_number_mode, int number_format, bool highlight_mode) {
    if (!isatty(STDOUT_FILENO)) {
        return;
    }

    auto component = std::make_shared<CatTuiComponent>();
    component->configure(number_mode, nonempty_number_mode, number_format, highlight_mode);

    bool have_stdin = false;
    std::string stdin_content;
    for (int i = 0; i < file_count; i++) {
        const char* name = filenames[i];
        if (strcmp(name, "-") == 0 || file_count == 1 && filenames[0] == nullptr) {
            have_stdin = true;
        }
    }
    if (file_count == 0) {
        have_stdin = true;
    }
    if (have_stdin) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            stdin_content.append(buf, n);
        }
        if (!stdin_content.empty() && stdin_content.back() != '\n') {
            stdin_content.push_back('\n');
        }
    }

    for (int i = 0; i < file_count; i++) {
        const char* name = filenames[i];
        if (strcmp(name, "-") == 0) {
            std::string ext;
            component->add_file(stdin_content, "(stdin)", ext, true);
            continue;
        }
        std::string content;
        if (highlight_mode) {
            const char* ext = get_file_extension(name);
            std::vector<PipelineLine*> plines = read_file_to_lines(name);
            char* buf = nullptr;
            size_t size = 0;
            FILE* fp = open_memstream(&buf, &size);
            for (auto* pl : plines) {
                print_highlighted(pl->text.c_str(), ext, fp);
                delete pl;
            }
            fclose(fp);
            if (buf) {
                content.assign(buf, size);
                free(buf);
            }
        } else {
            std::vector<PipelineLine*> plines = read_file_to_lines(name);
            for (auto* pl : plines) {
                content += pl->text;
                delete pl;
            }
        }
        std::string ext = get_file_extension(name);
        component->add_file(std::move(content), name, ext, false);
    }

    if (!have_stdin && file_count == 0) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            stdin_content.append(buf, n);
        }
        if (!stdin_content.empty() && stdin_content.back() != '\n') {
            stdin_content.push_back('\n');
        }
        component->add_file(std::move(stdin_content), "(stdin)", "", true);
    }

    if (component->entries_size() == 0 && file_count > 0 && !have_stdin) {
        fprintf(stderr, "cat: %s: No such file or directory\n", filenames[0]);
        return;
    }

    auto screen = App::FitComponent();
    screen.Loop(component);
}

void cat_tui_main(int file_count, const char** filenames) {
    cat_tui_main(file_count, filenames, false, false, 0, false);
}
