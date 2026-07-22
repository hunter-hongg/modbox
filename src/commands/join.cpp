#include <cstdio>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <vector>
#include <string>
#include <unordered_map>

#include "commands/join.hpp"

#define MAX_LINE 1048576
#define MAX_FIELDS 4096
#define FLD_BUF 4096

using Fields = std::vector<std::string>;

static bool fgetline(char* buf, int n, FILE* fp, int* has_newline) {
  if (fgets(buf, n, fp)) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
      buf[--len] = '\0';
      if (has_newline) *has_newline = 1;
    } else {
      if (has_newline) *has_newline = 0;
    }
    return true;
  }
  buf[0] = '\0';
  if (has_newline) *has_newline = 0;
  return false;
}

static int split(const char* s, char delim, Fields* out) {
  out->clear();
  const char* start = s;
  for (const char* p = s; ; p++) {
    if (*p == delim || *p == '\0') {
      out->push_back(std::string(start, p));
      if (*p == '\0') break;
      start = p + 1;
    }
  }
  return (int)out->size();
}

struct FileData {
  Fields fields;
  std::string key;
};

static void print_help(const char* prog) {
  printf("Usage: %s [OPTION]... FILE1 FILE2\n", prog);
  printf("For each pair of input lines with identical join fields, write a line to\n");
  printf("standard output. The default join field is the first field.\n");
  printf("\n");
  printf("  -1, --field1=FIELD        join on this FIELD of file 1 (default 1)\n");
  printf("  -2, --field2=FIELD        join on this FIELD of file 2 (default 1)\n");
  printf("  -t, --separator=CHAR      use CHAR as input and output field separator\n");
  printf("  -e, --empty=EMPTY         replace missing input fields with EMPTY\n");
  printf("  -i, --ignore-case         ignore differences in case when comparing\n");
  printf("  -a, --auto=FILE-NUM       print unpairable lines from file FILE-NUM, both by default\n");
  printf("  -v, --verbose=FILE-NUM    like -a but print pairable lines instead\n");
  printf("      --help                display this help and exit\n");
  printf("      --version             output version information and exit\n");
}

static int read_all(
  FILE* fp,
  std::vector<FileData>* out,
  int join_idx,
  char delim,
  bool ignore_case,
  std::unordered_map<std::string, int>* index,
  bool is_file2,
  const int auto_file,
  const int verb_file,
  int* eof_count
) {
  char line_buf[MAX_LINE];
  int has_nl;
  int line_num = 0;
  while (fgetline(line_buf, MAX_LINE, fp, &has_nl)) {
    if (line_buf[0] == '\0' && !has_nl) break;
    FileData fd;
    split(line_buf, delim, &fd.fields);
    if ((int)fd.fields.size() < join_idx) {
      fd.key = "";
    } else {
      fd.key = fd.fields[join_idx - 1];
      if (ignore_case) {
        for (size_t k = 0; k < fd.key.size(); k++) {
          fd.key[k] = (char)std::tolower((unsigned char)fd.key[k]);
        }
      }
    }
    int show = 0;
    if (verb_file == 0) show = 1;
    else if (verb_file == 1 && !is_file2) show = 1;
    else if (verb_file == 2 && is_file2) show = 1;

    if (show && auto_file > 0) {
      int unmatched = 1;
      if (index) {
        auto it = index->find(fd.key);
        if (it != index->end()) unmatched = 0;
      }
      if ((verb_file == 1 && !is_file2 && unmatched) ||
          (verb_file == 2 && is_file2 && unmatched)) {
        printf("%s\n", line_buf);
      }
    } else {
      out->push_back(std::move(fd));
      if (index && verb_file == 0) {
        (*index)[line_buf[0] == '\0' ? "" : fd.fields[join_idx - 1]] = line_num;
      }
    }
    line_num++;
  }
  *eof_count = line_num;
  return 0;
}

