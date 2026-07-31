#ifndef GETFACL_HPP
#define GETFACL_HPP

#include <sys/stat.h>
#include <unistd.h>

struct GetfaclOptions {
    int is_recursive = 0;
    int is_dereference = 0;      // -H / --dereference
    int is_logical = 0;          // -L / --logical
    int is_physical = 0;         // -P / --physical
    int is_numeric = 0;          // -n / --numeric (print numeric UIDs/GIDs)
    int is_tabular = 0;          // -t / --tabular
    int is_default = 0;          // -d / --default (show only default ACL)
    int is_access = 0;           // -a / --access (show only access ACL)
    int omit_header = 0;         // -c / --omit-header
    int all_effective = 0;       // -e / --all-effective
    int no_effective = 0;        // -E / --no-effective
    int skip_base = 0;           // -s / skip files with only base entries
    int preserve_root = 0;       // --preserve-root
    int absolute_names = 0;      // -p / --absolute-names (don't strip leading '/')
    int one_file_system = 0;     // --one-file-system
};

int getfacl_command(int argc, char **argv);

#endif // GETFACL_HPP
