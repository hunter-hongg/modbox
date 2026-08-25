#include "commands/ausearch.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <regex>
#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

// ── Data structures ──────────────────────────────────────────────────────────

struct AuditRecord {
    std::string type;
    uint64_t epoch = 0;
    uint32_t msec = 0;
    uint32_t serial = 0;
    std::unordered_map<std::string, std::string> fields;
    std::string raw_line;
};

struct AuditEvent {
    uint64_t epoch = 0;
    uint32_t msec = 0;
    uint32_t serial = 0;
    std::vector<AuditRecord> records;
};

// ── Options ──────────────────────────────────────────────────────────────────

enum OutputFormat { OUTPUT_DEFAULT, OUTPUT_RAW, OUTPUT_INTERPRET };

struct AusearchOptions {
    const char* input_file = nullptr;
    bool input_logs = false;

    // Filters
    std::vector<std::string> message_types;
    const char* node = nullptr;
    int64_t uid = -1;
    int64_t uid_eff = -1;
    int64_t uid_all = -1;
    int64_t loginuid = -1;
    int64_t gid = -1;
    int64_t gid_eff = -1;
    int64_t gid_all = -1;
    int64_t pid = -1;
    int64_t ppid = -1;
    const char* comm = nullptr;
    const char* executable = nullptr;
    const char* syscall = nullptr;
    int64_t exit_code = -1;
    const char* arch = nullptr;
    const char* file = nullptr;
    const char* key = nullptr;
    const char* subject = nullptr;
    const char* object = nullptr;
    const char* context = nullptr;
    int64_t start_time = -1;
    int64_t end_time = -1;
    const char* host = nullptr;
    const char* terminal = nullptr;
    const char* success = nullptr;

    // Output
    OutputFormat output_format = OUTPUT_DEFAULT;
    bool line_buffered = false;
    bool just_one = false;
    bool word_match = false;
};

// ── Syscall tables ───────────────────────────────────────────────────────────

static const std::pair<int64_t, const char*> x86_64_syscalls[] = {
    {0, "read"},
    {1, "write"},
    {2, "open"},
    {3, "close"},
    {4, "stat"},
    {5, "fstat"},
    {6, "lstat"},
    {7, "poll"},
    {8, "lseek"},
    {9, "mmap"},
    {10, "mprotect"},
    {11, "munmap"},
    {12, "brk"},
    {13, "rt_sigaction"},
    {14, "rt_sigprocmask"},
    {15, "rt_sigreturn"},
    {16, "ioctl"},
    {17, "pread64"},
    {18, "pwrite64"},
    {19, "readv"},
    {20, "writev"},
    {21, "access"},
    {22, "pipe"},
    {23, "select"},
    {24, "sched_yield"},
    {25, "mremap"},
    {26, "msync"},
    {27, "mincore"},
    {28, "madvise"},
    {29, "shmget"},
    {30, "shmat"},
    {31, "shmctl"},
    {32, "dup"},
    {33, "dup2"},
    {34, "pause"},
    {35, "nanosleep"},
    {36, "getitimer"},
    {37, "alarm"},
    {38, "setitimer"},
    {39, "getpid"},
    {40, "sendfile"},
    {41, "socket"},
    {42, "connect"},
    {43, "accept"},
    {44, "sendto"},
    {45, "recvfrom"},
    {46, "sendmsg"},
    {47, "recvmsg"},
    {48, "shutdown"},
    {49, "bind"},
    {50, "listen"},
    {51, "getsockname"},
    {52, "getpeername"},
    {53, "socketpair"},
    {54, "setsockopt"},
    {55, "getsockopt"},
    {56, "clone"},
    {57, "fork"},
    {58, "vfork"},
    {59, "execve"},
    {60, "exit"},
    {61, "wait4"},
    {62, "kill"},
    {63, "uname"},
    {64, "semget"},
    {65, "semop"},
    {66, "semctl"},
    {67, "shmdt"},
    {68, "msgget"},
    {69, "msgsnd"},
    {70, "msgrcv"},
    {71, "msgctl"},
    {72, "fcntl"},
    {73, "flock"},
    {74, "fsync"},
    {75, "fdatasync"},
    {76, "truncate"},
    {77, "ftruncate"},
    {78, "getdents"},
    {79, "getcwd"},
    {80, "chdir"},
    {81, "fchdir"},
    {82, "rename"},
    {83, "mkdir"},
    {84, "rmdir"},
    {85, "creat"},
    {86, "link"},
    {87, "unlink"},
    {88, "symlink"},
    {89, "readlink"},
    {90, "chmod"},
    {91, "fchmod"},
    {92, "chown"},
    {93, "fchown"},
    {94, "lchown"},
    {95, "umask"},
    {96, "gettimeofday"},
    {97, "getrlimit"},
    {98, "getrusage"},
    {99, "sysinfo"},
    {100, "times"},
    {101, "ptrace"},
    {102, "getuid"},
    {103, "syslog"},
    {104, "getgid"},
    {105, "setuid"},
    {106, "setgid"},
    {107, "geteuid"},
    {108, "getegid"},
    {109, "setpgid"},
    {110, "getppid"},
    {111, "getpgrp"},
    {112, "setsid"},
    {113, "setreuid"},
    {114, "setregid"},
    {115, "getgroups"},
    {116, "setgroups"},
    {117, "setresuid"},
    {118, "getresuid"},
    {119, "setresgid"},
    {120, "getresgid"},
    {121, "getpgid"},
    {122, "setfsuid"},
    {123, "setfsgid"},
    {124, "getsid"},
    {125, "capget"},
    {126, "capset"},
    {127, "rt_sigpending"},
    {128, "rt_sigtimedwait"},
    {129, "rt_sigqueueinfo"},
    {130, "rt_sigsuspend"},
    {131, "sigaltstack"},
    {132, "utime"},
    {133, "mknod"},
    {134, "uselib"},
    {135, "personality"},
    {136, "ustat"},
    {137, "statfs"},
    {138, "fstatfs"},
    {139, "sysfs"},
    {140, "getpriority"},
    {141, "setpriority"},
    {142, "sched_setparam"},
    {143, "sched_getparam"},
    {144, "sched_setscheduler"},
    {145, "sched_getscheduler"},
    {146, "sched_get_priority_max"},
    {147, "sched_get_priority_min"},
    {148, "sched_rr_get_interval"},
    {149, "mlock"},
    {150, "munlock"},
    {151, "mlockall"},
    {152, "munlockall"},
    {153, "vhangup"},
    {154, "modify_ldt"},
    {155, "pivot_root"},
    {156, "_sysctl"},
    {157, "prctl"},
    {158, "arch_prctl"},
    {159, "adjtimex"},
    {160, "setrlimit"},
    {161, "chroot"},
    {162, "sync"},
    {163, "acct"},
    {164, "settimeofday"},
    {165, "mount"},
    {166, "umount2"},
    {167, "swapon"},
    {168, "swapoff"},
    {169, "reboot"},
    {170, "sethostname"},
    {171, "setdomainname"},
    {172, "iopl"},
    {173, "ioperm"},
    {174, "create_module"},
    {175, "init_module"},
    {176, "delete_module"},
    {177, "get_kernel_syms"},
    {178, "query_module"},
    {179, "quotactl"},
    {180, "nfsservctl"},
    {181, "getpmsg"},
    {182, "putpmsg"},
    {183, "afs_syscall"},
    {184, "tuxcall"},
    {185, "security"},
    {186, "gettid"},
    {187, "readahead"},
    {188, "setxattr"},
    {189, "lsetxattr"},
    {190, "fsetxattr"},
    {191, "getxattr"},
    {192, "lgetxattr"},
    {193, "fgetxattr"},
    {194, "listxattr"},
    {195, "llistxattr"},
    {196, "flistxattr"},
    {197, "removexattr"},
    {198, "lremovexattr"},
    {199, "fremovexattr"},
    {200, "tkill"},
    {201, "time"},
    {202, "futex"},
    {203, "sched_setaffinity"},
    {204, "sched_getaffinity"},
    {205, "set_thread_area"},
    {206, "io_setup"},
    {207, "io_destroy"},
    {208, "io_getevents"},
    {209, "io_submit"},
    {210, "io_cancel"},
    {211, "get_thread_area"},
    {212, "lookup_dcookie"},
    {213, "epoll_create"},
    {214, "epoll_ctl_old"},
    {215, "epoll_wait_old"},
    {216, "remap_file_pages"},
    {217, "getdents64"},
    {218, "set_tid_address"},
    {219, "restart_syscall"},
    {220, "semtimedop"},
    {221, "fadvise64"},
    {222, "timer_create"},
    {223, "timer_settime"},
    {224, "timer_gettime"},
    {225, "timer_getoverrun"},
    {226, "timer_delete"},
    {227, "clock_settime"},
    {228, "clock_gettime"},
    {229, "clock_getres"},
    {230, "clock_nanosleep"},
    {231, "exit_group"},
    {232, "epoll_wait"},
    {233, "epoll_ctl"},
    {234, "tgkill"},
    {235, "utimes"},
    {236, "vserver"},
    {237, "mbind"},
    {238, "set_mempolicy"},
    {239, "get_mempolicy"},
    {240, "mq_open"},
    {241, "mq_unlink"},
    {242, "mq_timedsend"},
    {243, "mq_timedreceive"},
    {244, "mq_notify"},
    {245, "mq_getsetattr"},
    {246, "kexec_load"},
    {247, "waitid"},
    {248, "add_key"},
    {249, "request_key"},
    {250, "keyctl"},
    {251, "ioprio_set"},
    {252, "ioprio_get"},
    {253, "inotify_init"},
    {254, "inotify_add_watch"},
    {255, "inotify_rm_watch"},
    {256, "migrate_pages"},
    {257, "openat"},
    {258, "mkdirat"},
    {259, "mknodat"},
    {260, "fchownat"},
    {261, "futimesat"},
    {262, "newfstatat"},
    {263, "unlinkat"},
    {264, "renameat"},
    {265, "linkat"},
    {266, "symlinkat"},
    {267, "readlinkat"},
    {268, "fchmodat"},
    {269, "faccessat"},
    {270, "pselect6"},
    {271, "ppoll"},
    {272, "unshare"},
    {273, "set_robust_list"},
    {274, "get_robust_list"},
    {275, "splice"},
    {276, "tee"},
    {277, "sync_file_range"},
    {278, "vmsplice"},
    {279, "move_pages"},
    {280, "utimensat"},
    {281, "epoll_pwait"},
    {282, "signalfd"},
    {283, "timerfd_create"},
    {284, "eventfd"},
    {285, "fallocate"},
    {286, "timerfd_settime"},
    {287, "timerfd_gettime"},
    {288, "accept4"},
    {289, "signalfd4"},
    {290, "eventfd2"},
    {291, "epoll_create1"},
    {292, "dup3"},
    {293, "pipe2"},
    {294, "inotify_init1"},
    {295, "preadv"},
    {296, "pwritev"},
    {297, "rt_tgsigqueueinfo"},
    {298, "perf_event_open"},
    {299, "recvmmsg"},
    {300, "fanotify_init"},
    {301, "fanotify_mark"},
    {302, "prlimit64"},
    {303, "name_to_handle_at"},
    {304, "open_by_handle_at"},
    {305, "clock_adjtime"},
    {306, "syncfs"},
    {307, "sendmmsg"},
    {308, "setns"},
    {309, "getcpu"},
    {310, "process_vm_readv"},
    {311, "process_vm_writev"},
    {312, "kcmp"},
    {313, "finit_module"},
    {314, "sched_setattr"},
    {315, "sched_getattr"},
    {316, "renameat2"},
    {317, "seccomp"},
    {318, "getrandom"},
    {319, "memfd_create"},
    {320, "kexec_file_load"},
    {321, "bpf"},
    {322, "execveat"},
    {323, "userfaultfd"},
    {324, "membarrier"},
    {325, "mlock2"},
    {326, "copy_file_range"},
    {327, "preadv2"},
    {328, "pwritev2"},
    {329, "pkey_mprotect"},
    {330, "pkey_alloc"},
    {331, "pkey_free"},
    {332, "statx"},
    {333, "io_pgetevents"},
    {334, "rseq"},
    {335, "uretprobe"},
    {336, "uprobe"},
    {424, "pidfd_send_signal"},
    {425, "io_uring_setup"},
    {426, "io_uring_enter"},
    {427, "io_uring_register"},
    {428, "open_tree"},
    {429, "move_mount"},
    {430, "fsopen"},
    {431, "fsconfig"},
    {432, "fsmount"},
    {433, "fspick"},
    {434, "pidfd_open"},
    {435, "clone3"},
    {436, "close_range"},
    {437, "openat2"},
    {438, "pidfd_getfd"},
    {439, "faccessat2"},
    {440, "process_madvise"},
    {441, "epoll_pwait2"},
    {442, "mount_setattr"},
    {443, "quotactl_fd"},
    {444, "landlock_create_ruleset"},
    {445, "landlock_add_rule"},
    {446, "landlock_restrict_self"},
    {447, "memfd_secret"},
    {448, "process_mrelease"},
    {449, "futex_waitv"},
    {450, "set_mempolicy_home_node"},
    {451, "cachestat"},
    {452, "fchmodat2"},
    {453, "map_shadow_stack"},
    {454, "futex_wake"},
    {455, "futex_wait"},
    {456, "futex_requeue"},
    {457, "statmount"},
    {458, "listmount"},
    {459, "lsm_get_self_attr"},
    {460, "lsm_set_self_attr"},
    {461, "lsm_list_modules"},
    {462, "mseal"},
    {463, "setxattrat"},
    {464, "getxattrat"},
    {465, "listxattrat"},
    {466, "removexattrat"},
    {467, "open_tree_attr"},
    {468, "file_getattr"},
    {469, "file_setattr"},
    {470, "listns"},
    {471, "rseq_slice_yield"},
};

