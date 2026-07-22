#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <argtable3.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <cerrno>

#include "commands/stat.hpp"
#include "commands/command_macros.hpp"

namespace {

/* Permission bits mask (mode & 07777) for the %a conversion. */
constexpr mode_t PERMISSION_MASK = 07777;
/* Number of digits emitted for fractional nanoseconds in time strings. */
constexpr int NSEC_DIGITS = 9;

/* Common filesystem magic numbers (used by %t/%T in filesystem mode). */
#define FS_MAGIC_EXT2     0xEF53u
#define FS_MAGIC_TMPFS    0x01021997u
#define FS_MAGIC_PROC     0x9fa1u
#define FS_MAGIC_SYSFS    0x62656572u
#define FS_MAGIC_DEVTMPFS 0x09041934u
#define FS_MAGIC_OVERLAY  0x794c7630u
#define FS_MAGIC_BTRFS    0x9123683Eu
#define FS_MAGIC_XFS      0x58465342u
#define FS_MAGIC_VFAT     0x4d44u
#define FS_MAGIC_NFS      0x6969u
#define FS_MAGIC_CGROUP   0x27e0ebu
#define FS_MAGIC_CGROUP2  0x63677270u
#define FS_MAGIC_FUSEBLK  0x65735546u
#define FS_MAGIC_FUSECTL  0x65735543u
#define FS_MAGIC_RAMFS    0x858458f6u
#define FS_MAGIC_ISOFS    0x9660u
#define FS_MAGIC_NTFS     0x5346544eu
#define FS_MAGIC_ZFS      0x2fc12fc1u

/* Default (verbose) output format for a regular file. */
#define DEFAULT_FILE_FORMAT \
  "  File: %N\n" \
  "  Size: %-10s\tBlocks: %-10b IO Block: %-6o %F\n" \
  "Device: %Dh/%dd\tInode: %-10i  Links: %-5h\n" \
  "Access: (%04a/%10.10A)  Uid: (%5u/%8U)   Gid: (%5g/%8G)\n" \
  "Access: %x\n" \
  "Modify: %y\n" \
  "Change: %z\n" \
  " Birth: %w\n"

/* Default (verbose) output format for a filesystem. */
#define DEFAULT_FS_FORMAT \
  "  File: \"%n\"\n" \
  "    ID: %-8i Namelen: %-7l    Type: %T\n" \
  "Block Size: %-10s Fundamental Block Size: %-10S\n" \
  "Blocks: Total: %-10b Free: %-10f Available: %-10a Inodes: Total: %-10c Free: %-10d\n"

/* Default terse output formats (used with -t). */
#define TERSE_FILE_FORMAT "%n %s %b %f %u %g %D %i %h %t %T %X %Y %Z %o\n"
#define TERSE_FS_FORMAT   "%n %i %l %t %s %S %b %f %a %c %d\n"

struct FileStat {
  bool valid = false;
  mode_t mode = 0;
  nlink_t nlink = 0;
  uid_t uid = 0;
  gid_t gid = 0;
  off_t size = 0;
  dev_t dev = 0;
  dev_t rdev = 0;
  ino_t ino = 0;
  blkcnt_t blocks = 0;
  blksize_t blksize = 0;
  struct timespec atime {};
  struct timespec mtime {};
  struct timespec ctime {};
  struct timespec btime {};
  bool has_btime = false;
};

struct FsStat {
  bool valid = false;
  unsigned long bsize = 0;
  unsigned long frsize = 0;
  unsigned long long blocks = 0;
  unsigned long long bfree = 0;
  unsigned long long bavail = 0;
  unsigned long long files = 0;
  unsigned long long ffree = 0;
  unsigned long namemax = 0;
  uint64_t fsid = 0;
  unsigned long f_type = 0;
};

struct StatCtx {
  bool fs_mode = false;
  std::string name;
  bool is_link = false;
  std::string link_target;
  FileStat fst;
  FsStat fss;
};

std::string to_octal(unsigned long v) {
  if (v == 0) {
    return "0";
  }
  std::string s;
  while (v != 0) {
    s += static_cast<char>('0' + (v % 8));
    v /= 8;
  }
  std::ranges::reverse(s);
  return s;
}

std::string to_hex(unsigned long long v) {
  if (v == 0) {
    return "0";
  }
  static const char* digits = "0123456789abcdef";
  std::string s;
  while (v != 0) {
    s += digits[v % 16];
    v /= 16;
  }
  std::ranges::reverse(s);
  return s;
}

std::string nsec9(long ns) {
  std::string s(static_cast<size_t>(NSEC_DIGITS), '0');
  long v = ns;
  for (int k = NSEC_DIGITS - 1; k >= 0; k--) {
    s[static_cast<size_t>(k)] = static_cast<char>('0' + (v % 10));
    v /= 10;
  }
  return s;
}

std::string perm_string(mode_t mode) {
  std::string s(10, '-');
  if (S_ISDIR(mode)) {
    s[0] = 'd';
  } else if (S_ISLNK(mode)) {
    s[0] = 'l';
  } else if (S_ISCHR(mode)) {
    s[0] = 'c';
  } else if (S_ISBLK(mode)) {
    s[0] = 'b';
  } else if (S_ISSOCK(mode)) {
    s[0] = 's';
  } else if (S_ISFIFO(mode)) {
    s[0] = 'p';
  }
  s[1] = (mode & S_IRUSR) ? 'r' : '-';
  s[2] = (mode & S_IWUSR) ? 'w' : '-';
  s[3] = (mode & S_IXUSR) ? 'x' : '-';
  s[4] = (mode & S_IRGRP) ? 'r' : '-';
  s[5] = (mode & S_IWGRP) ? 'w' : '-';
  s[6] = (mode & S_IXGRP) ? 'x' : '-';
  s[7] = (mode & S_IROTH) ? 'r' : '-';
  s[8] = (mode & S_IWOTH) ? 'w' : '-';
  s[9] = (mode & S_IXOTH) ? 'x' : '-';
  if (mode & S_ISUID) {
    s[3] = (mode & S_IXUSR) ? 's' : 'S';
  }
  if (mode & S_ISGID) {
    s[6] = (mode & S_IXGRP) ? 's' : 'S';
  }
  if (mode & S_ISVTX) {
    s[9] = (mode & S_IXOTH) ? 't' : 'T';
  }
  return s;
}

std::string file_type_string(mode_t mode, off_t size) {
  if (S_ISREG(mode)) {
    return (size == 0) ? "regular empty file" : "regular file";
  }
  if (S_ISDIR(mode)) {
    return "directory";
  }
  if (S_ISLNK(mode)) {
    return "symbolic link";
  }
  if (S_ISCHR(mode)) {
    return "character special file";
  }
  if (S_ISBLK(mode)) {
    return "block special file";
  }
  if (S_ISFIFO(mode)) {
    return "fifo";
  }
  if (S_ISSOCK(mode)) {
    return "socket";
  }
  return "unknown";
}

std::string quote_name(const std::string& n) {
  return "'" + n + "'";
}

std::string dir_name(const std::string& p) {
  if (p.empty()) {
    return ".";
  }
  size_t pos = p.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0) {
    return "/";
  }
  return p.substr(0, pos);
}

