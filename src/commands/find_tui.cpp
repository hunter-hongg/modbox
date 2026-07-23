#include "commands/find.hpp"
#include "commands/tui_base.hpp"
#include "commands/fs_classify.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <vector>
#include <string>

using namespace ftxui;

static std::string get_owner(uid_t uid) {
    struct passwd* pw = getpwuid(uid);
    return pw ? pw->pw_name : std::to_string(uid);
}

static std::string get_group(gid_t gid) {
    struct group* gr = getgrgid(gid);
    return gr ? gr->gr_name : std::to_string(gid);
}

static std::string read_file_preview(const char* path, int max_lines) {
    FILE* f = fopen(path, "r");
    if (!f) return "(unreadable)";
    std::string result;
    char buf[4096];
    int lines = 0;
    while (lines < max_lines && fgets(buf, sizeof(buf), f)) {
        result += buf;
        lines++;
    }
    if (!feof(f)) result += "\n[… truncated]";
    fclose(f);
    return result;
}

class FindTuiComponent : public TuiBase {
    std::vector<FindMatch> entries_;
    std::string detail_;
    std::string status_msg_;

public:
    explicit FindTuiComponent(std::vector<FindMatch> matches)
        : entries_(std::move(matches)) {}

    int entries_size() const override { return (int)entries_.size(); }
    int header_rows() const override { return 1; }
    void fill_entries() override { update_scroll_math(); }

    Element render_row(int idx) const override {
        if (idx < 0 || idx >= (int)entries_.size()) return text("");
        const auto& m = entries_[idx];
        const char* icon = "\xEF\x80\x96";
        switch (m.file_type) {
        case 3: icon = "\xEF\x84\x95"; break;
        case 4: icon = "\xEF\x95\xB0"; break;
        case 5: icon = "\xEF\x94\xA1"; break;
        case 6: icon = "\xEF\x94\xA2"; break;
        case 7: case 8: icon = "\xEF\xA4\x81"; break;
        case 2: icon = "\xEF\x92\x89"; break;
        default: break;
        }
        std::string size_str;
        if (m.size >= 1024ull * 1024ull * 1024ull) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1fG", (double)m.size / (1024.0 * 1024.0 * 1024.0));
            size_str = buf;
        } else if (m.size >= 1024ull * 1024ull) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1fM", (double)m.size / (1024.0 * 1024.0));
            size_str = buf;
        } else if (m.size >= 1024ull) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1fK", (double)m.size / 1024.0);
            size_str = buf;
        } else {
            size_str = std::to_string(m.size);
        }
        return hbox({
            text(icon) | color(Color::GrayLight),
            text(" " + m.display_name + "  ") | flex,
            text(size_str) | color(Color::GrayLight) | align_right,
        });
    }

    Element render_detail() const {
        if (entries_.empty() || selected_ < 0 || selected_ >= (int)entries_.size()) {
            return text("No matches") | dim | center;
        }
        const auto& m = entries_[selected_];
        Elements body;
        body.push_back(text(m.display_name) | bold);
        body.push_back(separator());
        body.push_back(text("Path: " + m.path));
        body.push_back(text("Type: " + std::string(type_prefix_char_str((FileType)m.file_type)) +
                            "  Size: " + std::to_string(m.size) + "  Modified: " + m.mtime_str));
        body.push_back(text("Perms: " + m.perm_str + "  Owner: " + get_owner_from_match(m) +
                            ":" + get_group_from_match(m)));
        if (!detail_.empty()) {
            body.push_back(separator());
            auto lines_vec = split_lines(detail_);
            std::vector<Element> preview_elements;
            for (const auto& l : lines_vec) {
                preview_elements.push_back(text(l));
            }
            body.push_back(vbox(preview_elements) | yframe | flex);
        }
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
            " q:quit  /:search  d:delete  p:preview  n/N:next-prev  j/k:scroll"
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
        if (event == Event::Character('d')) {
            if (selected_ >= 0 && selected_ < (int)entries_.size()) {
                const auto& m = entries_[selected_];
                struct stat st;
                if (lstat(m.path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    if (rmdir(m.path.c_str()) == 0) {
                        status_msg_ = "Deleted: " + m.display_name;
                    } else {
                        status_msg_ = "Delete failed: " + std::string(strerror(errno));
                    }
                } else {
                    if (unlink(m.path.c_str()) == 0) {
                        status_msg_ = "Deleted: " + m.display_name;
                    } else {
                        status_msg_ = "Delete failed: " + std::string(strerror(errno));
                    }
                }
                entries_.erase(entries_.begin() + selected_);
                if (selected_ >= (int)entries_.size()) {
                    selected_ = (int)entries_.size() - 1;
                }
                if (selected_ < 0) selected_ = 0;
                scroll_offset_ = 0;
                update_scroll_math();
            }
            return true;
        }
        if (event == Event::Character('p')) {
            if (selected_ >= 0 && selected_ < (int)entries_.size()) {
                const auto& m = entries_[selected_];
                detail_ = read_file_preview(m.path.c_str(), 50);
            }
            return true;
        }
        if (event == Event::Character('n')) {
            if (!search_query_.empty()) {
                for (int i = selected_ + 1; i < entries_size(); i++) {
                    if (entries_[i].display_name.find(search_query_) != std::string::npos) {
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
                    if (entries_[i].display_name.find(search_query_) != std::string::npos) {
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
        if (event == Event::Custom) {
            update_scroll_math();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

private:
    static std::string get_owner_from_match(const FindMatch& m) {
        size_t pos = m.path.find_last_of('/');
        const char* basename = pos != std::string::npos ? m.path.c_str() + pos + 1 : m.path.c_str();
        struct stat st;
        if (lstat(m.path.c_str(), &st) != 0) return "?";
        return get_owner(st.st_uid);
    }

    static std::string get_group_from_match(const FindMatch& m) {
        struct stat st;
        if (lstat(m.path.c_str(), &st) != 0) return "?";
        return get_group(st.st_gid);
    }

    static std::vector<std::string> split_lines(const std::string& s) {
        std::vector<std::string> out;
        size_t start = 0;
        size_t pos = 0;
        while (pos < s.size()) {
            if (s[pos] == '\n') {
                out.push_back(s.substr(start, pos - start));
                start = pos + 1;
            }
            pos++;
        }
        out.push_back(s.substr(start));
        return out;
    }
};

void find_tui_main(int argc, char** argv) {
    FindOptions opts;

    int help_requested = 0;
    if (find_parse_args(argc, argv, &opts, &help_requested) != 0) {
        return;
    }

    if (help_requested) {
        find_usage(argv[0]);
        return;
    }

    if (!opts.has_action) {
        opts.do_print = 1;
    }

    if (opts.paths.empty()) {
        opts.paths.push_back(".");
    }

    auto matches = find_collect_matches(&opts);

    if (matches.empty()) {
        return;
    }

    auto component = std::make_shared<FindTuiComponent>(std::move(matches));
    component->fill_entries();
    auto screen = App::FitComponent();
    screen.Loop(component);
}