static const std::pair<int64_t, const char*> i386_syscalls[] = {
    {0, "restart_syscall"},
    {1, "exit"},
    {2, "fork"},
    {3, "read"},
    {4, "write"},
    {5, "open"},
    {6, "close"},
    {7, "waitpid"},
    {8, "creat"},
    {9, "link"},
    {10, "unlink"},
    {11, "execve"},
    {12, "chdir"},
    {13, "time"},
    {14, "mknod"},
    {15, "chmod"},
    {16, "lchown"},
    {17, "break"},
    {18, "oldstat"},
    {19, "lseek"},
    {20, "getpid"},
    {21, "mount"},
    {22, "umount"},
    {23, "setuid"},
    {24, "getuid"},
    {25, "stime"},
    {26, "ptrace"},
    {27, "alarm"},
    {28, "oldfstat"},
    {29, "pause"},
    {30, "utime"},
    {31, "stty"},
    {32, "gtty"},
    {33, "access"},
    {34, "nice"},
    {35, "ftime"},
    {36, "sync"},
    {37, "kill"},
    {38, "rename"},
    {39, "mkdir"},
    {40, "rmdir"},
    {41, "dup"},
    {42, "pipe"},
    {43, "times"},
    {44, "prof"},
    {45, "brk"},
    {46, "setgid"},
    {47, "getgid"},
    {48, "signal"},
    {49, "geteuid"},
    {50, "getegid"},
    {51, "acct"},
    {52, "umount2"},
    {53, "lock"},
    {54, "ioctl"},
    {55, "fcntl"},
    {56, "mpx"},
    {57, "setpgid"},
    {58, "ulimit"},
    {59, "oldolduname"},
    {60, "umask"},
    {61, "chroot"},
    {62, "ustat"},
    {63, "dup2"},
    {64, "getppid"},
    {65, "getpgrp"},
    {66, "setsid"},
    {67, "sigaction"},
    {68, "sgetmask"},
    {69, "ssetmask"},
    {70, "setreuid"},
    {71, "setregid"},
    {72, "sigsuspend"},
    {73, "sigpending"},
    {74, "sethostname"},
    {75, "setrlimit"},
    {76, "getrlimit"},
    {77, "getrusage"},
    {78, "gettimeofday"},
    {79, "settimeofday"},
    {80, "getgroups"},
    {81, "setgroups"},
    {82, "select"},
    {83, "symlink"},
    {84, "oldlstat"},
    {85, "readlink"},
    {86, "uselib"},
    {87, "swapon"},
    {88, "reboot"},
    {89, "readdir"},
    {90, "mmap"},
    {91, "munmap"},
    {92, "truncate"},
    {93, "ftruncate"},
    {94, "fchmod"},
    {95, "fchown"},
    {96, "getpriority"},
    {97, "setpriority"},
    {98, "profil"},
    {99, "statfs"},
    {100, "fstatfs"},
    {101, "ioperm"},
    {102, "socketcall"},
    {103, "syslog"},
    {104, "setitimer"},
    {105, "getitimer"},
    {106, "stat"},
    {107, "lstat"},
    {108, "fstat"},
    {109, "olduname"},
    {110, "iopl"},
    {111, "vhangup"},
    {112, "idle"},
    {113, "vm86old"},
    {114, "wait4"},
    {115, "swapoff"},
    {116, "sysinfo"},
    {117, "ipc"},
    {118, "fsync"},
    {119, "sigreturn"},
    {120, "clone"},
    {121, "setdomainname"},
    {122, "uname"},
    {123, "modify_ldt"},
    {124, "adjtimex"},
    {125, "mprotect"},
    {126, "sigprocmask"},
    {127, "create_module"},
    {128, "init_module"},
    {129, "delete_module"},
    {130, "get_kernel_syms"},
    {131, "quotactl"},
    {132, "getpgid"},
    {133, "fchdir"},
    {134, "bdflush"},
    {135, "sysfs"},
    {136, "personality"},
    {137, "afs_syscall"},
    {138, "setfsuid"},
    {139, "setfsgid"},
    {140, "_llseek"},
    {141, "getdents"},
    {142, "_newselect"},
    {143, "flock"},
    {144, "msync"},
    {145, "readv"},
    {146, "writev"},
    {147, "getsid"},
    {148, "fdatasync"},
    {149, "_sysctl"},
    {150, "mlock"},
    {151, "munlock"},
    {152, "mlockall"},
    {153, "munlockall"},
    {154, "sched_setparam"},
    {155, "sched_getparam"},
    {156, "sched_setscheduler"},
    {157, "sched_getscheduler"},
    {158, "sched_yield"},
    {159, "sched_get_priority_max"},
    {160, "sched_get_priority_min"},
    {161, "sched_rr_get_interval"},
    {162, "nanosleep"},
    {163, "mremap"},
    {164, "setresuid"},
    {165, "getresuid"},
    {166, "vm86"},
    {167, "query_module"},
    {168, "poll"},
    {169, "nfsservctl"},
    {170, "setresgid"},
    {171, "getresgid"},
    {172, "prctl"},
    {173, "rt_sigreturn"},
    {174, "rt_sigaction"},
    {175, "rt_sigprocmask"},
    {176, "rt_sigpending"},
    {177, "rt_sigtimedwait"},
    {178, "rt_sigqueueinfo"},
    {179, "rt_sigsuspend"},
    {180, "pread64"},
    {181, "pwrite64"},
    {182, "chown"},
    {183, "getcwd"},
    {184, "capget"},
    {185, "capset"},
    {186, "sigaltstack"},
    {187, "sendfile"},
    {188, "getpmsg"},
    {189, "putpmsg"},
    {190, "vfork"},
    {191, "ugetrlimit"},
    {192, "mmap2"},
    {193, "truncate64"},
    {194, "ftruncate64"},
    {195, "stat64"},
    {196, "lstat64"},
    {197, "fstat64"},
    {198, "lchown32"},
    {199, "getuid32"},
    {200, "getgid32"},
    {201, "geteuid32"},
    {202, "getegid32"},
    {203, "setreuid32"},
    {204, "setregid32"},
    {205, "getgroups32"},
    {206, "setgroups32"},
    {207, "fchown32"},
    {208, "setresuid32"},
    {209, "getresuid32"},
    {210, "setresgid32"},
    {211, "getresgid32"},
    {212, "chown32"},
    {213, "setuid32"},
    {214, "setgid32"},
    {215, "setfsuid32"},
    {216, "setfsgid32"},
    {217, "pivot_root"},
    {218, "mincore"},
    {219, "madvise"},
    {220, "getdents64"},
    {221, "fcntl64"},
    {224, "gettid"},
    {225, "readahead"},
    {226, "setxattr"},
    {227, "lsetxattr"},
    {228, "fsetxattr"},
    {229, "getxattr"},
    {230, "lgetxattr"},
    {231, "fgetxattr"},
    {232, "listxattr"},
    {233, "llistxattr"},
    {234, "flistxattr"},
    {235, "removexattr"},
    {236, "lremovexattr"},
    {237, "fremovexattr"},
    {238, "tkill"},
    {239, "sendfile64"},
    {240, "futex"},
    {241, "sched_setaffinity"},
    {242, "sched_getaffinity"},
    {243, "set_thread_area"},
    {244, "get_thread_area"},
    {245, "io_setup"},
    {246, "io_destroy"},
    {247, "io_getevents"},
    {248, "io_submit"},
    {249, "io_cancel"},
    {250, "fadvise64"},
    {252, "exit_group"},
    {253, "lookup_dcookie"},
    {254, "epoll_create"},
    {255, "epoll_ctl"},
    {256, "epoll_wait"},
    {257, "remap_file_pages"},
    {258, "set_tid_address"},
    {259, "timer_create"},
    {260, "timer_settime"},
    {261, "timer_gettime"},
    {262, "timer_getoverrun"},
    {263, "timer_delete"},
    {264, "clock_settime"},
    {265, "clock_gettime"},
    {266, "clock_getres"},
    {267, "clock_nanosleep"},
    {268, "statfs64"},
    {269, "fstatfs64"},
    {270, "tgkill"},
    {271, "utimes"},
    {272, "fadvise64_64"},
    {273, "vserver"},
    {274, "mbind"},
    {275, "get_mempolicy"},
    {276, "set_mempolicy"},
    {277, "mq_open"},
    {278, "mq_unlink"},
    {279, "mq_timedsend"},
    {280, "mq_timedreceive"},
    {281, "mq_notify"},
    {282, "mq_getsetattr"},
    {283, "kexec_load"},
    {284, "waitid"},
    {286, "add_key"},
    {287, "request_key"},
    {288, "keyctl"},
    {289, "ioprio_set"},
    {290, "ioprio_get"},
    {291, "inotify_init"},
    {292, "inotify_add_watch"},
    {293, "inotify_rm_watch"},
    {294, "migrate_pages"},
    {295, "openat"},
    {296, "mkdirat"},
    {297, "mknodat"},
    {298, "fchownat"},
    {299, "futimesat"},
    {300, "fstatat64"},
    {301, "unlinkat"},
    {302, "renameat"},
    {303, "linkat"},
    {304, "symlinkat"},
    {305, "readlinkat"},
    {306, "fchmodat"},
    {307, "faccessat"},
    {308, "pselect6"},
    {309, "ppoll"},
    {310, "unshare"},
    {311, "set_robust_list"},
    {312, "get_robust_list"},
    {313, "splice"},
    {314, "sync_file_range"},
    {315, "tee"},
    {316, "vmsplice"},
    {317, "move_pages"},
    {318, "getcpu"},
    {319, "epoll_pwait"},
    {320, "utimensat"},
    {321, "signalfd"},
    {322, "timerfd_create"},
    {323, "eventfd"},
    {324, "fallocate"},
    {325, "timerfd_settime"},
    {326, "timerfd_gettime"},
    {327, "signalfd4"},
    {328, "eventfd2"},
    {329, "epoll_create1"},
    {330, "dup3"},
    {331, "pipe2"},
    {332, "inotify_init1"},
    {333, "preadv"},
    {334, "pwritev"},
    {335, "rt_tgsigqueueinfo"},
    {336, "perf_event_open"},
    {337, "recvmmsg"},
    {338, "fanotify_init"},
    {339, "fanotify_mark"},
    {340, "prlimit64"},
    {341, "name_to_handle_at"},
    {342, "open_by_handle_at"},
    {343, "clock_adjtime"},
    {344, "syncfs"},
    {345, "sendmmsg"},
    {346, "setns"},
    {347, "process_vm_readv"},
    {348, "process_vm_writev"},
    {349, "kcmp"},
    {350, "finit_module"},
    {351, "sched_setattr"},
    {352, "sched_getattr"},
    {353, "renameat2"},
    {354, "seccomp"},
    {355, "getrandom"},
    {356, "memfd_create"},
    {357, "bpf"},
    {358, "execveat"},
    {359, "socket"},
    {360, "socketpair"},
    {361, "bind"},
    {362, "connect"},
    {363, "listen"},
    {364, "accept4"},
    {365, "getsockopt"},
    {366, "setsockopt"},
    {367, "getsockname"},
    {368, "getpeername"},
    {369, "sendto"},
    {370, "sendmsg"},
    {371, "recvfrom"},
    {372, "recvmsg"},
    {373, "shutdown"},
    {374, "userfaultfd"},
    {375, "membarrier"},
    {376, "mlock2"},
    {377, "copy_file_range"},
    {378, "preadv2"},
    {379, "pwritev2"},
    {380, "pkey_mprotect"},
    {381, "pkey_alloc"},
    {382, "pkey_free"},
    {383, "statx"},
    {384, "arch_prctl"},
    {385, "io_pgetevents"},
    {386, "rseq"},
    {393, "semget"},
    {394, "semctl"},
    {395, "shmget"},
    {396, "shmctl"},
    {397, "shmat"},
    {398, "shmdt"},
    {399, "msgget"},
    {400, "msgsnd"},
    {401, "msgrcv"},
    {402, "msgctl"},
    {403, "clock_gettime64"},
    {404, "clock_settime64"},
    {405, "clock_adjtime64"},
    {406, "clock_getres_time64"},
    {407, "clock_nanosleep_time64"},
    {408, "timer_gettime64"},
    {409, "timer_settime64"},
    {410, "timerfd_gettime64"},
    {411, "timerfd_settime64"},
    {412, "utimensat_time64"},
    {413, "pselect6_time64"},
    {414, "ppoll_time64"},
    {416, "io_pgetevents_time64"},
    {417, "recvmmsg_time64"},
    {418, "mq_timedsend_time64"},
    {419, "mq_timedreceive_time64"},
    {420, "semtimedop_time64"},
    {421, "rt_sigtimedwait_time64"},
    {422, "futex_time64"},
    {423, "sched_rr_get_interval_time64"},
    {424, "pidfd_send_signal"},
    {425, "io_uring_setup"},
    {426, "io_uring_enter"},
    {427, "io_uring_register"},
    {428, "open_tree"},
    {429, "move_mount"},
    {430, "fsopen"},
    {431, "fsconfig"},
    {432, "fsmount"},
    {433, "fspick"},
    {434, "pidfd_open"},
    {435, "clone3"},
    {436, "close_range"},
    {437, "openat2"},
    {438, "pidfd_getfd"},
    {439, "faccessat2"},
    {440, "process_madvise"},
    {441, "epoll_pwait2"},
    {442, "mount_setattr"},
    {443, "quotactl_fd"},
    {444, "landlock_create_ruleset"},
    {445, "landlock_add_rule"},
    {446, "landlock_restrict_self"},
    {447, "memfd_secret"},
    {448, "process_mrelease"},
    {449, "futex_waitv"},
    {450, "set_mempolicy_home_node"},
    {451, "cachestat"},
    {452, "fchmodat2"},
    {453, "map_shadow_stack"},
    {454, "futex_wake"},
    {455, "futex_wait"},
    {456, "futex_requeue"},
    {457, "statmount"},
    {458, "listmount"},
    {459, "lsm_get_self_attr"},
    {460, "lsm_set_self_attr"},
    {461, "lsm_list_modules"},
    {462, "mseal"},
    {463, "setxattrat"},
    {464, "getxattrat"},
    {465, "listxattrat"},
    {466, "removexattrat"},
    {467, "open_tree_attr"},
    {468, "file_getattr"},
    {469, "file_setattr"},
    {470, "listns"},
    {471, "rseq_slice_yield"},
};