std::string mount_point(const std::string& path) {
  struct stat st;
  if (lstat(path.c_str(), &st) != 0) {
    return "?";
  }
  dev_t dev = st.st_dev;
  std::string cur = path;
  if (!S_ISDIR(st.st_mode)) {
    cur = dir_name(cur);
    if (lstat(cur.c_str(), &st) != 0) {
      return cur;
    }
    dev = st.st_dev;
  }
  while (true) {
    std::string parent = dir_name(cur);
    if (parent == cur) {
      break;
    }
    struct stat pst;
    if (lstat(parent.c_str(), &pst) != 0) {
      break;
    }
    if (pst.st_dev != dev) {
      break;
    }
    cur = parent;
  }
  return cur;
}

std::string human_time(const struct timespec& ts) {
  struct tm tmres;
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (localtime_r(&ts.tv_sec, &tmres) == NULL) {
    return "-";
  }
  char buf[64];
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmres);
  std::string s(buf);
  if (ts.tv_nsec != 0) {
    s += "." + nsec9(ts.tv_nsec);
  }
  char zbuf[16];
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)strftime(zbuf, sizeof(zbuf), " %z", &tmres);
  s += zbuf;
  return s;
}

const struct {
  unsigned long magic;
  const char* name;
} fs_magic_names[] = {
  { .magic = FS_MAGIC_EXT2,     .name = "ext2/ext3" },
  { .magic = FS_MAGIC_TMPFS,    .name = "tmpfs" },
  { .magic = FS_MAGIC_PROC,     .name = "proc" },
  { .magic = FS_MAGIC_SYSFS,    .name = "sysfs" },
  { .magic = FS_MAGIC_DEVTMPFS, .name = "devtmpfs" },
  { .magic = FS_MAGIC_OVERLAY,  .name = "overlay" },
  { .magic = FS_MAGIC_BTRFS,    .name = "btrfs" },
  { .magic = FS_MAGIC_XFS,      .name = "xfs" },
  { .magic = FS_MAGIC_VFAT,     .name = "vfat" },
  { .magic = FS_MAGIC_NFS,      .name = "nfs" },
  { .magic = FS_MAGIC_CGROUP,   .name = "cgroup" },
  { .magic = FS_MAGIC_CGROUP2,  .name = "cgroup2" },
  { .magic = FS_MAGIC_FUSEBLK,  .name = "fuseblk" },
  { .magic = FS_MAGIC_FUSECTL,  .name = "fusectl" },
  { .magic = FS_MAGIC_RAMFS,    .name = "ramfs" },
  { .magic = FS_MAGIC_ISOFS,    .name = "isofs" },
  { .magic = FS_MAGIC_NTFS,     .name = "ntfs" },
  { .magic = FS_MAGIC_ZFS,      .name = "zfs" },
  { .magic = 0,                 .name = NULL },
};

