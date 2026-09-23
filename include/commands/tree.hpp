#ifndef TREE_HPP
#define TREE_HPP

#include <string>
#include <vector>

struct TreeOptions {
    std::vector<std::string> paths;
    int show_hidden = 0;          // -a
    int dirs_only = 0;            // -d
    int one_filesystem = 0;       // -x
    int follow_links = 0;         // -l (follow symlinks, cycle-safe)
    int show_date = 0;            // -D
    int dirs_first = 0;           // --dirsfirst
    int files_first = 0;          // --filesfirst
    int classify = 0;             // -F
    int show_size = 0;            // -s (st_size of each entry)
    int show_du = 0;              // --du (directory size summed from contents)
    int human_size = 0;           // -h
    int show_inodes = 0;          // --inodes
    int show_selinux = 0;         // --selinux
    int full_path = 0;            // -f
    int noreport = 0;             // --noreport
    int no_indent = 0;            // -i
    int ascii_charset = 0;        // --charset=ascii
    int max_depth = 0;            // -L N (0 = unlimited)
    int reverse = 0;              // -r
    int sort_mode = 0;            // 0=name 1=version 2=size 3=mtime 4=ctime 5=none
    std::string exclude_pattern;  // -I
    std::string include_pattern;  // -P
};

int tree_command(int argc, char** argv);

#endif
