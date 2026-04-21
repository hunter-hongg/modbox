#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <argtable3.h>
#include <pwd.h>
#include <grp.h>

#include "commands/ls.h"

static void print_file_info(const char* filename, int show_details) {
    if (!show_details) {
        printf("%s  ", filename);
        return;
    }

    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("stat");
        return;
    }

    // 权限
    printf("%c%c%c%c%c%c%c%c%c ",
           S_ISDIR(st.st_mode) ? 'd' : '-',
           st.st_mode & S_IRUSR ? 'r' : '-',
           st.st_mode & S_IWUSR ? 'w' : '-',
           st.st_mode & S_IXUSR ? 'x' : '-',
           st.st_mode & S_IRGRP ? 'r' : '-',
           st.st_mode & S_IWGRP ? 'w' : '-',
           st.st_mode & S_IXGRP ? 'x' : '-',
           st.st_mode & S_IROTH ? 'r' : '-',
           st.st_mode & S_IWOTH ? 'w' : '-');

    // 链接数
    printf("%4ld ", (long)st.st_nlink);

    // 所有者和组
    struct passwd* pwd = getpwuid(st.st_uid);
    struct group* grp = getgrgid(st.st_gid);
    printf("%s %s ", pwd ? pwd->pw_name : "-", grp ? grp->gr_name : "-");

    // 文件大小
    printf("%8ld ", (long)st.st_size);

    // 最后修改时间
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&st.st_mtime));
    printf("%s ", time_buf);

    // 文件名
    printf("%s\n", filename);
}

void ls_command(gint argc, gchar** argv) {
    int show_all = 0;
    int show_details = 0;

    struct arg_lit* all_opt = arg_lit0("a", "all", "do not ignore entries starting with .");
    struct arg_lit* long_opt = arg_lit0("l", "long", "use a long listing format");
    struct arg_file* dir_arg = arg_filen(NULL, NULL, "DIR", 0, 100, "directory to list");
    struct arg_end* end = arg_end(20);

    void* argtable[] = { all_opt, long_opt, dir_arg, end };

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    show_all = (all_opt->count > 0);
    show_details = (long_opt->count > 0);

    // 如果没有指定目录，使用当前目录
    if (dir_arg->count == 0) {
        dir_arg->count = 1;
        dir_arg->filename[0] = ".";
    }

    for (int i = 0; i < dir_arg->count; i++) {
        DIR* dir = opendir(dir_arg->filename[i]);
        if (dir == NULL) {
            printf("ls: %s: No such file or directory\n", dir_arg->filename[i]);
            continue;
        }

        struct dirent* entry;
        GList* files = NULL;

        // 读取所有目录项
        while ((entry = readdir(dir)) != NULL) {
            if (!show_all && entry->d_name[0] == '.') {
                continue;
            }
            files = g_list_append(files, g_strdup(entry->d_name));
        }

        // 排序
        files = g_list_sort(files, (GCompareFunc)strcmp);

        // 输出
        GList* iter = files;
        while (iter != NULL) {
            if (show_details) {
                print_file_info((const char*)iter->data, show_details);
            } else {
                print_file_info((const char*)iter->data, show_details);
            }
            g_free(iter->data);
            iter = iter->next;
        }

        if (!show_details) {
            printf("\n");
        }

        g_list_free(files);
        closedir(dir);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