std::string fs_type_name(unsigned long magic) {
  for (size_t k = 0; fs_magic_names[k].magic != 0; k++) {
    if (fs_magic_names[k].magic == magic) {
      return fs_magic_names[k].name;
    }
  }
  return to_hex(static_cast<unsigned long long>(magic));
}

FileStat do_stat_file(const char* path, bool deref) {
  FileStat st;
#ifdef __linux__
  struct statx stx;
  int flags = AT_STATX_SYNC_AS_STAT | (deref ? 0 : AT_SYMLINK_NOFOLLOW);
  if (statx(AT_FDCWD, path, flags, STATX_ALL, &stx) == 0) {
    st.valid = true;
    st.mode = static_cast<mode_t>(stx.stx_mode);
    st.nlink = static_cast<nlink_t>(stx.stx_nlink);
    st.uid = static_cast<uid_t>(stx.stx_uid);
    st.gid = static_cast<gid_t>(stx.stx_gid);
    st.size = static_cast<off_t>(stx.stx_size);
    st.dev = makedev(stx.stx_dev_major, stx.stx_dev_minor);
    st.rdev = makedev(stx.stx_rdev_major, stx.stx_rdev_minor);
    st.ino = static_cast<ino_t>(stx.stx_ino);
    st.blocks = static_cast<blkcnt_t>(stx.stx_blocks);
    st.blksize = static_cast<blksize_t>(stx.stx_blksize);
    st.atime = { .tv_sec = static_cast<time_t>(stx.stx_atime.tv_sec),
                 .tv_nsec = static_cast<long>(stx.stx_atime.tv_nsec) };
    st.mtime = { .tv_sec = static_cast<time_t>(stx.stx_mtime.tv_sec),
                 .tv_nsec = static_cast<long>(stx.stx_mtime.tv_nsec) };
    st.ctime = { .tv_sec = static_cast<time_t>(stx.stx_ctime.tv_sec),
                 .tv_nsec = static_cast<long>(stx.stx_ctime.tv_nsec) };
    if ((stx.stx_mask & STATX_BTIME) != 0) {
      st.has_btime = true;
      st.btime = { .tv_sec = static_cast<time_t>(stx.stx_btime.tv_sec),
                   .tv_nsec = static_cast<long>(stx.stx_btime.tv_nsec) };
    }
    return st;
  }
#endif
  struct stat s;
  int r = deref ? stat(path, &s) : lstat(path, &s);
  if (r != 0) {
    return st;
  }
  st.valid = true;
  st.mode = s.st_mode;
  st.nlink = s.st_nlink;
  st.uid = s.st_uid;
  st.gid = s.st_gid;
  st.size = s.st_size;
  st.dev = s.st_dev;
  st.rdev = s.st_rdev;
  st.ino = s.st_ino;
  st.blocks = s.st_blocks;
  st.blksize = s.st_blksize;
  st.atime = s.st_atim;
  st.mtime = s.st_mtim;
  st.ctime = s.st_ctim;
  return st;
}