static const char* lookup_syscall(int64_t num, const std::string& arch) {
    const auto* table = x86_64_syscalls;
    size_t n = sizeof(x86_64_syscalls) / sizeof(x86_64_syscalls[0]);
    if (arch == "i386" || arch == "0x40" || arch == "c000003b" || arch == "0xc000003b") {
        table = i386_syscalls;
        n = sizeof(i386_syscalls) / sizeof(i386_syscalls[0]);
    }
    for (size_t i = 0; i < n; ++i) {
        if (table[i].first == num) return table[i].second;
    }
    return nullptr;
}

static std::string resolve_syscall(const std::string& val, const std::string& arch) {
    char* end = nullptr;
    long num = std::strtol(val.c_str(), &end, 0);
    if (end != val.c_str() && *end == '\0') {
        const char* name = lookup_syscall(num, arch);
        if (name) return name;
    }
    return val;
}

static std::string resolve_uid(uint32_t uid) {
    const struct passwd* pw = getpwuid(uid);
    if (pw) return std::string(pw->pw_name);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u", uid);
    return std::string(buf);
}

static std::string resolve_gid(uint32_t gid) {
    const struct group* gr = getgrgid(gid);
    if (gr) return std::string(gr->gr_name);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u", gid);
    return std::string(buf);
}

