#include "commands/ls_tui.hpp"
#include "commands/ls.hpp"
#include "commands/ls_entry.hpp"
#include "commands/tui_base.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <vector>
#include <algorithm>
#include <filesystem>

using namespace ftxui;

static std::string get_owner(uid_t uid) {
  struct passwd* pw = getpwuid(uid);
  return pw ? pw->pw_name : std::to_string(uid);
}

static std::string get_group(gid_t gid) {
  struct group* gr = getgrgid(gid);
  return gr ? gr->gr_name : std::to_string(gid);
}

static std::string fmt_time(time_t t) {
  char buf[32];
  strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&t));
  return buf;
}

 static const char* sort_label(SortMode m) {
 switch (m) {
 case SortMode::Size: return "size";
 case SortMode::Mtime: return "mtime";
 case SortMode::Type: return "type";
 default: return "name";
 }
}

static TuiEntry ls_entry_to_tui(const LsEntry& e) {
  TuiEntry te;
  te.path = e.path;
  te.display_name = e.display_name;
  te.size = (uint64_t)e.st.st_size;
  te.mtime = e.st.st_mtime;
  te.mtime_str = fmt_time(e.st.st_mtime);

  if (e.st.st_mode != 0) {
    te.file_type = classify(e.st);

    te.perm_str = std::string(1, type_prefix_char(te.file_type)) +
      (e.st.st_mode & S_IRUSR ? "r" : "-") +
      (e.st.st_mode & S_IWUSR ? "w" : "-") +
      (e.st.st_mode & S_IXUSR ? "x" : "-") +
      (e.st.st_mode & S_IRGRP ? "r" : "-") +
      (e.st.st_mode & S_IWGRP ? "w" : "-") +
      (e.st.st_mode & S_IXGRP ? "x" : "-") +
      (e.st.st_mode & S_IROTH ? "r" : "-") +
      (e.st.st_mode & S_IWOTH ? "w" : "-") +
      (e.st.st_mode & S_IXOTH ? "x" : "-");
    te.owner = get_owner(e.st.st_uid);
    te.group = get_group(e.st.st_gid);
  } else {
    te.file_type = FileType::Unknown;
    te.size = 0;
    te.perm_str = "??????????";
    te.owner = "?";
    te.group = "?";
    te.mtime_str = "?";
    te.mtime = 0;
  }
  return te;
}