FsStat do_stat_fs(const char* path) {
  FsStat r;
  struct statvfs vfs;
  if (statvfs(path, &vfs) != 0) {
    return r;
  }
  r.valid = true;
  r.bsize = vfs.f_bsize;
  r.frsize = vfs.f_frsize;
  r.blocks = static_cast<unsigned long long>(vfs.f_blocks);
  r.bfree = static_cast<unsigned long long>(vfs.f_bfree);
  r.bavail = static_cast<unsigned long long>(vfs.f_bavail);
  r.files = static_cast<unsigned long long>(vfs.f_files);
  r.ffree = static_cast<unsigned long long>(vfs.f_ffree);
  r.namemax = vfs.f_namemax;
  struct statfs sfs;
  if (statfs(path, &sfs) == 0) {
    r.f_type = static_cast<unsigned long>(sfs.f_type);
    uint64_t hi = static_cast<uint64_t>(static_cast<uint32_t>(sfs.f_fsid.__val[0]));
    uint64_t lo = static_cast<uint64_t>(static_cast<uint32_t>(sfs.f_fsid.__val[1]));
    r.fsid = (hi << 32) | lo;
  }
  return r;
}

std::string conv_value(char c, bool fs_mode, const StatCtx& ctx,
                       bool& is_str) {
  if (fs_mode) {
    const FsStat& f = ctx.fss;
    switch (c) {
    case 'n': is_str = true;  return ctx.name;
    case 'i': is_str = false; return to_hex(f.fsid);
    case 'l': is_str = false; return std::to_string(f.namemax);
    case 't': is_str = false; return to_hex(static_cast<unsigned long long>(f.f_type));
    case 'T': is_str = true;  return fs_type_name(f.f_type);
    case 's': is_str = false; return std::to_string(f.bsize);
    case 'S': is_str = false; return std::to_string(f.frsize);
    case 'b': is_str = false; return std::to_string(f.blocks);
    case 'f': is_str = false; return std::to_string(f.bfree);
    case 'a': is_str = false; return std::to_string(f.bavail);
    case 'c': is_str = false; return std::to_string(f.files);
    case 'd': is_str = false; return std::to_string(f.ffree);
    default:  is_str = true;  return "";
    }
  }
  const FileStat& s = ctx.fst;
  switch (c) {
  case 'n': is_str = true;  return ctx.name;
  case 'N': {
    is_str = true;
    std::string r = quote_name(ctx.name);
    if (ctx.is_link) {
      r += " -> " + quote_name(ctx.link_target);
    }
    return r;
  }
  case 'A': is_str = true;  return perm_string(s.mode);
  case 'F': is_str = true;  return file_type_string(s.mode, s.size);
  case 'a': is_str = false; return to_octal(static_cast<unsigned long>(s.mode & PERMISSION_MASK));
  case 'b': is_str = false; return std::to_string(static_cast<unsigned long long>(s.blocks));
  case 'B': is_str = false; return "512";
  case 'd': is_str = false; return std::to_string(static_cast<unsigned long>(s.dev));
  case 'D': is_str = false; return to_hex(static_cast<unsigned long long>(s.dev));
  case 'f': is_str = false; return to_hex(static_cast<unsigned long>(s.mode));
  case 'h': is_str = false; return std::to_string(static_cast<unsigned long>(s.nlink));
  case 'i': is_str = false; return std::to_string(static_cast<unsigned long long>(s.ino));
  case 'o': is_str = false; return std::to_string(static_cast<unsigned long>(s.blksize));
  case 's': is_str = false; return std::to_string(static_cast<unsigned long long>(s.size));
  case 't': is_str = false; return to_hex(static_cast<unsigned long>(major(s.rdev)));
  case 'T': is_str = false; return to_hex(static_cast<unsigned long>(minor(s.rdev)));
  case 'u': is_str = false; return std::to_string(static_cast<unsigned long>(s.uid));
  case 'U': {
    is_str = true;
    struct passwd* p = getpwuid(s.uid);
    return p ? std::string(p->pw_name) : std::to_string(static_cast<unsigned long>(s.uid));
  }
  case 'g': is_str = false; return std::to_string(static_cast<unsigned long>(s.gid));
  case 'G': {
    is_str = true;
    struct group* gr = getgrgid(s.gid);
    return gr ? std::string(gr->gr_name) : std::to_string(static_cast<unsigned long>(s.gid));
  }
  case 'm': is_str = true;  return mount_point(ctx.name);
  case 'w': is_str = true;  return s.has_btime ? human_time(s.btime) : std::string("-");
  case 'W': is_str = false; return s.has_btime ? std::to_string(static_cast<long>(s.btime.tv_sec)) : std::string("0");
  case 'x': is_str = true;  return human_time(s.atime);
  case 'X': is_str = false; return std::to_string(static_cast<long>(s.atime.tv_sec));
  case 'y': is_str = true;  return human_time(s.mtime);
  case 'Y': is_str = false; return std::to_string(static_cast<long>(s.mtime.tv_sec));
  case 'z': is_str = true;  return human_time(s.ctime);
  case 'Z': is_str = false; return std::to_string(static_cast<long>(s.ctime.tv_sec));
  default:  is_str = true;  return "";
  }
}