static std::string resolve_uid_str(const std::string& val) {
    char* end = nullptr;
    long num = std::strtol(val.c_str(), &end, 10);
    if (end != val.c_str() && *end == '\0') {
        return resolve_uid(static_cast<uint32_t>(num));
    }
    return val;
}

static std::string resolve_gid_str(const std::string& val) {
    char* end = nullptr;
    long num = std::strtol(val.c_str(), &end, 10);
    if (end != val.c_str() && *end == '\0') {
        return resolve_gid(static_cast<uint32_t>(num));
    }
    return val;
}

// Filter values accept a number or a name resolved via getpwnam/getgrnam.
static bool parse_uid_value(const std::string& val, int64_t* out) {
    char* end = nullptr;
    long long num = std::strtoll(val.c_str(), &end, 10);
    if (end != val.c_str() && *end == '\0') {
        *out = num;
        return true;
    }
    const struct passwd* pw = getpwnam(val.c_str());
    if (pw) {
        *out = static_cast<int64_t>(pw->pw_uid);
        return true;
    }
    return false;
}

static bool parse_gid_value(const std::string& val, int64_t* out) {
    char* end = nullptr;
    long long num = std::strtoll(val.c_str(), &end, 10);
    if (end != val.c_str() && *end == '\0') {
        *out = num;
        return true;
    }
    const struct group* gr = getgrnam(val.c_str());
    if (gr) {
        *out = static_cast<int64_t>(gr->gr_gid);
        return true;
    }
    return false;
}

// ── Help text ────────────────────────────────────────────────────────────────

static const char* AUSEARCH_HELP =
    "Usage: ausearch [options]\n"
    "\n"
    "Input sources:\n"
    "  -i FILE, --input=FILE        Read from file\n"
    "      --input-logs             Force using auditd log locations\n"
    "\n"
    "Event identification:\n"
    "  -a NUM, --event=NUM          Search by event ID (requires libaudit)\n"
    "  -m TYPE, --message=TYPE      Search by message type (no arg: list types)\n"
    "  -n NAME, --node=NAME         Search by node name\n"
    "\n"
    "User/Process filters:\n"
    "      --uid=NUM                Real UID (number or name)\n"
    "      --uid-effective=NUM      Effective UID (number or name)\n"
    "      --uid-all=NUM            Match NUM in any UID field\n"
    "      --loginuid=NUM           Login UID (number or name)\n"
    "      --gid=NUM                Real GID (number or name)\n"
    "      --gid-effective=NUM      Effective GID (number or name)\n"
    "      --gid-all=NUM            Match NUM in any GID field\n"
    "  -p NUM, --pid=NUM            Process ID\n"
    "      --ppid=NUM               Parent PID\n"
    "  -c NAME, --comm=NAME         Command name\n"
    "  -x PATH, --executable=PATH   Executable path\n"
    "\n"
    "Syscall filters:\n"
    "      --syscall=NAME           Syscall name or number\n"
    "  -e CODE, --exit=CODE         Exit code\n"
    "      --arch=ARCH              Architecture (hex, e.g. c000003e)\n"
    "\n"
    "Path/Key filters:\n"
    "  -f PATH, --file=PATH         Filename\n"
    "  -k STRING, --key=STRING      Audit rule key\n"
    "\n"
    "SELinux filters:\n"
    "      --subject=CTX            Subject context\n"
    "  -o CTX, --object=CTX         Object context\n"
    "      --context=CTX            Either subject or object\n"
    "\n"
    "Time filters:\n"
    "      --start=TIME             Start time\n"
    "      --end=TIME               End time\n"
    "\n"
    "Output:\n"
    "      --interpret              Interpret mode (numeric to text)\n"
    "  -r, --raw                    Raw output\n"
    "      --format=FORMAT          Output format (default/interpret/raw)\n"
    "  -l, --line-buffered          Line buffered output\n"
    "      --just-one               Stop after first match\n"
    "      --word                   Whole-word matching\n"
    "\n"
    "Other:\n"
    "      --host=NAME              Hostname\n"
    "      --terminal=TTY           Terminal\n"
    "      --success=FLAG           Success filter (yes/no)\n"
    "  -v, --version                Version\n"
    "  -h, --help                   Help\n"
    ;