std::vector<TuiEntry> tui_collect_entries(const char* dirpath) {
  std::vector<TuiEntry> result;
  std::vector<LsEntry> entries = ls_collect_entries(dirpath, 1, 1, 0);
  for (const auto& e : entries) {
    result.push_back(ls_entry_to_tui(e));
  }
  return result;
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

class LsfComponent : public TuiBase {
    std::string current_dir_;
    std::vector<TuiEntry> entries_;
    std::vector<TuiEntry> all_entries_;
    std::string status_msg_;
    bool quit_requested_ = false;
    bool open_requested_ = false;
    std::string open_path_;
    std::vector<std::string> nav_history_;
    int history_pos_ = -1;
    SortMode sort_mode_ = SortMode::Name;
    bool sort_reverse_ = false;
    enum class ConfirmMode { None, Delete } confirm_mode_ = ConfirmMode::None;
    std::string confirm_path_;

    void push_history(const std::string& dir) {
      if (!nav_history_.empty() && nav_history_.back() == dir) return;
      if (history_pos_ >= 0 && history_pos_ < (int)nav_history_.size() - 1) {
        nav_history_.resize(history_pos_ + 1);
      }
      nav_history_.push_back(dir);
      history_pos_ = (int)nav_history_.size() - 1;
    }

    void apply_sort() {
      std::sort(all_entries_.begin(), all_entries_.end(), [this](const TuiEntry& a, const TuiEntry& b) {
        bool cmp = false;
        switch (sort_mode_) {
          case SortMode::Size:
            if (a.size != b.size) { cmp = a.size > b.size; break; }
            break;
          case SortMode::Mtime:
            if (a.mtime != b.mtime) { cmp = a.mtime > b.mtime; break; }
            break;
          case SortMode::Type:
            if ((int)a.file_type != (int)b.file_type) { cmp = (int)a.file_type > (int)b.file_type; break; }
            break;
          default:
            break;
        }
        if (!cmp && sort_mode_ != SortMode::Name) cmp = a.display_name < b.display_name;
        else if (!cmp) cmp = a.display_name < b.display_name;
        return sort_reverse_ ? !cmp : cmp;
      });
    }

    void apply_filter() {
      entries_.clear();
      for (const auto& e : all_entries_) {
        if (search_query_.empty() || e.display_name.find(search_query_) != std::string::npos) {
          entries_.push_back(e);
        }
      }
      if (selected_ >= (int)entries_.size()) {
        selected_ = (int)entries_.size() - 1;
      }
    }

    ftxui::Element render_preview() const {
        using namespace ftxui;
        if (entries_.empty() || selected_ < 0 || selected_ >= (int)entries_.size()) {
            return text("No entries") | dim | center;
        }
        const auto& e = entries_[selected_];

        std::vector<Element> lines;
        lines.push_back(text(e.display_name) | bold | hcenter);
        lines.push_back(separator());

        lines.push_back(text("Perms: " + e.perm_str + "  Owner: " + e.owner + ":" + e.group));
        lines.push_back(text("Size: " + format_bytes(e.size) + "  Mtime: " + e.mtime_str));
        lines.push_back(separator());

        if (e.file_type == FileType::Directory) {
            auto children = tui_collect_entries(e.path.c_str());
            for (const auto& c : children) {
                std::string prefix = c.file_type == FileType::Directory ? "\xEF\x84\x95 " : "  ";
                lines.push_back(text(prefix + c.display_name));
            }
            if (children.empty()) {
                lines.push_back(text("(empty directory)") | dim);
            }
        } else if (e.file_type == FileType::Symlink) {
            char target[4096];
            ssize_t n = readlink(e.path.c_str(), target, sizeof(target) - 1);
            if (n >= 0) {
                target[n] = '\0';
                lines.push_back(text("Symlink \u2192 " + std::string(target)));
            }
            struct stat st;
            if (stat(e.path.c_str(), &st) == 0) {
                lines.push_back(text("Target type: " + std::string(
                    S_ISDIR(st.st_mode) ? "directory" : "file")));
            }
        } else {
            std::string preview = read_file_preview(e.path.c_str(), 200);
            auto lines_vec = split_lines(preview);
            std::vector<Element> preview_elements;
            for (const auto& l : lines_vec) {
                preview_elements.push_back(text(l));
            }
            lines.push_back(vbox(preview_elements) | flex | yframe);
        }

        return vbox(lines) | flex | xframe;
    }

public:
    explicit LsfComponent(std::string dir) : current_dir_(std::move(dir)) {}

     int entries_size() const override { return (int)entries_.size(); }

     int header_rows() const override { return 1; }

     Element render_row(int idx) const override {
        const auto& e = entries_[idx];
        std::string icon(icon_utf8(e.file_type));
        return text(icon + " " + e.display_name);
    }

    void fill_entries() override {
        all_entries_ = tui_collect_entries(current_dir_.c_str());
        apply_sort();
        apply_filter();
    }

    void on_search_input_changed() override {
        search_query_ = search_input_;
        apply_filter();
    }

    ftxui::Element OnRender() override {
        using namespace ftxui;

        if (entries_.empty()) {
            return text("(empty directory)") | dim | center;
        }

        Elements top;
        top.push_back(text("modbox \u2014 " + current_dir_) | bold | hcenter);
        top.push_back(separator());

        Element preview = render_preview();
        Elements body;
        body.push_back(hbox({
            render_list() | yframe | flex,
            preview,
        }) | flex);

        Elements footer;
        if (search_mode_) {
            footer.push_back(text("Filter: " + search_input_ + "\u2588") | dim | frame);
        } else if (confirm_mode_ == ConfirmMode::Delete) {
            footer.push_back(text("Delete " + confirm_path_ + "? (y/N)") | bold | color(Color::Red) | frame);
        } else {
 std::string footer_sort = sort_label(sort_mode_);
 if (sort_reverse_) footer_sort += " (rev)";
 footer.push_back(text(
 " j/k=nav h=up Enter=cd /=filter s=sort(" + footer_sort + ") c=copy o=open d=del q=quit"
 ) | dim | frame);
        }
        if (!status_msg_.empty()) {
            footer.push_back(text(status_msg_) | bold | center);
        }

        Elements all;
        all.push_back(vbox(std::move(top)));
        all.push_back(vbox(std::move(body)) | flex);
        all.push_back(separator());
        all.push_back(vbox(std::move(footer)));

        return vbox(std::move(all));
    }

    bool OnEvent(Event event) override {
        using namespace ftxui;

        if (search_mode_) {
            if (event == Event::Escape) {
                search_mode_ = false;
                search_input_.clear();
                search_query_.clear();
                fill_entries();
                update_scroll_math();
                return true;
            }
            if (event == Event::Return) {
                search_mode_ = false;
                search_query_ = search_input_;
                fill_entries();
                update_scroll_math();
                return true;
            }
            if (handle_search(event)) return true;
            return ComponentBase::OnEvent(event);
        }

        if (confirm_mode_ == ConfirmMode::Delete) {
            if (event == Event::Character('y') || event == Event::Character('Y')) {
                const auto& e = entries_[selected_];
                int rc;
                if (e.file_type == FileType::Directory) {
                    rc = rmdir(e.path.c_str());
                } else {
                    rc = unlink(e.path.c_str());
                }
                if (rc == 0) {
                    status_msg_ = "Deleted: " + e.display_name;
                } else {
                    status_msg_ = "Delete failed: " + std::string(strerror(errno));
                }
                confirm_mode_ = ConfirmMode::None;
                confirm_path_.clear();
                fill_entries();
            } else {
                confirm_mode_ = ConfirmMode::None;
                confirm_path_.clear();
                status_msg_.clear();
            }
            return true;
        }

        if (on_command_key(event)) return true;
        if (handle_nav(event)) return true;
        return ComponentBase::OnEvent(event);
    }

    bool on_command_key(ftxui::Event event) override {
        using namespace ftxui;

        if (event == Event::Character('q') || event == Event::Character('Q')) {
            quit_requested_ = true;
            return true;
        }
 if (event == Event::Character('h') || event == Event::Backspace) {
 std::string parent = current_dir_;
 size_t pos = parent.size();
 bool found = false;
 while (pos > 0) {
 if (parent[pos - 1] == '/') {
 if (pos == parent.size()) { pos--; continue; }
 parent = parent.substr(0, pos - 1);
 found = true;
 break;
 }
 pos--;
 }
 if (!found) parent = "/";
 if (parent != current_dir_) {
 push_history(current_dir_);
 current_dir_ = parent;
 fill_entries();
 selected_ = 0;
 }
 return true;
 }
        if (event == Event::Return) {
            if (selected_ >= 0 && selected_ < (int)entries_.size()) {
                const auto& e = entries_[selected_];
                if (e.file_type == FileType::Directory) {
                    push_history(current_dir_);
                    current_dir_ = e.path;
                    fill_entries();
                    selected_ = 0;
                }
            }
            return true;
        }
        if (event == Event::Character('u')) {
            if (history_pos_ > 0) {
                history_pos_--;
                current_dir_ = nav_history_[history_pos_];
                fill_entries();
                selected_ = 0;
            }
            return true;
        }
        if (event == Event::Character('U')) {
            if (history_pos_ < (int)nav_history_.size() - 1) {
                history_pos_++;
                current_dir_ = nav_history_[history_pos_];
                fill_entries();
                selected_ = 0;
            }
            return true;
        }
        if (event == Event::Character('/')) {
            search_mode_ = true;
            search_input_ = search_query_;
            return true;
        }
        if (event == Event::Character('s')) {
            sort_mode_ = (SortMode)(((int)sort_mode_ + 1) % 4);
            apply_sort();
            apply_filter();
            const char* labels[] = {"Name", "Size", "Mtime", "Type"};
            status_msg_ = std::string("Sort: ") + labels[(int)sort_mode_];
            return true;
        }
 if (event == Event::Character('S')) {
 sort_reverse_ = !sort_reverse_;
 apply_sort();
 apply_filter();
 status_msg_ = std::string("Sort: ") + (sort_reverse_ ? "reverse " : "") + sort_label(sort_mode_);
 return true;
 }
        if (event == Event::Character('o')) {
            if (selected_ >= 0 && selected_ < (int)entries_.size()) {
                const auto& e = entries_[selected_];
                if (e.file_type == FileType::Directory) {
                    push_history(current_dir_);
                    current_dir_ = e.path;
                    fill_entries();
                    selected_ = 0;
                } else {
                    const char* editor = getenv("EDITOR");
                    if (!editor) editor = getenv("PAGER");
                    if (!editor) editor = "cat";
                    open_requested_ = true;
                    open_path_ = e.path;
                }
            }
            return true;
        }
 if (event == Event::Character('d')) {
 if (selected_ >= 0 && selected_ < (int)entries_.size()) {
 confirm_mode_ = ConfirmMode::Delete;
 confirm_path_ = entries_[selected_].display_name;
 status_msg_ = "Delete " + confirm_path_ + "? (y/N)";
 }
 return true;
 }
 if (event == Event::Character('c')) {
 if (selected_ >= 0 && selected_ < (int)entries_.size()) {
 const auto& e = entries_[selected_];
 std::string abspath = current_dir_ + "/" + e.display_name;
 std::string osc = "\033]52;c;" + abspath + "\007";
 fputs(osc.c_str(), stdout);
 fflush(stdout);
 status_msg_ = "Copied: " + abspath;
 }
 return true;
 }
 return false;
}

    const std::string& current_dir() const { return current_dir_; }
    bool quit_requested() const { return quit_requested_; }
    bool open_requested() const { return open_requested_; }
    const std::string& open_path() const { return open_path_; }
};

void ls_tui_command(int argc, char** argv) {
  auto screen = App::FitComponent();
  auto component = std::make_shared<LsfComponent>(".");
  component->Refresh();
  screen.Loop(component);

  if (component->open_requested()) {
    const char* editor = getenv("EDITOR");
    if (!editor) editor = getenv("PAGER");
    if (!editor) editor = "cat";

    std::string path = component->open_path();
    pid_t pid = fork();
    if (pid == 0) {
      execlp(editor, editor, path.c_str(), (char*)nullptr);
      _exit(127);
    } else if (pid > 0) {
      int status = 0;
      waitpid(pid, &status, 0);
      printf("\033c");
      fflush(stdout);
      auto screen2 = App::FitComponent();
      auto component2 = std::make_shared<LsfComponent>(component->current_dir());
      component2->Refresh();
      screen2.Loop(component2);
    }
  }

  if (component->quit_requested()) {
    const char* cwd_file = std::getenv("HOME");
    if (cwd_file) {
      std::string path = std::string(cwd_file) + "/.cache/lf/cwd";
      std::error_code ec;
      std::filesystem::create_directories(std::string(cwd_file) + "/.cache/lf", ec);
      if (!ec) {
        FILE* f = fopen(path.c_str(), "w");
        if (f) {
          fprintf(f, "%s\n", component->current_dir().c_str());
          fclose(f);
        }
      }
    }
  }
}