std::string apply_width(const std::string& val, int width, int prec,
                        bool left, bool zero, bool is_str) {
  std::string s = val;
  if (is_str) {
    if (prec > 0 && s.size() > static_cast<size_t>(prec)) {
      s = s.substr(0, static_cast<size_t>(prec));
    }
    if (width > 0) {
      int pad = width - static_cast<int>(s.size());
      if (pad > 0) {
        s = left ? s + std::string(static_cast<size_t>(pad), ' ')
                 : std::string(static_cast<size_t>(pad), ' ') + s;
      }
    }
  } else {
    if (prec > 0 && s.size() < static_cast<size_t>(prec)) {
      s = std::string(static_cast<size_t>(prec - static_cast<int>(s.size())), '0') + s;
    }
    if (width > 0) {
      int pad = width - static_cast<int>(s.size());
      if (pad > 0) {
        if (left) {
          s += std::string(static_cast<size_t>(pad), ' ');
        } else if (zero && prec <= 0) {
          s = std::string(static_cast<size_t>(pad), '0') + s;
        } else {
          s = std::string(static_cast<size_t>(pad), ' ') + s;
        }
      }
    }
  }
  return s;
}

std::string process_escapes(const std::string& s) {
  std::string out;
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char n = s[i + 1];
      switch (n) {
      case 'a': out += '\a'; i += 2; break;
      case 'b': out += '\b'; i += 2; break;
      case 'f': out += '\f'; i += 2; break;
      case 'n': out += '\n'; i += 2; break;
      case 'r': out += '\r'; i += 2; break;
      case 't': out += '\t'; i += 2; break;
      case 'v': out += '\v'; i += 2; break;
      case '\\': out += '\\'; i += 2; break;
      case '"': out += '"'; i += 2; break;
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7': {
        int oct = 0, k = 0;
        while (k < 3 && i + 1 + static_cast<size_t>(k) < s.size() &&
               s[i + 1 + static_cast<size_t>(k)] >= '0' && s[i + 1 + static_cast<size_t>(k)] <= '7') {
          oct = (oct * 8) + (s[i + 1 + static_cast<size_t>(k)] - '0');
          k++;
        }
        out += static_cast<char>(oct);
        i += 1 + static_cast<size_t>(k);
        break;
      }
      case 'x': {
        if (i + 2 < s.size()) {
          int hex = 0, k = 0;
          while (k < 2 && i + 2 + static_cast<size_t>(k) < s.size()) {
            char h = s[i + 2 + static_cast<size_t>(k)];
            int d = 0;
            if (h >= '0' && h <= '9') { d = h - '0'; }
            else if (h >= 'a' && h <= 'f') { d = h - 'a' + 10; }
            else if (h >= 'A' && h <= 'F') { d = h - 'A' + 10; }
            else { break; }
            hex = (hex * 16) + d;
            k++;
          }
          if (k > 0) {
            out += static_cast<char>(hex);
            i += 2 + static_cast<size_t>(k);
            break;
          }
        }
        out += '\\';
        i += 1;
        break;
      }
      default:
        out += '\\';
        i += 1;
        break;
      }
    } else {
      out += s[i];
      i++;
    }
  }
  return out;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::string expand_format(const std::string& fmt, bool fs_mode,
                          const StatCtx& ctx, bool interpret_escapes) {
  std::string out;
  size_t i = 0;
  while (i < fmt.size()) {
    char ch = fmt[i];
    if (ch != '%') {
      out += ch;
      i++;
      continue;
    }
    if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
      out += '%';
      i += 2;
      continue;
    }
    if (i + 1 >= fmt.size()) {
      out += '%';
      i++;
      continue;
    }
    i++;
    bool left = false, zero = false;
    while (i < fmt.size() && (fmt[i] == '-' || fmt[i] == '0')) {
      if (fmt[i] == '-') {
        left = true;
      } else {
        zero = true;
      }
      i++;
    }
    int width = 0;
    while (i < fmt.size() && isdigit(static_cast<unsigned char>(fmt[i]))) {
      width = (width * 10) + (fmt[i] - '0');
      i++;
    }
    int prec = -1;
    if (i < fmt.size() && fmt[i] == '.') {
      i++;
      prec = 0;
      while (i < fmt.size() && isdigit(static_cast<unsigned char>(fmt[i]))) {
        prec = (prec * 10) + (fmt[i] - '0');
        i++;
      }
    }
    if (i >= fmt.size()) {
      break;
    }
    char conv = fmt[i];
    i++;
    bool is_str = false;
    std::string val = conv_value(conv, fs_mode, ctx, is_str);
    out += apply_width(val, width, prec, left, zero, is_str);
  }
  if (interpret_escapes) {
    out = process_escapes(out);
  }
  return out;
}