static const char* MESSAGE_TYPES =
    "Valid message types:\n"
    "  ALL        All message types\n"
    "  SYSCALL    System call records\n"
    "  AVC        Access vector cache entries\n"
    "  USER_AVC   User-space AVC entries\n"
    "  SELINUXERR SELinux error records\n"
    "  LOGIN      Login/logout records\n"
    "  LOG_RELABEL Relabeling records\n"
    "  USER_CMD   User commands\n"
    "  USER_AUTH  User authentication\n"
    "  USERacct   User account\n"
    "  ANOM_ABEND Application abnormal end\n"
    "  ANOM_LOGIN Authentication anomaly\n"
    "  ANOM_PROMISCUOUS Promiscuous mode\n"
    "  ANOM_RBDC  Remote bond change\n"
    "  CRYPTO     Cryptographic operations\n"
    "  KERNEL     Kernel messages\n"
    "  USER_START User session start\n"
    "  USER_END   User session end\n"
    "  SU_START   Su session start\n"
    "  SU_END     Su session end\n"
    "  POLICYLOAD Policy load\n"
    "  POLICYDELC Policy delete\n"
;

// ── Parsing ──────────────────────────────────────────────────────────────────

static bool parse_msg_stamp(const std::string& line, uint64_t& epoch, uint32_t& msec,
                            uint32_t& serial) {
    std::string key = "msg=audit(";
    size_t pos = line.find(key);
    if (pos == std::string::npos) {
        epoch = 0;
        msec = 0;
        serial = 0;
        return false;
    }
    pos += key.size();
    size_t close = line.find(')', pos);
    if (close == std::string::npos) return false;
    std::string stamp = line.substr(pos, close - pos);
    size_t dot = stamp.find('.');
    size_t colon = stamp.find(':');
    if (dot == std::string::npos || colon == std::string::npos || dot > colon) {
        return false;
    }
    char* end1 = nullptr;
    char* end2 = nullptr;
    epoch = std::strtoull(stamp.c_str(), &end1, 10);
    msec = static_cast<uint32_t>(std::strtoul(stamp.c_str() + dot + 1, &end2, 10));
    serial = static_cast<uint32_t>(std::strtoul(stamp.c_str() + colon + 1, nullptr, 10));
    return true;
}

static void parse_record_fields(const std::string& line, AuditRecord& rec) {
    // Find the start of msg=audit(
    size_t msg_start = line.find("msg=audit(");
    if (msg_start == std::string::npos) return;

    // Find the matching closing parenthesis of msg=audit(...)
    size_t paren_depth = 0;
    size_t msg_end = msg_start + 10;
    for (size_t i = msg_start + 10; i < line.size(); ++i) {
        if (line[i] == '(') {
            ++paren_depth;
        } else if (line[i] == ')') {
            if (paren_depth == 0) {
                msg_end = i;
                break;
            }
            --paren_depth;
        }
    }
    if (msg_end == msg_start + 10) return;

    // Find the colon after the closing parenthesis
    size_t colon_pos = line.find(':', msg_end);
    if (colon_pos == std::string::npos) return;

    // Extract type from before msg=
    size_t type_start = 0;
    size_t type_end = line.find(' ', type_start);
    if (type_end != std::string::npos && type_end < msg_start) {
        std::string type_part = line.substr(type_start, type_end - type_start);
        size_t eq = type_part.find('=');
        if (eq != std::string::npos && type_part.substr(0, eq) == "type") {
            rec.type = type_part.substr(eq + 1);
        }
    }

    // Parse key=value pairs after the colon. Free-form text (e.g. the
    // "avc:  denied  { read } for " prefix in AVC records) is not a field;
    // a valid key contains no spaces, otherwise resync one character ahead.
    size_t pos = colon_pos + 1;
    while (pos < line.size()) {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        if (pos >= line.size()) break;

        size_t eq_pos = line.find('=', pos);
        if (eq_pos == std::string::npos) break;

        std::string key = line.substr(pos, eq_pos - pos);
        if (key.empty() || key.find(' ') != std::string::npos) {
            ++pos;
            continue;
        }
        ++eq_pos;

        std::string value;
        if (eq_pos < line.size() && (line[eq_pos] == '"' || line[eq_pos] == '\'')) {
            char quote = line[eq_pos];
            size_t open_pos = eq_pos;
            ++eq_pos;
            size_t val_end = eq_pos;
            while (val_end < line.size() && line[val_end] != quote) {
                if (quote == '"' && line[val_end] == '\\' && val_end + 1 < line.size()) {
                    val_end += 2;
                } else {
                    ++val_end;
                }
            }
            // Store the raw value including its quotes so output stays faithful.
            value = line.substr(open_pos, val_end - open_pos + 1);
            pos = val_end + 1;
        } else {
            size_t val_end = eq_pos;
            while (val_end < line.size() && line[val_end] != ' ' && line[val_end] != '\t') {
                ++val_end;
            }
            value = line.substr(eq_pos, val_end - eq_pos);
            pos = val_end;
        }
        rec.fields[key] = value;
    }
}

static AuditRecord parse_audit_line(const std::string& line) {
    AuditRecord rec;
    rec.raw_line = line;
    parse_msg_stamp(line, rec.epoch, rec.msec, rec.serial);
    parse_record_fields(line, rec);
    return rec;
}

static void assemble_events(std::vector<std::string>& lines,
                            std::vector<AuditEvent>& events) {
    AuditEvent current;
    bool has_current = false;

    for (const auto& line : lines) {
        if (line.empty()) continue;
        AuditRecord rec = parse_audit_line(line);
        if (rec.epoch == 0 && rec.serial == 0 && rec.type.empty()) continue;

        bool same_stamp = has_current &&
            rec.epoch == current.epoch && rec.serial == current.serial;
        if (!same_stamp) {
            if (has_current) {
                events.push_back(std::move(current));
            }
            current = AuditEvent{};
            current.epoch = rec.epoch;
            current.msec = rec.msec;
            current.serial = rec.serial;
            has_current = true;
        }
        current.records.push_back(rec);
    }
    if (has_current) {
        events.push_back(std::move(current));
    }
}

// ── Time parsing ─────────────────────────────────────────────────────────────

static int parse_time_value(const std::string& val, time_t* result) {
    // Try numeric
    char* end = nullptr;
    long long num = std::strtoll(val.c_str(), &end, 10);
    if (end != val.c_str() && *end == '\0') {
        *result = static_cast<time_t>(num);
        return 0;
    }

    // Special keywords
    time_t now = std::time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    if (val == "now") {
        *result = now;
        return 0;
    }
    if (val == "recent") {
        *result = now - 600; // 10 minutes
        return 0;
    }
    if (val == "today") {
        tm_now.tm_hour = 0;
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        *result = std::mktime(&tm_now);
        return 0;
    }
    if (val == "yesterday") {
        tm_now.tm_hour = 0;
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        *result = std::mktime(&tm_now) - 86400;
        return 0;
    }
    if (val == "this-hour") {
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        *result = std::mktime(&tm_now);
        return 0;
    }
    if (val == "week-ago") {
        *result = now - 7 * 86400;
        return 0;
    }
    if (val == "this-week") {
        tm_now.tm_hour = 0;
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        // Monday of this week
        int dow = tm_now.tm_wday;
        int days_since_monday = (dow == 0) ? 6 : dow - 1;
        *result = std::mktime(&tm_now) - days_since_monday * 86400;
        return 0;
    }
    if (val == "this-month") {
        tm_now.tm_mday = 1;
        tm_now.tm_hour = 0;
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        *result = std::mktime(&tm_now);
        return 0;
    }
    if (val == "this-year") {
        tm_now.tm_mon = 0;
        tm_now.tm_mday = 1;
        tm_now.tm_hour = 0;
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        *result = std::mktime(&tm_now);
        return 0;
    }
    if (val == "boot") {
        // Try to read from /proc/uptime
        FILE* f = std::fopen("/proc/uptime", "r");
        if (f) {
            double uptime;
            if (std::fscanf(f, "%lf", &uptime) == 1) {
                *result = static_cast<time_t>(now - uptime);
                std::fclose(f);
                return 0;
            }
            std::fclose(f);
        }
        // Fallback: 24 hours ago
        *result = now - 86400;
        return 0;
    }

    // Try strptime
    struct tm tm_val;
    std::memset(&tm_val, 0, sizeof(tm_val));
    if (::strptime(val.c_str(), "%Y-%m-%d %H:%M:%S", &tm_val) != nullptr ||
        ::strptime(val.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_val) != nullptr ||
        ::strptime(val.c_str(), "%x %X", &tm_val) != nullptr) {
        *result = std::mktime(&tm_val);
        return 0;
    }

    return -1;
}

// ── Format helpers ───────────────────────────────────────────────────────────

static std::string format_timestamp(uint64_t epoch) {
    time_t t = static_cast<time_t>(epoch);
    struct tm tm_val;
    localtime_r(&t, &tm_val);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm_val);
    return std::string(buf);
}

// ── Filters ──────────────────────────────────────────────────────────────────

