#include "commands/ls_tui.hpp"
#include "commands/ls.hpp"
#include "commands/ls_entry.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>

using namespace ftxui;

struct TuiCtx {
  std::string current_dir;
  std::vector<TuiEntry> entries;
  std::vector<TuiEntry> all_entries;
  int selected;
  std::string filter;
  std::string status_msg;
  int filtering;
  bool quit_requested;
  bool open_requested;
  std::string open_path;
};

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

static std::string fmt_size(uint64_t sz) {
  if (sz >= 1024ULL * 1024ULL * 1024ULL) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1fG", (double)sz / (1024.0 * 1024.0 * 1024.0));
    return buf;
  } else if (sz >= 1024ULL * 1024ULL) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1fM", (double)sz / (1024.0 * 1024.0));
    return buf;
  } else if (sz >= 1024ULL) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1fK", (double)sz / 1024.0);
    return buf;
  }
  return std::to_string(sz);
}

static TuiEntry ls_entry_to_tui(const LsEntry& e) {
  TuiEntry te;
  te.path = e.path;
  te.display_name = e.display_name;
  if (e.st.st_mode != 0 || S_ISREG(e.st.st_mode) || S_ISDIR(e.st.st_mode)) {
    te.is_dir = S_ISDIR(e.st.st_mode);
    te.is_symlink = S_ISLNK(e.st.st_mode);
    te.is_executable = (e.st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
    te.size = (uint64_t)e.st.st_size;
    te.perm_str = std::string(
      S_ISDIR(e.st.st_mode) ? "d" : S_ISLNK(e.st.st_mode) ? "l" : "-") +
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
    te.mtime_str = fmt_time(e.st.st_mtime);
  } else {
    te.is_dir = 0;
    te.is_symlink = 0;
    te.is_executable = 0;
    te.size = 0;
    te.perm_str = "??????????";
    te.owner = "?";
    te.group = "?";
    te.mtime_str = "?";
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

static Element render_preview(TuiCtx& ctx) {
    if (ctx.entries.empty()) {
        return text("No entries") | dim | center;
    }
    const auto& e = ctx.entries[ctx.selected];

    std::vector<Element> lines;
    lines.push_back(text(e.display_name) | bold | hcenter);
    lines.push_back(separator());

    lines.push_back(text("Perms: " + e.perm_str + "  Owner: " + e.owner + ":" + e.group));
    lines.push_back(text("Size: " + fmt_size(e.size) + "  Mtime: " + e.mtime_str));
    lines.push_back(separator());

    if (e.is_dir) {
        auto children = tui_collect_entries(e.path.c_str());
        for (const auto& c : children) {
            std::string prefix = c.is_dir ? "\xEF\x84\x95 " : "  ";
            lines.push_back(text(prefix + c.display_name));
        }
        if (children.empty()) {
            lines.push_back(text("(empty directory)") | dim);
        }
    } else if (e.is_symlink) {
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

static std::vector<TuiEntry> filter_entries(const std::vector<TuiEntry>& src, const std::string& q) {
    if (q.empty()) return src;
    std::vector<TuiEntry> out;
    for (const auto& e : src) {
        if (e.display_name.find(q) != std::string::npos) {
            out.push_back(e);
        }
    }
    return out;
}

class LsfComponent : public ComponentBase {
  TuiCtx& ctx_;

public:
  explicit LsfComponent(TuiCtx& ctx) : ctx_(ctx) {}

  Element OnRender() override {
    if (ctx_.entries.empty()) {
      return text("(empty directory)") | dim | center;
    }

    Elements left;
    for (size_t i = 0; i < ctx_.entries.size(); i++) {
      const auto& e = ctx_.entries[i];
      std::string prefix;
      if (e.is_dir) {
        prefix = "\xEF\x84\x95 ";
      } else if (e.is_executable) {
        prefix = "\xEF\x92\x89 ";
      } else {
        prefix = "\xEF\x80\x96 ";
      }
      std::string name = prefix + e.display_name;
      if ((int)i == ctx_.selected) {
        left.push_back(text(name) | inverted);
      } else {
        left.push_back(text(name));
      }
    }

    const auto& sel = ctx_.entries[ctx_.selected];
    Elements right;
    right.push_back(text(sel.display_name) | bold | hcenter);
    right.push_back(separator());
    right.push_back(text("Perms: " + sel.perm_str + "  Owner: " + sel.owner + ":" + sel.group));
    right.push_back(text("Size: " + fmt_size(sel.size) + "  Mtime: " + sel.mtime_str));
    right.push_back(separator());
    right.push_back(text("Preview pane (coming soon)") | dim);

    Elements body;
    Element preview = render_preview(ctx_);
    body.push_back(hbox({
      vbox(left) | vscroll_indicator | yframe | flex,
      preview,
    }) | flex);

    Elements footer;
    if (ctx_.filtering) {
      footer.push_back(text("Filter: " + ctx_.filter + "█") | dim | frame);
    } else {
      footer.push_back(text("  j/k=nav  Enter=cd  /=filter  o=open  c=copy  d=del  q=quit") | dim | frame);
    }
    if (!ctx_.status_msg.empty()) {
      footer.push_back(text(ctx_.status_msg) | bold | center);
    }

    Elements top;
    top.push_back(text("modbox — " + ctx_.current_dir) | bold | hcenter);
    top.push_back(separator());

    Elements all;
    all.push_back(vbox(std::move(top)));
    all.push_back(vbox(std::move(body)) | flex);
    all.push_back(separator());
    all.push_back(vbox(std::move(footer)));

    return vbox(std::move(all));
  }

  bool OnEvent(Event event) override {
    if (ctx_.filtering) {
      if (event == Event::Character('q') || event == Event::Escape) {
        ctx_.filtering = 0;
        ctx_.filter.clear();
        ctx_.entries = ctx_.all_entries;
        return true;
      }
      if (event == Event::Backspace) {
        if (!ctx_.filter.empty()) {
          ctx_.filter.pop_back();
          ctx_.entries = filter_entries(ctx_.all_entries, ctx_.filter);
          if (ctx_.selected >= (int)ctx_.entries.size()) {
            ctx_.selected = (int)ctx_.entries.size() - 1;
          }
        }
        return true;
      }
      if (event.is_character()) {
        std::string ch = event.character();
        if (!ch.empty() && (unsigned char)ch[0] >= 32 && (unsigned char)ch[0] < 127) {
          ctx_.filter += ch[0];
          ctx_.entries = filter_entries(ctx_.all_entries, ctx_.filter);
          if (ctx_.selected >= (int)ctx_.entries.size()) {
            ctx_.selected = (int)ctx_.entries.size() - 1;
          }
        }
        return true;
      }
    }

    if (event == Event::ArrowUp || event == Event::Character('k')) {
      if (ctx_.selected > 0) ctx_.selected--;
      return true;
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      if (ctx_.selected < (int)ctx_.entries.size() - 1) ctx_.selected++;
      return true;
    }
    if (event == Event::Return) {
      if (ctx_.selected >= 0 && ctx_.selected < (int)ctx_.entries.size()) {
        const auto& e = ctx_.entries[ctx_.selected];
        if (e.is_dir) {
          ctx_.current_dir = e.path;
          ctx_.all_entries = tui_collect_entries(ctx_.current_dir.c_str());
          ctx_.entries = ctx_.all_entries;
          ctx_.selected = 0;
          ctx_.filter.clear();
        }
      }
      return true;
    }
    if (event == Event::Character('/')) {
      ctx_.filtering = !ctx_.filtering;
      return true;
    }
    if (event == Event::Character('o')) {
      if (ctx_.selected >= 0 && ctx_.selected < (int)ctx_.entries.size()) {
        const auto& e = ctx_.entries[ctx_.selected];
        ctx_.open_requested = true;
        ctx_.open_path = e.path;
      }
      return true;
    }
    if (event == Event::Character('c')) {
      if (ctx_.selected >= 0 && ctx_.selected < (int)ctx_.entries.size()) {
        const auto& e = ctx_.entries[ctx_.selected];
        std::string abspath = ctx_.current_dir + "/" + e.display_name;
        std::string osc = "\033]52;c;" + abspath + "\007";
        fputs(osc.c_str(), stdout);
        fflush(stdout);
        ctx_.status_msg = "Copied: " + abspath;
      }
      return true;
    }
    if (event == Event::Character('d')) {
      if (ctx_.selected >= 0 && ctx_.selected < (int)ctx_.entries.size()) {
        const auto& e = ctx_.entries[ctx_.selected];
        int rc;
        if (e.is_dir) {
          rc = rmdir(e.path.c_str());
        } else {
          rc = unlink(e.path.c_str());
        }
        if (rc == 0) {
          ctx_.all_entries.erase(ctx_.all_entries.begin() + ctx_.selected);
          ctx_.entries = ctx_.all_entries;
          if (ctx_.selected >= (int)ctx_.entries.size()) {
            ctx_.selected = (int)ctx_.entries.size() - 1;
          }
          ctx_.status_msg = "Deleted: " + e.display_name;
        } else {
          ctx_.status_msg = "Delete failed: " + std::string(strerror(errno));
        }
      }
      return true;
    }
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      ctx_.quit_requested = true;
      return true;
    }
    return ComponentBase::OnEvent(event);
  }
};

void ls_tui_command(int argc, char** argv, const LsOptions* opts) {
  (void)argc;
  (void)argv;
  (void)opts;

  TuiCtx ctx;
  ctx.current_dir = ".";
  ctx.selected = 0;
  ctx.entries = tui_collect_entries(ctx.current_dir.c_str());
  ctx.quit_requested = false;

  auto screen = App::FitComponent();
  auto component = std::make_shared<LsfComponent>(ctx);
  screen.Loop(component);

  if (ctx.open_requested) {
    const char* editor = getenv("EDITOR");
    if (!editor) editor = getenv("PAGER");
    if (!editor) editor = "cat";
    std::string cmd = std::string(editor) + " " + ctx.open_path;
    printf("\033c");
    fflush(stdout);
    int rc = system(cmd.c_str());
    (void)rc;
  }

  if (ctx.quit_requested) {
    printf("%s\n", ctx.current_dir.c_str());
  }
}