void emit(const std::string& s) {
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)fwrite(s.data(), 1, s.size(), stdout);
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void stat_command(int argc, char** argv) {
  struct arg_lit* deref_opt =
      arg_lit0("L", "dereference", "follow links");
  struct arg_lit* fs_opt =
      arg_lit0("f", "file-system", "display file system status instead of file status");
  struct arg_lit* terse_opt =
      arg_lit0("t", "terse", "print the information in terse form");
  struct arg_str* format_opt =
      arg_str0("c", "format", "FORMAT", "use the specified FORMAT instead of the default");
  struct arg_str* printf_opt =
      arg_str0(NULL, "printf", "FORMAT", "like --format, but interpret backslash escapes and do not append a newline");
  struct arg_lit* version_opt =
      arg_lit0(NULL, "version", "output version information and exit");
  struct arg_lit* help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file* file_arg =
      arg_filen(NULL, NULL, "FILE", 0, 100, "file or file system to stat");
  struct arg_end* end = arg_end(20);

  void* argtable[] = {deref_opt, fs_opt, terse_opt, format_opt, printf_opt,
                      version_opt, help_opt, file_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
    printf("Display file or file system status.\n");
    printf("\n");
    printf("  -L, --dereference  follow links\n");
    printf("  -f, --file-system  display file system status instead of file status\n");
    printf("  -t, --terse        print the information in terse form\n");
    printf("  -c, --format=FORMAT   use the specified FORMAT instead of the default\n");
    printf("      --printf=FORMAT   like --format, but interpret backslash escapes,\n");
    printf("                         and do not output a trailing newline\n");
    printf("      --version     output version information and exit\n");
    printf("  -h, --help        display this help and exit\n");
    printf("\n");
    printf("The valid format sequences for files (without --file-system):\n");
    printf("  %%a   access rights in octal   %%A   access rights in human readable form\n");
    printf("  %%b   blocks allocated         %%B   size of each block reported by %%b\n");
    printf("  %%d   device number in decimal %%D   device number in hex\n");
    printf("  %%f   raw mode in hex          %%F   file type\n");
    printf("  %%g   group ID                 %%G   group name\n");
    printf("  %%h   number of hard links     %%i   inode number\n");
    printf("  %%n   file name                %%N   quoted file name, with symlink target\n");
    printf("  %%o   optimal I/O transfer size%%s   total size in bytes\n");
    printf("  %%t   major device type in hex %%T   minor device type in hex\n");
    printf("  %%u   user ID                  %%U   user name\n");
    printf("  %%w   time of birth, human     %%W   time of birth, seconds since Epoch\n");
    printf("  %%x   time of last access      %%X   time of last access, seconds\n");
    printf("  %%y   time of last modify      %%Y   time of last modify, seconds\n");
    printf("  %%z   time of last change      %%Z   time of last change, seconds\n");
    printf("  %%m   mount point of the file\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (version_opt->count > 0) {
    printf("stat (modbox) 1.0\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  bool fs_mode = (fs_opt->count > 0);
  bool deref = (deref_opt->count > 0);

  bool use_printf = (printf_opt->count > 0);
  bool use_format = (format_opt->count > 0);
  std::string format;
  bool append_newline = false;
  bool interpret_escapes = false;

  if (use_printf) {
    format = printf_opt->sval[0];
    interpret_escapes = true;
  } else if (use_format) {
    format = format_opt->sval[0];
    append_newline = true;
  } else if (fs_mode) {
    format = terse_opt->count > 0 ? TERSE_FS_FORMAT : DEFAULT_FS_FORMAT;
  } else {
    format = terse_opt->count > 0 ? TERSE_FILE_FORMAT : DEFAULT_FILE_FORMAT;
  }

  if (file_arg->count == 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "stat: missing operand\n");
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "Try 'stat --help' for more information.\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    exit(1);
  }

  int exit_status = 0;
  for (int idx = 0; idx < file_arg->count; idx++) {
    const char* path = file_arg->filename[idx];
    StatCtx ctx;
    ctx.fs_mode = fs_mode;
    ctx.name = path;

    /* Detect symlinks for %N (independent of --dereference). */
    struct stat ls;
    if (lstat(path, &ls) == 0 && S_ISLNK(ls.st_mode)) {
      ctx.is_link = true;
      char buf[4096];
      ssize_t n = readlink(path, buf, sizeof(buf) - 1);
      if (n > 0) {
        buf[n] = '\0';
        ctx.link_target = buf;
      }
    }

    if (fs_mode) {
      ctx.fss = do_stat_fs(path);
      if (!ctx.fss.valid) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "stat: cannot read file system status for '%s': %s\n",
                      path, strerror(errno));
        exit_status = 1;
        continue;
      }
    } else {
      ctx.fst = do_stat_file(path, deref);
      if (!ctx.fst.valid) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "stat: cannot stat '%s': %s\n", path,
                      strerror(errno));
        exit_status = 1;
        continue;
      }
    }

    std::string out = expand_format(format, fs_mode, ctx, interpret_escapes);
    if (append_newline) {
      out += '\n';
    }
    emit(out);
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
  if (exit_status != 0) {
    exit(exit_status);
  }
}

REGISTER_COMMAND("stat", stat_command, "Display file or filesystem status");