static void do_join(const char* p1, const char* p2, const JoinOptions* opts, bool ignore_case, int auto_file, const char* empty_str, int verb_file) {
  FILE* f1 = (!p1 || strcmp(p1, "-") == 0) ? stdin : fopen(p1, "r");
  FILE* f2 = (!p2 || strcmp(p2, "-") == 0) ? stdin : fopen(p2, "r");
  if (!f1) { fprintf(stderr, "join: %s: %s\n", p1 ? p1 : "stdin", strerror(errno)); return; }
  if (!f2) { fprintf(stderr, "join: %s: %s\n", p2 ? p2 : "stdin", strerror(errno)); if (f1 != stdin) fclose(f1); return; }

  std::vector<FileData> lines1, lines2;
  std::unordered_map<std::string, std::vector<int>> map2;
  int n2;
  int j1 = opts->file1_field < 1 ? 1 : opts->file1_field;
  int j2 = opts->file2_field < 1 ? 1 : opts->file2_field;

  char line_buf[MAX_LINE];
  int has_nl;
  while (fgetline(line_buf, MAX_LINE, f2, &has_nl)) {
    if (line_buf[0] == '\0' && !has_nl && feof(f2)) break;
    FileData fd;
    split(line_buf, opts->delim, &fd.fields);
    if ((int)fd.fields.size() < j2) fd.key = "";
    else fd.key = fd.fields[j2 - 1];
    if (ignore_case) {
      for (size_t k = 0; k < fd.key.size(); k++) {
        fd.key[k] = (char)std::tolower((unsigned char)fd.key[k]);
      }
    }
    map2[fd.key].push_back((int)lines2.size());
    lines2.push_back(std::move(fd));
  }
  n2 = (int)lines2.size();

  if (f2 != stdin) fclose(f2);

  while (fgetline(line_buf, MAX_LINE, f1, &has_nl)) {
    if (line_buf[0] == '\0' && !has_nl && feof(f1)) break;
    FileData fd;
    split(line_buf, opts->delim, &fd.fields);
    std::string key;
    if ((int)fd.fields.size() < j1) key = "";
    else key = fd.fields[j1 - 1];
    if (ignore_case) {
      for (size_t k = 0; k < key.size(); k++) {
        key[k] = (char)std::tolower((unsigned char)key[k]);
      }
    }

    auto it = map2.find(key);
    if (it != map2.end()) {
      for (int idx : it->second) {
        const FileData& b = lines2[idx];
        const Fields& a_f = fd.fields;
        const Fields& b_f = b.fields;
        int na = (int)a_f.size();
        int nb = (int)b_f.size();

        printf("%s", line_buf);
        for (int i = 0; i < nb; i++) {
          if (i + 1 == j2) continue;
          printf("%c%s", opts->delim, i < nb ? b_f[i].c_str() : empty_str);
        }
        printf("\n");
      }
      map2.erase(it);
    } else if (verb_file > 0) {
      printf("%s\n", line_buf);
    }
  }

  if (auto_file == 0) {
    for (const auto& kv : map2) {
      for (int idx : kv.second) {
        const FileData& b = lines2[idx];
        printf("%s", empty_str);
        for (int i = 0; i < (int)b.fields.size(); i++) {
          if (i + 1 != j2) printf("%c%s", opts->delim, b.fields[i].c_str());
        }
        printf("\n");
      }
    }
  }

  if (f1 != stdin) fclose(f1);
}

void join_command(int argc, char** argv) {
  struct JoinOptions opts = {1, ' ', 1, 1};
  bool ignore_case = false;
  int auto_file = 0;
  const char* empty_str = "";
  const char* f1 = nullptr, *f2 = nullptr;
  int verb_file = 0;

  int i = 1;
  for (; i < argc; i++) {
    const char* a = argv[i];
    if (strcmp(a, "--help") == 0) { print_help(argv[0]); return; }
    if (strcmp(a, "--version") == 0) { printf("join (modbox) 1.0\n"); return; }
    if (strcmp(a, "-i") == 0 || strcmp(a, "--ignore-case") == 0) { ignore_case = true; continue; }
    if (strcmp(a, "-1") == 0 && i + 1 < argc) { opts.file1_field = std::atoi(argv[++i]); continue; }
    if (strcmp(a, "-2") == 0 && i + 1 < argc) { opts.file2_field = std::atoi(argv[++i]); continue; }
    if (strcmp(a, "-t") == 0 && i + 1 < argc) { opts.delim = argv[++i][0]; continue; }
    if (strcmp(a, "-e") == 0 && i + 1 < argc) { empty_str = argv[++i]; continue; }
    if (strcmp(a, "-a") == 0 && i + 1 < argc) { auto_file = std::atoi(argv[++i]); continue; }
    if (strncmp(a, "-a", 2) == 0 && a[2]) { auto_file = std::atoi(a + 2); continue; }
    if (strcmp(a, "-v") == 0 && i + 1 < argc) { verb_file = std::atoi(argv[++i]); continue; }
    if (strncmp(a, "-v", 2) == 0 && a[2]) { verb_file = std::atoi(a + 2); continue; }
    if (a[0] == '-' && a[1] == '-' && a[2] == '\0') { i++; break; }
    if (a[0] == '-' && a[1] != '-' && a[1] != '\0' && a[1] == opts.delim) { f1 = "-"; continue; }
    if (a[0] == '-' && a[1] == opts.delim) { f1 = "-"; continue; }
    if (a[0] == '-' && a[1] != '-') { f1 = "-"; }
    if (!f1) { f1 = a; } else if (!f2) { f2 = a; } else { fprintf(stderr, "join: too many arguments\n"); return; }
  }
  for (; i < argc; i++) {
    if (!f1) f1 = argv[i];
    else if (!f2) f2 = argv[i];
    else { fprintf(stderr, "join: too many arguments\n"); return; }
  }
  if (!f2) { fprintf(stderr, "join: missing operand\n"); return; }
  do_join(f1, f2, &opts, ignore_case, auto_file, empty_str, verb_file);
}
