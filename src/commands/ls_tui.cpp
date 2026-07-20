#include "commands/ls_tui.hpp"
#include "commands/ls.hpp"

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
  int selected;
  std::string filter;
  std::string status_msg;
  int filtering;
  bool quit_requested;
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

static void build_entry(TuiEntry& e, const char* full_path, const char* dname) {
  e.path = full_path;
  e.display_name = dname;
  struct stat st;
  if (lstat(full_path, &st) == 0) {
    e.is_dir = S_ISDIR(st.st_mode);
    e.is_symlink = S_ISLNK(st.st_mode);
    e.is_executable = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
    e.size = (uint64_t)st.st_size;
    e.perm_str = std::string(
      S_ISDIR(st.st_mode) ? "d" : S_ISLNK(st.st_mode) ? "l" : "-") +
      (st.st_mode & S_IRUSR ? "r" : "-") +
      (st.st_mode & S_IWUSR ? "w" : "-") +
      (st.st_mode & S_IXUSR ? "x" : "-") +
      (st.st_mode & S_IRGRP ? "r" : "-") +
      (st.st_mode & S_IWGRP ? "w" : "-") +
      (st.st_mode & S_IXGRP ? "x" : "-") +
      (st.st_mode & S_IROTH ? "r" : "-") +
      (st.st_mode & S_IWOTH ? "w" : "-") +
      (st.st_mode & S_IXOTH ? "x" : "-");
    e.owner = get_owner(st.st_uid);
    e.group = get_group(st.st_gid);
    e.mtime_str = fmt_time(st.st_mtime);
  } else {
    e.is_dir = 0;
    e.is_symlink = 0;
    e.is_executable = 0;
    e.size = 0;
    e.perm_str = "??????????";
    e.owner = "?";
    e.group = "?";
    e.mtime_str = "?";
  }
}

std::vector<TuiEntry> tui_collect_entries(const char* dirpath) {
  std::vector<TuiEntry> result;
  DIR* dir = opendir(dirpath);
  if (!dir) return result;

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0) continue;
    if (strcmp(entry->d_name, "..") == 0) continue;

    char full_path[4096];
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);
    TuiEntry e;
    build_entry(e, full_path, entry->d_name);
    result.push_back(std::move(e));
  }
  closedir(dir);
  return result;
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
    body.push_back(hbox({
      vbox(left) | vscroll_indicator | yframe | flex,
      vbox(right) | flex | xframe,
    }) | flex);

    Elements footer;
    footer.push_back(text("  j/k=nav  Enter=cd  q=quit") | dim | frame);

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
          ctx_.entries = tui_collect_entries(ctx_.current_dir.c_str());
          ctx_.selected = 0;
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

  (void)ctx.quit_requested;
}