static std::string escape_regex(const std::string& s) {
    std::string escaped;
    for (char c : s) {
        if (c == '\\' || c == '.' || c == '*' || c == '+' || c == '?' ||
            c == '^' || c == '$' || c == '[' || c == ']' || c == '(' ||
            c == ')' || c == '{' || c == '}' || c == '|' || c == '-') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}


static const std::string* field(const AuditRecord& rec, const char* key) {
    auto it = rec.fields.find(key);
    return it == rec.fields.end() ? nullptr : &it->second;
}

// Field values are stored with their original quotes (see parse_record_fields);
// filters must compare the unquoted value.
static std::string unquote(const std::string& v) {
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
        return v.substr(1, v.size() - 2);
    }
    if (v.size() >= 2 && v.front() == '\'' && v.back() == '\'') {
        return v.substr(1, v.size() - 2);
    }
    return v;
}

static bool is_uint(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

static bool is_known_message_type(const std::string& t) {
    if (t == "ALL") return true;
    static const std::unordered_set<std::string> known = {
        "AVC", "AVC_PATH", "CIPSOV4_IN", "CIPSOV4_MAP", "CIPSOV4_OUT", "CRYPTO",
        "CRED_ACQ", "CRED_DISP", "EXECVE", "IPC", "KERNEL", "LOGIN", "MAC_POLICYLOAD",
        "MAC_SYSCALL", "MEM", "MOUNT", "NEXUS", "PATH", "POLICY_LOAD", "POLICY_QUERY",
        "POLICY_DELETE", "PROCTITLE", "SECCOMP", "SELINUX_ERR", "SYSCALL", "TRAP",
        "USER_AVC", "USER_CMD", "USER_CRED", "USER_START", "USER_END", "USER_MAX",
        "USER_SETAF", "USER_SEATTY", "USER_CHAUTHTOK", "USER_CONFIG_CHANGE",
        "USER_SSH", "USER_TTY", "USER_USERMK", "USER_USETOS", "USER_USYS",
        "USER_KERNEL", "USER_ERR", "USER_USERNET", "USER_SYSCALL", "USER_LOGIN",
        "USER_LOGOUT", "USER_NVS", "USER_AVX", "USER_PAX", "USER_CIL",
        "USER_XATTR", "USER_XPAX", "ANOM_ABEND", "ANOM_PROMISCUOUS", "ANOM_LINK",
        "ANOM_LINKINDOMAIN", "ANOM_NICE", "ANOM_NICE_ROOT", "ANOM_NOPKT", "ANOM_HBA",
        "ANOM_CRYPTO", "ANOM_MMAP", "ANOM_MEM", "ANOM_FORK", "ANOM_CONN",
        "ANOM_CONNSTATE", "ANOM_NSHOOK", "ANOM_NODENY", "ANOM_NOSHARE", "ANOM_NOSUID",
        "ANOM_NOSUID_ROOT", "ANOM_NOSUID_NONROOT", "ANOM_NOSUID_SETUID",
        "ANOM_NOSUID_SETGID", "ANOM_NOSUID_SETUID_ROOT", "ANOM_NOSUID_SETGID_ROOT",
        "ANOM_NOSUID_SETUID_NONROOT", "ANOM_NOSUID_SETGID_NONROOT", "ANOM_BADCPU",
        "ANOM_BADHOST",
    };
    return known.count(t) != 0;
}

static bool matches_word(const std::string& haystack, const std::string& needle,
                         bool word_match) {
    if (needle.empty()) return true;
    if (word_match) {
        // Whole word: use word boundaries
        std::string pattern = "(?<![[:alnum:]_])" + escape_regex(needle) + "(?![[:alnum:]_])";
        try {
            std::regex re(pattern, std::regex::ECMAScript);
            return std::regex_search(haystack, re);
        } catch (...) {
            return haystack == needle;
        }
    }
    return haystack.find(needle) != std::string::npos;
}

static bool has_record_filters(const AusearchOptions* o) {
    return !o->message_types.empty() || o->node != nullptr || o->uid >= 0 ||
           o->uid_eff >= 0 || o->uid_all >= 0 || o->loginuid >= 0 || o->gid >= 0 ||
           o->gid_eff >= 0 || o->gid_all >= 0 || o->pid >= 0 || o->ppid >= 0 ||
           o->comm != nullptr || o->executable != nullptr || o->syscall != nullptr ||
           o->exit_code >= 0 || o->arch != nullptr || o->file != nullptr ||
           o->key != nullptr || o->subject != nullptr || o->object != nullptr ||
           o->context != nullptr || o->host != nullptr || o->terminal != nullptr ||
           o->success != nullptr;
}

static bool record_matches(const AuditRecord& rec, const AusearchOptions* opts) {
    if (!opts->message_types.empty()) {
        bool all = false;
        bool found = false;
        for (const auto& mt : opts->message_types) {
            if (mt == "ALL") { all = true; break; }
            if (rec.type == mt) { found = true; break; }
        }
        if (!all && !found) return false;
    }

    if (opts->node) {
        const std::string* v = field(rec, "node");
        if (!v || unquote(*v) != opts->node) return false;
    }

    if (opts->host) {
        bool found = false;
        for (const auto& [k, v] : rec.fields) {
            if ((k == "addr" || k == "host") &&
                matches_word(unquote(v), opts->host, opts->word_match)) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    if (opts->terminal) {
        bool found = false;
        for (const auto& [k, v] : rec.fields) {
            if ((k == "tty" || k == "term") &&
                matches_word(unquote(v), opts->terminal, opts->word_match)) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    if (opts->uid >= 0) {
        const std::string* v = field(rec, "uid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->uid) return false;
    }
    if (opts->uid_eff >= 0) {
        const std::string* v = field(rec, "euid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->uid_eff) return false;
    }
    if (opts->uid_all >= 0) {
        bool found = false;
        for (const char* k : {"uid", "euid", "auid", "suid"}) {
            const std::string* v = field(rec, k);
            if (v && is_uint(unquote(*v)) &&
                std::strtoll(unquote(*v).c_str(), nullptr, 10) == opts->uid_all) {
                found = true; break;
            }
        }
        if (!found) return false;
    }
    if (opts->loginuid >= 0) {
        const std::string* v = field(rec, "auid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->loginuid) return false;
    }

    if (opts->gid >= 0) {
        const std::string* v = field(rec, "gid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->gid) return false;
    }
    if (opts->gid_eff >= 0) {
        const std::string* v = field(rec, "egid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->gid_eff) return false;
    }
    if (opts->gid_all >= 0) {
        bool found = false;
        for (const char* k : {"gid", "egid", "sgid"}) {
            const std::string* v = field(rec, k);
            if (v && is_uint(unquote(*v)) &&
                std::strtoll(unquote(*v).c_str(), nullptr, 10) == opts->gid_all) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    if (opts->pid >= 0) {
        const std::string* v = field(rec, "pid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->pid) return false;
    }
    if (opts->ppid >= 0) {
        const std::string* v = field(rec, "ppid");
        if (!v || !is_uint(unquote(*v)) ||
            std::strtoll(unquote(*v).c_str(), nullptr, 10) != opts->ppid) return false;
    }
    if (opts->comm) {
        const std::string* v = field(rec, "comm");
        if (!v || !matches_word(unquote(*v), opts->comm, opts->word_match)) return false;
    }
    if (opts->executable) {
        const std::string* v = field(rec, "exe");
        if (!v || !matches_word(unquote(*v), opts->executable, opts->word_match)) return false;
    }

    if (opts->syscall) {
        const std::string* v = field(rec, "syscall");
        if (!v) return false;
        std::string raw = unquote(*v);
        const std::string* arch = field(rec, "arch");
        std::string resolved = resolve_syscall(raw, arch ? *arch : "x86_64");
        if (!matches_word(raw, opts->syscall, opts->word_match) &&
            !matches_word(resolved, opts->syscall, opts->word_match)) return false;
    }

    if (opts->exit_code >= 0) {
        const std::string* v = field(rec, "exit");
        if (!v) return false;
        std::string raw = unquote(*v);
        if (!(is_uint(raw) || (!raw.empty() && raw[0] == '-' && is_uint(raw.substr(1))))) {
            return false;
        }
        if (std::strtoll(raw.c_str(), nullptr, 10) != opts->exit_code) return false;
    }

    if (opts->arch) {
        const std::string* v = field(rec, "arch");
        if (!v) return false;
        std::string f = unquote(*v);
        std::string a = opts->arch;
        if (f.size() > 2 && f[0] == '0' && (f[1] == 'x' || f[1] == 'X')) f = f.substr(2);
        if (a.size() > 2 && a[0] == '0' && (a[1] == 'x' || a[1] == 'X')) a = a.substr(2);
        for (auto& c : f) c = std::tolower(c);
        for (auto& c : a) c = std::tolower(c);
        if (f != a) return false;
    }

    if (opts->success) {
        const std::string* v = field(rec, "success");
        if (!v) return false;
        std::string sv = unquote(*v);
        for (auto& c : sv) c = std::tolower(c);
        std::string expected = opts->success;
        for (auto& c : expected) c = std::tolower(c);
        if (sv != expected) return false;
    }

    if (opts->file) {
        bool found = false;
        for (const auto& [k, v] : rec.fields) {
            if (k == "name" && matches_word(unquote(v), opts->file, opts->word_match)) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    if (opts->key) {
        const std::string* v = field(rec, "key");
        if (!v || !matches_word(unquote(*v), opts->key, opts->word_match)) return false;
    }

    if (opts->subject) {
        const std::string* v = field(rec, "scontext");
        if (!v || !matches_word(unquote(*v), opts->subject, opts->word_match)) return false;
    }

    if (opts->object) {
        const std::string* v = field(rec, "tcontext");
        if (!v || !matches_word(unquote(*v), opts->object, opts->word_match)) return false;
    }

    if (opts->context) {
        bool found = false;
        for (const char* k : {"scontext", "tcontext"}) {
            const std::string* v = field(rec, k);
            if (v && matches_word(unquote(*v), opts->context, opts->word_match)) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    return true;
}

static bool event_matches(const AuditEvent& event, const AusearchOptions* opts) {
    if (opts->start_time >= 0 && event.epoch < static_cast<uint64_t>(opts->start_time)) {
        return false;
    }
    if (opts->end_time >= 0 && event.epoch > static_cast<uint64_t>(opts->end_time)) {
        return false;
    }
    if (!has_record_filters(opts)) return true;
    for (const auto& rec : event.records) {
        if (record_matches(rec, opts)) return true;
    }
    return false;
}

// ── Output ───────────────────────────────────────────────────────────────────

static void emit_default_event(const AuditEvent& event, const AusearchOptions*) {
    std::string ts = format_timestamp(event.epoch);
    fprintf(stdout, "---- time->%s\n", ts.c_str());
    for (const auto& rec : event.records) {
        char stamp[64];
        std::snprintf(stamp, sizeof(stamp), "msg=audit(%llu.%03u:%u): ",
                      (unsigned long long)event.epoch, event.msec, event.serial);
        fprintf(stdout, "type=%s %s", rec.type.c_str(), stamp);
        bool first = true;
        for (const auto& [k, v] : rec.fields) {
            if (k == "type") continue;
            if (!first) fprintf(stdout, " ");
            fprintf(stdout, "%s=%s", k.c_str(), v.c_str());
            first = false;
        }
        fprintf(stdout, "\n");
    }
}

static void emit_raw_event(const AuditEvent& event, const AusearchOptions*) {
    for (const auto& rec : event.records) {
        fprintf(stdout, "%s\n", rec.raw_line.c_str());
    }
}

static std::string interpret_field(const std::string& k, const std::string& v,
                                   const AuditRecord& rec) {
    if (!is_uint(v)) return v;
    long long n = std::strtoll(v.c_str(), nullptr, 10);
    if (k == "uid" || k == "euid" || k == "suid" || k == "fsuid" || k == "auid") {
        return resolve_uid(static_cast<uint32_t>(n));
    }
    if (k == "gid" || k == "egid" || k == "sgid" || k == "fsgid") {
        return resolve_gid(static_cast<uint32_t>(n));
    }
    if (k == "syscall") {
        const std::string* arch = field(rec, "arch");
        const char* name = lookup_syscall(static_cast<int64_t>(n),
                                          arch ? *arch : std::string("x86_64"));
        if (name) return name;
    }
    return v;
}

static void emit_interpret_event(const AuditEvent& event, const AusearchOptions*) {
    std::string ts = format_timestamp(event.epoch);
    fprintf(stdout, "---- time->%s\n", ts.c_str());
    for (const auto& rec : event.records) {
        char stamp[64];
        std::snprintf(stamp, sizeof(stamp), "msg=audit(%llu.%03u:%u): ",
                      (unsigned long long)event.epoch, event.msec, event.serial);
        fprintf(stdout, "type=%s %s", rec.type.c_str(), stamp);
        bool first = true;
        for (const auto& [k, v] : rec.fields) {
            if (k == "type") continue;
            if (!first) fprintf(stdout, " ");
            fprintf(stdout, "%s=%s", k.c_str(), interpret_field(k, v, rec).c_str());
            first = false;
        }
        fprintf(stdout, "\n");
    }
}

static void flush_output(const AusearchOptions* opts) {
    if (opts->line_buffered) {
        std::fflush(stdout);
    }
}

// ── Input reading ────────────────────────────────────────────────────────────

static std::vector<std::string> read_input(const char* input_path) {
    std::vector<std::string> lines;
    if (input_path) {
        std::ifstream file(input_path);
        if (!file.is_open()) {
            fprintf(stderr, "ausearch: cannot open '%s': %s\n",
                    input_path, strerror(errno));
            return lines;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) lines.push_back(line);
        }
    } else {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty()) lines.push_back(line);
        }
    }
    return lines;
}

// ── Main command ─────────────────────────────────────────────────────────────

int ausearch_command(int argc, char** argv) {
    // Input source options
    struct arg_file* input_opt = arg_file0("i", "input", "FILE", "read from file");
    struct arg_lit* input_logs_opt = arg_lit0(NULL, "input-logs", "force auditd log locations");

    // Event identification options
    struct arg_int* event_opt = arg_int0("a", "event", "NUM", "search by event ID");
    struct arg_lit* boot_opt = arg_lit0("b", "boot", "search since last boot (requires libaudit)");
    struct arg_lit* lastreload_opt = arg_lit0(NULL, "lastreload", "search since last reload (requires libaudit)");
    struct arg_str* message_opt = arg_strn("m", "message", "TYPE", 0, 100, "search by message type");
    struct arg_str* node_opt = arg_str0("n", "node", "NAME", "search by node name");

    // User/Process filter options (long options only: argtable3 has no
    // multi-character short options)
struct arg_str* uid_opt = arg_str0(NULL, "uid", "NUM", "real UID (number or name)");
struct arg_str* uid_eff_opt = arg_str0(NULL, "uid-effective", "NUM", "effective UID (number or name)");
struct arg_int* uid_all_opt = arg_int0(NULL, "uid-all", "NUM", "match NUM in any UID field");
struct arg_str* loginuid_opt = arg_str0(NULL, "loginuid", "NUM", "login UID (number or name)");
struct arg_str* gid_opt = arg_str0(NULL, "gid", "NUM", "real GID (number or name)");
struct arg_str* gid_eff_opt = arg_str0(NULL, "gid-effective", "NUM", "effective GID (number or name)");
struct arg_int* gid_all_opt = arg_int0(NULL, "gid-all", "NUM", "match NUM in any GID field");
    struct arg_int* pid_opt = arg_int0("p", "pid", "NUM", "process ID");
    struct arg_int* ppid_opt = arg_int0(NULL, "ppid", "NUM", "parent PID");
    struct arg_str* comm_opt = arg_str0("c", "comm", "NAME", "command name");
    struct arg_str* executable_opt = arg_str0("x", "executable", "PATH", "executable path");

    // Syscall filter options
    struct arg_str* syscall_opt = arg_str0(NULL, "syscall", "NAME", "syscall name or number");
    struct arg_int* exit_opt = arg_int0("e", "exit", "CODE", "exit code");
    struct arg_str* arch_opt = arg_str0(NULL, "arch", "ARCH", "architecture (hex, e.g. c000003e)");

    // Path/Key filter options
    struct arg_str* file_opt = arg_str0("f", "file", "PATH", "filename");
    struct arg_str* key_opt = arg_str0("k", "key", "STRING", "audit rule key");

    // SELinux filter options
    struct arg_str* subject_opt = arg_str0(NULL, "subject", "CTX", "subject context");
    struct arg_str* object_opt = arg_str0("o", "object", "CTX", "object context");
    struct arg_str* context_opt = arg_str0(NULL, "context", "CTX", "either subject or object");

    // Time filter options
    struct arg_str* start_time_opt = arg_str0(NULL, "start", "TIME", "start time");
    struct arg_str* end_time_opt = arg_str0(NULL, "end", "TIME", "end time");

    // Output options
    struct arg_lit* interpret_opt = arg_lit0(NULL, "interpret", "interpret mode (numeric to text)");
    struct arg_lit* raw_opt = arg_lit0("r", "raw", "raw output");
    struct arg_str* format_opt = arg_str0(NULL, "format", "FORMAT", "output format (default/interpret/raw)");
    struct arg_lit* line_buffered_opt = arg_lit0("l", "line-buffered", "line buffered output");
    struct arg_lit* just_one_opt = arg_lit0(NULL, "just-one", "stop after first match");
    struct arg_lit* word_opt = arg_lit0(NULL, "word", "whole-word matching");

    // Other options
    struct arg_str* host_opt = arg_str0(NULL, "host", "NAME", "hostname");
    struct arg_str* terminal_opt = arg_str0(NULL, "terminal", "TTY", "terminal");
    struct arg_str* success_opt = arg_str0(NULL, "success", "YES/NO", "success filter");

    // Standard options
    struct arg_lit* version_opt = arg_lit0("v", "version", "version");
    struct arg_lit* help_opt = arg_lit0("h", "help", "help");

    struct arg_end* end = arg_end(20);

    ArgTable at({
        input_opt, input_logs_opt,
        event_opt, boot_opt, lastreload_opt,
        message_opt, node_opt,
        uid_opt, uid_eff_opt, uid_all_opt, loginuid_opt,
        gid_opt, gid_eff_opt, gid_all_opt,
        pid_opt, ppid_opt, comm_opt, executable_opt,
        syscall_opt, exit_opt, arch_opt,
        file_opt, key_opt,
        subject_opt, object_opt, context_opt,
        start_time_opt, end_time_opt,
        interpret_opt, raw_opt, format_opt,
        line_buffered_opt, just_one_opt, word_opt,
        host_opt, terminal_opt, success_opt,
        version_opt, help_opt,
        end
    });

    // -m without an argument lists the valid message types. Pre-scan argv
    // because argtable3 reports an error instead of leaving a missing value.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-m") == 0 &&
            (i + 1 >= argc || argv[i + 1][0] == '-')) {
            printf("%s", MESSAGE_TYPES);
            return 0;
        }
    }

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("%s", AUSEARCH_HELP);
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, "ausearch");
    }

    if (version_opt->count > 0) {
        print_version("ausearch");
        return 0;
    }

    // Validate message types
    for (int i = 0; i < message_opt->count; ++i) {
        const std::string mt = message_opt->sval[i];
        if (!is_known_message_type(mt)) {
            fprintf(stderr, "ausearch: unknown message type '%s'\n", mt.c_str());
            return 1;
        }
    }

    // Build options
    AusearchOptions opts;

    // Unsupported libaudit flags
    if (event_opt->count > 0) {
        fprintf(stderr, "ausearch: libaudit not available\n");
        return 1;
    }
    if (boot_opt->count > 0) {
        fprintf(stderr, "ausearch: libaudit not available\n");
        return 1;
    }
    if (lastreload_opt->count > 0) {
        fprintf(stderr, "ausearch: libaudit not available\n");
        return 1;
    }

    opts.input_file = (input_opt->count > 0) ? input_opt->filename[0] : nullptr;
    opts.input_logs = (input_logs_opt->count > 0);

    for (int i = 0; i < message_opt->count; ++i) {
        opts.message_types.push_back(message_opt->sval[i]);
    }
    opts.node = (node_opt->count > 0) ? node_opt->sval[0] : nullptr;

    if (uid_opt->count > 0 && !parse_uid_value(uid_opt->sval[0], &opts.uid)) {
        fprintf(stderr, "ausearch: cannot resolve uid '%s'\n", uid_opt->sval[0]);
        return 1;
    }
    if (uid_eff_opt->count > 0 && !parse_uid_value(uid_eff_opt->sval[0], &opts.uid_eff)) {
        fprintf(stderr, "ausearch: cannot resolve uid '%s'\n", uid_eff_opt->sval[0]);
        return 1;
    }
    if (uid_all_opt->count > 0) opts.uid_all = uid_all_opt->ival[0];
    if (loginuid_opt->count > 0 && !parse_uid_value(loginuid_opt->sval[0], &opts.loginuid)) {
        fprintf(stderr, "ausearch: cannot resolve loginuid '%s'\n", loginuid_opt->sval[0]);
        return 1;
    }
    if (gid_opt->count > 0 && !parse_gid_value(gid_opt->sval[0], &opts.gid)) {
        fprintf(stderr, "ausearch: cannot resolve gid '%s'\n", gid_opt->sval[0]);
        return 1;
    }
    if (gid_eff_opt->count > 0 && !parse_gid_value(gid_eff_opt->sval[0], &opts.gid_eff)) {
        fprintf(stderr, "ausearch: cannot resolve gid '%s'\n", gid_eff_opt->sval[0]);
        return 1;
    }
    if (gid_all_opt->count > 0) opts.gid_all = gid_all_opt->ival[0];
    if (pid_opt->count > 0) opts.pid = pid_opt->ival[0];
    if (ppid_opt->count > 0) opts.ppid = ppid_opt->ival[0];
    opts.comm = (comm_opt->count > 0) ? comm_opt->sval[0] : nullptr;
    opts.executable = (executable_opt->count > 0) ? executable_opt->sval[0] : nullptr;

    opts.syscall = (syscall_opt->count > 0) ? syscall_opt->sval[0] : nullptr;
    if (exit_opt->count > 0) opts.exit_code = exit_opt->ival[0];
    opts.arch = (arch_opt->count > 0) ? arch_opt->sval[0] : nullptr;

    opts.file = (file_opt->count > 0) ? file_opt->sval[0] : nullptr;
    opts.key = (key_opt->count > 0) ? key_opt->sval[0] : nullptr;

    opts.subject = (subject_opt->count > 0) ? subject_opt->sval[0] : nullptr;
    opts.object = (object_opt->count > 0) ? object_opt->sval[0] : nullptr;
    opts.context = (context_opt->count > 0) ? context_opt->sval[0] : nullptr;

    if (start_time_opt->count > 0) {
        time_t t;
        if (parse_time_value(start_time_opt->sval[0], &t) == 0) {
            opts.start_time = t;
        }
    }
    if (end_time_opt->count > 0) {
        time_t t;
        if (parse_time_value(end_time_opt->sval[0], &t) == 0) {
            opts.end_time = t;
        }
    }

    opts.host = (host_opt->count > 0) ? host_opt->sval[0] : nullptr;
    opts.terminal = (terminal_opt->count > 0) ? terminal_opt->sval[0] : nullptr;
    opts.success = (success_opt->count > 0) ? success_opt->sval[0] : nullptr;

    // Determine output format (--format takes priority)
    if (format_opt->count > 0) {
        std::string fmt = format_opt->sval[0];
        for (auto& c : fmt) c = std::tolower(c);
        if (fmt == "raw") {
            opts.output_format = OUTPUT_RAW;
        } else if (fmt == "interpret") {
            opts.output_format = OUTPUT_INTERPRET;
        } else if (fmt == "default") {
            opts.output_format = OUTPUT_DEFAULT;
        } else if (fmt == "csv" || fmt == "text") {
            fprintf(stderr, "ausearch: --format %s is not supported\n", format_opt->sval[0]);
            return 1;
        } else {
            fprintf(stderr, "ausearch: unknown format '%s'\n", format_opt->sval[0]);
            return 1;
        }
    } else {
        if (interpret_opt->count > 0) {
            opts.output_format = OUTPUT_INTERPRET;
        } else if (raw_opt->count > 0) {
            opts.output_format = OUTPUT_RAW;
        } else {
            opts.output_format = OUTPUT_DEFAULT;
        }
    }

    opts.line_buffered = (line_buffered_opt->count > 0);
    opts.just_one = (just_one_opt->count > 0);
    opts.word_match = (word_opt->count > 0);

    // Check input
    bool has_input = (opts.input_file != nullptr) || opts.input_logs;
    if (!has_input && isatty(STDIN_FILENO)) {
        fprintf(stderr, "ausearch: no input specified\n");
        return 1;
    }

    // Read input
    std::vector<std::string> lines = read_input(opts.input_file);

    // Assemble events
    std::vector<AuditEvent> events;
    assemble_events(lines, events);

    // Filter and output
    for (const auto& event : events) {
        if (!event_matches(event, &opts)) continue;
        switch (opts.output_format) {
            case OUTPUT_DEFAULT:
                emit_default_event(event, &opts);
                break;
            case OUTPUT_RAW:
                emit_raw_event(event, &opts);
                break;
            case OUTPUT_INTERPRET:
                emit_interpret_event(event, &opts);
                break;
        }
        flush_output(&opts);
        if (opts.just_one) break;
    }

    return 0;
}

REGISTER_COMMAND("ausearch", ausearch_command, "Search audit log for events");
